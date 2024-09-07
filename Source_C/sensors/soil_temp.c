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

// Constants for the means and standard deviations of soil temperature values
#define MEAN_TEMP 25.616244
#define STD_TEMP 5.063749

#define MAX_REGISTRATION_RETRY 5

static int max_registration_retry = MAX_REGISTRATION_RETRY;

// Function to simulate soil temperature data
int soil_temp_simulate()
{
    int temperature = gaussian(MEAN_TEMP, STD_TEMP);

    // Ensure the simulated values are within the range [8.8, 43.7]
    if (temperature < 8.8)
        temperature = 8.8;
    if (temperature > 43.7)
        temperature = 43.7;

    return temperature;
}

// Handler function for GET requests (reading soil temperature)
static void res_get_handler_soil_temp(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    int temperature = soil_temp_simulate();

    int len = snprintf((char *)buffer, preferred_size, "{\"temperature\":%d}",
                       temperature);

    coap_set_header_content_format(response, APPLICATION_JSON);
    coap_set_payload(response, (uint8_t *)buffer, len);
}

// Definition of the resource for the soil temperature sensor
RESOURCE(res_soil_temp,
         "title=\"Soil Temperature\";rt=\"Temperature",
         res_get_handler_soil_temp,
         NULL,
         NULL,
         NULL);

// Handler for the response to the CoAP registration
static void client_chunk_handler(coap_message_t *response) {
    if (response == NULL) {
        printf("Request timed out\n");
        max_registration_retry--;
    } else {
        printf("Registration successful\n");
        max_registration_retry = -1; // Exit the registration loop
    }
}

//coap-client -m POST coap://[fd00::206:6:6:6]:5683/sowing_actuator -e '{"length": 10, "width": 10, "square_size": 1}' -t 50


PROCESS(soil_temp_sensor_server, "Temperature Sensor CoAP Server");
AUTOSTART_PROCESSES(&soil_temp_sensor_server);

PROCESS_THREAD(soil_temp_sensor_server, ev, data)
{
    static coap_endpoint_t main_server_ep;
    static coap_message_t request[1];
    static struct etimer registration_timer;

    PROCESS_BEGIN();

    printf("Soil Temperature Started\n");

    // Activate the resource
    coap_activate_resource(&res_soil_temp, "temperature");

    while (max_registration_retry > 0) {
        /* -------------- REGISTRATION --------------*/
        // Parse the server endpoint and prepare the registration request
        coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &main_server_ep);
        coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);  // Use POST for registration
        coap_set_header_uri_path(request, REGISTER_URL);
        const char msg[] = "temperature";
        coap_set_payload(request, (uint8_t *)msg, sizeof(msg) - 1);

        // Send the registration request and handle the response
        COAP_BLOCKING_REQUEST(&main_server_ep, request, client_chunk_handler);

        /* -------------- END REGISTRATION --------------*/
        if (max_registration_retry > 0) {
            // Wait and retry if registration failed
            etimer_set(&registration_timer, 30 * CLOCK_SECOND);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&registration_timer));
            
            // Decrement retry count if registration failed
            max_registration_retry--;
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
