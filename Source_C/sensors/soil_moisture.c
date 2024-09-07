#include "contiki.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "coap-engine.h"
#include "coap-blocking-api.h"
#include "sys/etimer.h"
#include "gaussian.h"

#include "contiki-net.h"

// mean and standard deviation
#define MEAN_MOISTURE 71
#define STD_MOISTURE 22

#define MIN_MOISTURE 0   
#define MAX_MOISTURE 100 

#define SERVER_EP "coap://[fd00::1]:5683" // server CoAP address
#define REGISTER_URL "/register"          // registration endpoint
#define MAX_REGISTRATION_RETRY 5

static int max_registration_retry = MAX_REGISTRATION_RETRY;

int simulate_soil_moisture()
{
    int moisture = gaussian(MEAN_MOISTURE, STD_MOISTURE);
    
    if (moisture < MIN_MOISTURE) {
        moisture = MIN_MOISTURE;  
    } else if (moisture > MAX_MOISTURE) {
        moisture = MAX_MOISTURE;  
    }
    
    return moisture;
}


static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    int moisture = simulate_soil_moisture();
    coap_set_header_content_format(response, APPLICATION_JSON);
    int payload_len = snprintf((char *)buffer, preferred_size, "{\"moisture\":%d}", moisture);
    coap_set_payload(response, buffer, payload_len);
}

// Defining resource for soil moisture
RESOURCE(res_soil_moisture,
         "title=\"Soil Moisture\";rt=\"moisture",
         res_get_handler,
         NULL,
         NULL,
         NULL);

// Handler for the response to the CoAP registration
static void client_chunk_handler(coap_message_t *response)
{
    if (response == NULL)
    {
        printf("Request timed out\n");
        max_registration_retry--;
    }
    else
    {
        printf("Registration successful\n");
        max_registration_retry = -1; // Exit the registration loop
    }
}

PROCESS(soil_sensor_server, "Moisture Sensor CoAP Server");
AUTOSTART_PROCESSES(&soil_sensor_server);

PROCESS_THREAD(soil_sensor_server, ev, data)
{
    static coap_endpoint_t main_server_ep;
    static coap_message_t request[1];
    static struct etimer registration_timer;

    PROCESS_BEGIN();

    printf("Moisture Sensor Server Started\n");

    // Activate the resource with the right path
    coap_activate_resource(&res_soil_moisture, "moisture");

    while (max_registration_retry > 0)
    {
        /* -------------- REGISTRATION --------------*/
        // Analyze the server endpoint and prepare the registration request
        coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &main_server_ep);
        coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
        coap_set_header_uri_path(request, REGISTER_URL);
        const char msg[] = "moisture";
        coap_set_payload(request, (uint8_t *)msg, sizeof(msg) - 1);

        // send the registratione request and handle the response
        COAP_BLOCKING_REQUEST(&main_server_ep, request, client_chunk_handler);

        // if the registration failed, wait and try again
        if (max_registration_retry > 0)
        {
            etimer_set(&registration_timer, 30 * CLOCK_SECOND);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&registration_timer));
        }
    }

    if (max_registration_retry == 0)
    {
        printf("Registration failed after maximum attempts\n");
    }
    else
    {
        printf("Registration successful\n");
    }

    while (1)
    {
        PROCESS_WAIT_EVENT();
    }

    PROCESS_END();
}
