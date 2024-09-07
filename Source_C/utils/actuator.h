#ifndef ACTUATOR_H
#define ACTUATOR_H

/*
Actuator Workflow

1. Initialization and Registration:
   - The first step is to register with the CoAP server. This involves sending a registration request to the server with the actuator's name and IPV6 address.

2. Discovery of Sensor IPs:
   - After registration, perform a CoAP discover request to find the IP addresses of the sensors: npk, ph, moisture, and temperature.

3. Retrieve Sensor Data and Determine Seeding Type:
   - Once the command is received, perform a CoAP GET request to collect data from the sensors.
   - Use machine learning algorithms to analyze the sensor data and decide which type of seed should be used based on the gathered information.

4. Simulate Seeding Operation:
   - Simulate the seeding process by introducing a delay to represent the time taken for seeding.

5. Update Central CoAP Server:
   - Send a CoAP message to the central server with details of the seeding operation, including the type of seed used, the current row and column, and the sensor values.

6. Restart until the field is completely sowed:
   - Loop from 3.

*/

#include "contiki.h"
#include <stdlib.h>
#include <stdio.h>
#include "coap-engine.h"
#include "coap-blocking-api.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "DT_model.h"

#include "os/dev/leds.h"
#include "os/dev/button-hal.h"

#define NPK_SENSOR 1
#define PH_SENSOR 2
#define MOISTURE_SENSOR 3
#define TEMP_SENSOR 4

#define SERVER_EP "coap://[fd00::1]:5683" // server CoAP address
#define REGISTER_URL "/register"          // registration endpoint 
#define DISCOVER_URL "/discover"          // discovery endpoint
#define SAVE_URL "/save"                  // endpoint to save the data

#define NPK_SENSOR_URL "/npk"
#define PH_SENSOR_URL "/ph"
#define MOISTURE_SENSOR_URL "/moisture"
#define TEMP_SENSOR_URL "/temperature"

#define MAX_REGISTRATION_RETRY 5
#define MAX_IPV6_LENGTH 46 // Maximum length for IPv6 address 45 + 1 of terminator character
#define PAYLOAD_SIZE 256

#define MSG_SIZE 64
#define NUM_MSG_TO_SAVE 5

#define INACTIVE 0
#define ACTIVE 1


typedef struct
{
    int nitrogen;
    int phosphorus;
    int potassium;
} npk;

typedef struct
{
    unsigned int length;
    unsigned int width;
    unsigned int square_size;
    unsigned int current_row;
    unsigned int current_col;
    unsigned int total_rows;
    unsigned int total_cols;
    unsigned int **matrix;
    short int move_complete;
    short int active;
    int direction;
    unsigned int field_id;
} movement_grid_t;

short int is_move_complete();
short int is_movement_active();

void calculate_mat_dimensions();
void allocate_matrix();
void free_matrix();
void setup_movement_info(int length, int width, int square_size, int field_id);
void clear_movement_info();

int apply_decision_tree_model(npk npk_value, int ph, int moisture, int temp);

void update_position();

int parse_sensors_data(const uint8_t *payload);
int evaluate_sensor_type(const char *payload_str);

void start_movement();
void stop_movement();
void set_movement_complete();
void set_movement_uncomplete();

static void sowing_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void sowing_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void sowing_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void sowing_delete_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void status_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buf, uint16_t preferred_size, int32_t *offset);
static void obs_(void);

#endif
