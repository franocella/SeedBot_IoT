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
#define SERVER_EP "coap://[fd00::1]:5683" // server CoAP address
#define REGISTER_URL "/register"          // registration endpoint
#define MAX_REGISTRATION_RETRY 5

// Definition of the structure for npk values
typedef struct {
    int nitrogen;
    int phosphorus;
    int potassium;
} npk;

// Constants for the means and standard deviations of npk values from my dataset
#define MEAN_NITROGEN    50.551818
#define STD_NITROGEN     36.917334
#define MEAN_PHOSPHORUS  53.362727
#define STD_PHOSPHORUS   32.985883
#define MEAN_POTASSIUM   48.149091
#define STD_POTASSIUM    50.647931



// Function to simulate npk values
npk npk_simulate() {
    npk simulated_npk;

    // Generate simulated values
    simulated_npk.nitrogen = gaussian(MEAN_NITROGEN, STD_NITROGEN);
    simulated_npk.phosphorus = gaussian(MEAN_PHOSPHORUS, STD_PHOSPHORUS);
    simulated_npk.potassium = gaussian(MEAN_POTASSIUM, STD_POTASSIUM);

    // Ensure the simulated values are within the range [0, 205]
    simulated_npk.nitrogen = simulated_npk.nitrogen < 0 ? 0 : (simulated_npk.nitrogen > 140 ? 140 : simulated_npk.nitrogen);
    simulated_npk.phosphorus = simulated_npk.phosphorus < 5 ? 5 : (simulated_npk.phosphorus > 145 ? 145 : simulated_npk.phosphorus);
    simulated_npk.potassium = simulated_npk.potassium < 5 ? 5 : (simulated_npk.potassium > 205 ? 205 : simulated_npk.potassium);

    return simulated_npk;
}



static int max_registration_retry = MAX_REGISTRATION_RETRY;

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

// Definition of the resource for the npk sensor
RESOURCE(res_npk_sensor,
         "title=\"npk Sensor\";rt=\"npk",
         res_get_handler,
         NULL,
         NULL,
         NULL);

// Handler function for GET requests (reading npk values)
static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    // Retrieve simulated npk values
    npk simulated_npk = npk_simulate();

    // Construct the response in JSON format
    int len = snprintf((char *)buffer, preferred_size, "{\"n\":%d,\"p\":%d,\"k\":%d}", 
                        simulated_npk.nitrogen, simulated_npk.phosphorus, simulated_npk.potassium);

    // Set the response headers
    coap_set_header_content_format(response, APPLICATION_JSON);
    coap_set_payload(response, (uint8_t *)buffer, len);
}

// Handler for the response to CoAP registration
static void client_chunk_handler(coap_message_t *response) {
    if (response == NULL) {
        printf("Request timed out\n");
        max_registration_retry--;
    } else {
        printf("Registration successful\n");
        max_registration_retry = -1; // Exit the registration loop
    }
}
PROCESS(npk_sensor_server, "npk Sensor CoAP Server");
AUTOSTART_PROCESSES(&npk_sensor_server);

PROCESS_THREAD(npk_sensor_server, ev, data) {
    static coap_endpoint_t main_server_ep;
    static coap_message_t request[1];
    static struct etimer registration_timer;

    PROCESS_BEGIN();

    printf("npk Sensor CoAP Server started\n");

    // Activate the resource
    coap_activate_resource(&res_npk_sensor, "npk");

    while (max_registration_retry > 0) {
        /* -------------- REGISTRATION --------------*/
        // Parse the server endpoint and prepare the registration request
        coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &main_server_ep);
        coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
        coap_set_header_uri_path(request, REGISTER_URL);
        const char msg[] = "npk";
        coap_set_payload(request, (uint8_t *)msg, sizeof(msg) - 1);

        // Send the registration request and handle the response
        COAP_BLOCKING_REQUEST(&main_server_ep, request, client_chunk_handler);

        if (max_registration_retry > 0) {
            // If registration failed, wait and retry
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
