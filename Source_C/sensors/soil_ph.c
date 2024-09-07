#include "contiki.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "coap-engine.h"
#include "coap-blocking-api.h"
#include "sys/etimer.h"
#include "gaussian.h"

// Constants for CoAP registration
#define SERVER_EP "coap://[fd00::1]:5683" // Indirizzo del server CoAP
#define REGISTER_URL "/register"          // Endpoint di registrazione

// Costanti per le medie e le deviazioni standard 
#define MEAN_PH          6.469480
#define STD_PH           1.2


#define MAX_REGISTRATION_RETRY 5

static int max_registration_retry = MAX_REGISTRATION_RETRY;


// Funzione per simulare i dati 
int simulate_soil_ph() {
    int ph = gaussian(MEAN_PH, STD_PH);
    if (ph < 3.5) ph = 3.5;
    if (ph > 10) ph = 10;
    return ph;
}

// Handler per le richieste GET (lettura dei dati del suolo)
static void res_get_handler_soil_ph(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
    int ph = simulate_soil_ph();
    int len = snprintf((char *)buffer, preferred_size, "{\"ph\":%d}", ph);

    coap_set_header_content_format(response, APPLICATION_JSON);
    coap_set_payload(response, buffer, len);
}

// Definizione della risorsa per il sensore di pH e umidità del suolo
RESOURCE(res_soil_ph,
         "title=\"Soil pH\";rt=\"ph",
         res_get_handler_soil_ph,
         NULL,
         NULL,
         NULL);

// Handler per la risposta alla registrazione CoAP
static void client_chunk_handler(coap_message_t *response) {
    if (response == NULL) {
        printf("Request timed out\n");
        max_registration_retry--;
    } else {
        printf("Registration successful\n");
        max_registration_retry = -1; // Exit the registration loop
    }
}

PROCESS(soil_sensor_server, "Ph Sensor CoAP Server");
AUTOSTART_PROCESSES(&soil_sensor_server);

PROCESS_THREAD(soil_sensor_server, ev, data) {
    static coap_endpoint_t main_server_ep;
    static coap_message_t request[1];
    static struct etimer registration_timer;

    PROCESS_BEGIN();

    printf("Ph Sensor CoAP Server Started\n");

    // Attivare la risorsa con il percorso corretto
    coap_activate_resource(&res_soil_ph, "ph");

    while (max_registration_retry > 0) {
        /* -------------- REGISTRAZIONE --------------*/
        // Analizza l'endpoint del server e prepara la richiesta di registrazione
        coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &main_server_ep);
        coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
        coap_set_header_uri_path(request, REGISTER_URL);
        const char msg[] = "ph";
        coap_set_payload(request, (uint8_t *)msg, sizeof(msg) - 1);

        // Invia la richiesta di registrazione e gestisce la risposta
        COAP_BLOCKING_REQUEST(&main_server_ep, request, client_chunk_handler);

        // Se la registrazione è fallita, attendi e riprova
        if (max_registration_retry > 0) {
            etimer_set(&registration_timer, 30 * CLOCK_SECOND);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&registration_timer));
        }
    }

    if (max_registration_retry == 0) {
        printf("Registration failed after maximum attempts\n");
    } else {
        printf("Registration successful\n");
    }

    while (1)
    {
        PROCESS_WAIT_EVENT();
    }

    PROCESS_END();
}
