#include "actuator.h"

PROCESS(device_process, "Device Process");
PROCESS(button_process, "Button Process");
AUTOSTART_PROCESSES(&device_process, &button_process);

static int max_registration_retry = MAX_REGISTRATION_RETRY;

static char npk_sensor_ip[MAX_IPV6_LENGTH];
static char ph_sensor_ip[MAX_IPV6_LENGTH];
static char moisture_sensor_ip[MAX_IPV6_LENGTH];
static char temperature_sensor_ip[MAX_IPV6_LENGTH];

static npk npk_data = {0, 0, 0};
static int ph_data = 0;
static int moisture_data = 0;
static int temperature_data = 0;

// Flag to check the output from the cycle
static short int exit_flag = 0;
static int seed_type = -1;
// Initialize mov_data

static movement_grid_t mov_data = {
    .length = 10,
    .width = 10,
    .square_size = 1,
    .current_row = 0,
    .current_col = 0,
    .total_rows = 10,
    .total_cols = 10,
    .matrix = NULL,
    .direction = 0,
    .field_id = 0};

static short int move_complete = 0;
static short int active = 0;

/*-----------------SOWING RESOURCE-----------------*/

// Definizione della risorsa
RESOURCE(sowing_actuator_resource,
         "title=\"Sowing Actuator\";rt=\"actuator\"",
         sowing_get_handler,
         sowing_post_handler,
         sowing_put_handler,
         sowing_delete_handler);


static void sowing_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
   char data[128]; // Define a buffer to hold the JSON string.

   if (is_movement_active(&mov_data))
   {
      // Format the JSON string using snprintf.
      int len = snprintf(data, sizeof(data), "{\"current_row\": %d, \"current_col\": %d}", mov_data.current_row, mov_data.current_col);

      // Set the content format to JSON.
      coap_set_header_content_format(response, APPLICATION_JSON);

      // Set the response payload.
      coap_set_payload(response, (uint8_t *)data, len);

      // Set the response status code.
      coap_set_status_code(response, CONTENT_2_05); // Content, successful response with payload
   }
   else if (is_move_complete(&mov_data))
   {
      // Set the JSON string for the completed status.
      const char *data = "{\"status\": \"completed\"}";

      // Set the content format to JSON.
      coap_set_header_content_format(response, APPLICATION_JSON);

      // Set the response payload.
      coap_set_payload(response, (uint8_t *)data, strlen(data));

      // Set the response status code.
      coap_set_status_code(response, CONTENT_2_05); // Content, successful response with payload
   }
   else
   {
      // Set the JSON string for the inactive status.
      const char *data = "{\"status\": \"inactive\"}";

      // Set the content format to JSON.
      coap_set_header_content_format(response, APPLICATION_JSON);

      // Set the response payload.
      coap_set_payload(response, (uint8_t *)data, strlen(data));

      // Set the response status code.
      coap_set_status_code(response, CONTENT_2_05); // Content, successful response with payload
   }
}

static void sowing_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
   const uint8_t *payload;
   int payload_len = coap_get_payload(request, &payload);

   // Variables for the movement parameters
   int length = 0, width = 0, square_size = 0, field_id;

   // Default status code for error handling
   coap_status_t response_code = BAD_REQUEST_4_00; // Set to BAD_REQUEST by default

   // Extract payload data if existent
   if (payload_len > 0)
   {
      // Use sscanf to extract length, width, e square_size
      int extracted_values = sscanf((char *)payload, "{\"length\": %d, \"width\": %d, \"square_size\": %d, \"field_id\": %d}", &length, &width, &square_size, &field_id);

      // Verify if the extraction was succesful
      if (extracted_values == 4 && length > 0 && width > 0 && square_size > 0 && field_id > 0)
      {

         mov_data.field_id = field_id;

         // If the movement is inactive, configure movement's parameters and start
         if (!is_movement_active())
         {

            setup_movement_info(length, width, square_size, field_id);

            start_movement();

            response_code = CHANGED_2_04; // Success: Resource modified succesfully
         }
      }
   }

   // Verify the status of the movement and set the response message
   if (!is_move_complete() && is_movement_active())
   {
      // Movement active and not complete
      coap_set_header_content_format(response, TEXT_PLAIN);
      coap_set_payload(response, (uint8_t *)"Sowing process started", strlen("Sowing process started"));
   }
   else if (is_move_complete())
   {
      // Movement complete
      coap_set_header_content_format(response, TEXT_PLAIN);
      coap_set_payload(response, (uint8_t *)"Sowing process already completed", strlen("Sowing process already completed"));
   }
   else if (!is_movement_active())
   {
      // Movement not active
      coap_set_header_content_format(response, TEXT_PLAIN);
      coap_set_payload(response, (uint8_t *)"Movement not active", strlen("Movement not active"));
   }
   coap_set_status_code(response, response_code);
}

static void sowing_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
   const uint8_t *payload;
   int payload_len = coap_get_payload(request, &payload);

   if (payload_len > 0)
   {
      printf("PUT payload: %.*s\n", payload_len, (char *)payload);

      // Comparison of the received command with "start" and "stop"
      if (strncmp((const char *)payload, "start", payload_len) == 0)
      {
         if (!is_move_complete())
         {
            start_movement(); // Start the movement
            coap_set_header_content_format(response, TEXT_PLAIN);
            coap_set_payload(response, (uint8_t *)"Movement started", strlen("Movement started"));
            coap_set_status_code(response, CHANGED_2_04); // Resource modified successfully
         }
         else
         {
            coap_set_header_content_format(response, TEXT_PLAIN);
            coap_set_payload(response, (uint8_t *)"Movement already completed", strlen("Movement already completed"));
            coap_set_status_code(response, VALID_2_03); // Valid operation but no modification
         }
      }
      else if (strncmp((const char *)payload, "stop", payload_len) == 0)
      {
         stop_movement(&mov_data); // Stop the movement
         coap_set_header_content_format(response, TEXT_PLAIN);
         coap_set_payload(response, (uint8_t *)"Movement stopped", strlen("Movement stopped"));
         coap_set_status_code(response, CHANGED_2_04); // Resource modified successfully
      }
      else
      {
         // Command not valid
         coap_set_header_content_format(response, TEXT_PLAIN);
         coap_set_payload(response, (uint8_t *)"Invalid command", strlen("Invalid command"));
         coap_set_status_code(response, BAD_REQUEST_4_00); // Request not valid
      }
   }
   else
   {
      // Missing payload or not valid
      coap_set_header_content_format(response, TEXT_PLAIN);
      coap_set_payload(response, (uint8_t *)"Empty payload", strlen("Empty payload"));
      coap_set_status_code(response, BAD_REQUEST_4_00); // Request not valid
   }
}

static void sowing_delete_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
   printf("DELETE request received.\n");

   // Response to confirme delete
   coap_set_status_code(response, DELETED_2_02); // Resource deleted successfully

   // Clear the data structure of the movement
   clear_movement_info(&mov_data);

   // Set the flag to exit the cycle
   exit_flag = 1;
}



/*--------------------STATUS RESOURCE-----------------*/

// Declare the resource
EVENT_RESOURCE(actuator_status_res,
               "title=\"Actuator Status\";obs",
               status_get_handler,
               NULL, // No POST handler
               NULL, // No PUT handler
               NULL, // No DELETE handler
               obs_);

// Handler for the GET request
static void status_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buf, uint16_t preferred_size, int32_t *offset)
{
   char response_buf[128];
   int len;

   // Create the response string in JSON format
   len = snprintf(response_buf, sizeof(response_buf), "{\"complete\": %d, \"active\": %d}", move_complete, active);

   // Set the payload and response headers
   coap_set_header_content_format(response, APPLICATION_JSON);
   coap_set_payload(response, (uint8_t *)response_buf, len);
}

// Handler for notifications to observers
static void obs_(void)
{  
    coap_message_t notification[1]; // Notification message
    char msg[MSG_SIZE];
    
    // Format the JSON message
    snprintf(msg, sizeof(msg), "{\"complete\": %d, \"active\": %d}", move_complete, active);

    // Initialize the notification message
    coap_init_message(notification, COAP_TYPE_CON, CONTENT_2_05, coap_get_mid());

    // Set the payload of the notification
    coap_set_payload(notification, (uint8_t *)msg, strlen(msg));

    // Notify all observers of the resource
    coap_notify_observers(&actuator_status_res);
}


/*----------------------------------------------------------------*/

static void discovery_response_callback(coap_message_t *response)
{
   if (response == NULL)
   {
      printf("Discovery failed: no response from server.\n");
      return;
   }

   const uint8_t *payload;
   int len = coap_get_payload(response, &payload);
   if (len > 0)
   {
      const char *payload_str = (const char *)payload;

      if (strstr(payload_str, "\"npk\"") != NULL)
      {
         if (sscanf(payload_str, "{\"npk\": \"%45[^\"]\"}", npk_sensor_ip) == 1)
         {
            printf("NPK Sensor IP: %s\n", npk_sensor_ip);
         }
         else
         {
            printf("Failed to parse NPK sensor IP.\n");
         }
      }
      else if (strstr(payload_str, "\"ph\"") != NULL)
      {
         if (sscanf(payload_str, "{\"ph\": \"%45[^\"]\"}", ph_sensor_ip) == 1)
         {
            printf("pH Sensor IP: %s\n", ph_sensor_ip);
         }
         else
         {
            printf("Failed to parse pH sensor IP.\n");
         }
      }
      else if (strstr(payload_str, "\"moisture\"") != NULL)
      {
         if (sscanf(payload_str, "{\"moisture\": \"%45[^\"]\"}", moisture_sensor_ip) == 1)
         {

            printf("Moisture Sensor IP: %s\n", moisture_sensor_ip);
         }
         else
         {
            printf("Failed to parse Moisture sensor IP.\n");
         }
      }
      else if (strstr(payload_str, "\"temperature\"") != NULL)
      {
         if (sscanf(payload_str, "{\"temperature\": \"%45[^\"]\"}", temperature_sensor_ip) == 1)
         {
            printf("Temperature Sensor IP: %s\n", temperature_sensor_ip);
         }
         else
         {
            printf("Failed to parse Temperature sensor IP.\n");
         }
      }
      else
      {
         printf("Unexpected sensor type in response.\n");
      }
   }
   else
   {
      printf("Empty payload in server response.\n");
   }
}

void get_measurement_callback(coap_message_t *response)
{
   if (response == NULL)
   {
      printf("Failed to retrieve data.\n");
      return;
   }

   const uint8_t *payload;
   int len = coap_get_payload(response, &payload);
   if (len <= 0 || payload == NULL)
   {
      printf("No payload received.\n");
      return;
   }

   // Convert payload to a string for easy manipulation
   const char *payload_str = (const char *)payload;

   // Determine sensor type by checking for specific keys
   int sensor_type = evaluate_sensor_type(payload_str);

   // Extract sensor values based on sensor type
   switch (sensor_type)
   {
   case NPK_SENSOR:
   {
      char *n_start = strstr(payload_str, "\"n\"");
      if (n_start)
      {
         npk_data.nitrogen = atof(strchr(n_start, ':') + 1);
      }
      char *p_start = strstr(payload_str, "\"p\"");
      if (p_start)
      {
         npk_data.phosphorus = atof(strchr(p_start, ':') + 1);
      }
      char *k_start = strstr(payload_str, "\"k\"");
      if (k_start)
      {
         npk_data.potassium = atof(strchr(k_start, ':') + 1);
      }
      printf("NPK Data - n: %d, p: %d, k: %d\n",
             npk_data.nitrogen, npk_data.phosphorus, npk_data.potassium);
      break;
   }

   case PH_SENSOR:
   {
      char *ph_start = strstr(payload_str, "\"ph\"");
      if (ph_start)
      {
         ph_data = atof(strchr(ph_start, ':') + 1);
         printf("pH Data - pH: %d\n", ph_data);
      }
      break;
   }

   case MOISTURE_SENSOR:
   {
      char *moisture_start = strstr(payload_str, "\"moisture\"");
      if (moisture_start)
      {
         moisture_data = atof(strchr(moisture_start, ':') + 1);
         printf("Moisture Data - Moisture: %d\n", moisture_data);
      }
      break;
   }

   case TEMP_SENSOR:
   {
      char *temp_start = strstr(payload_str, "\"temperature\"");
      if (temp_start)
      {
         temperature_data = atof(strchr(temp_start, ':') + 1);
         printf("Temperature Data - Temp: %d\n", temperature_data);
      }
      break;
   }

   default:
      printf("Unknown sensor data.\n");
      break;
   }
}
// Callback function for the response to the saving in the DB
void save_response_callback(coap_message_t *response)
{

   if (response == NULL)
   {
      printf("Failed to send data to the DB.\n");
      return;
   }

   const uint8_t *payload = NULL;
   int len = coap_get_payload(response, &payload);
   if (len > 0)
   {
      printf("Data successfully sent to the DB.\n");
      return;
   }
}

// Handler for the responce to CoAP registration
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

const char *sensor_names[] = {"npk", "temperature", "ph", "moisture"};
const char *sensor_urls[] = {NPK_SENSOR_URL, TEMP_SENSOR_URL, PH_SENSOR_URL, MOISTURE_SENSOR_URL};

PROCESS_THREAD(button_process, ev, data)
{
   PROCESS_BEGIN();

   while (!exit_flag)
   {
      // Wait for the button release event
      PROCESS_YIELD();
      if (ev == button_hal_press_event)
      {
         // Check if the event is indeed a button release event
         if (is_movement_active(&mov_data))
         {
            stop_movement(&mov_data);
            printf("Pause request initiated via button\n");
         }
         else
         {
            start_movement(&mov_data);
            printf("Movement request initiated via button\n");
         }
      }
   }

   PROCESS_END();
}

PROCESS_THREAD(device_process, ev, data)
{
   static coap_message_t request;
   static coap_message_t reg_request[1];
   static coap_endpoint_t sensor_ep;
   static coap_endpoint_t server_ep;
   static struct etimer sowing_timer;
   static struct etimer timer;

   PROCESS_BEGIN();
   printf("Starting Actuator\n");

   // Initialize the endpoint of the CoAP server
   coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);

   // Activate the resource
   coap_activate_resource(&sowing_actuator_resource, "sowing_actuator");
   coap_activate_resource(&actuator_status_res, "sowing_actuator/status");

   button_hal_init();

   /* ----------------------------REGISTER-------------------------------*/

   while (max_registration_retry > 0)
   {

      coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);
      coap_init_message(reg_request, COAP_TYPE_CON, COAP_POST, 0);
      coap_set_header_uri_path(reg_request, REGISTER_URL);
      const char msg[] = "sowing_actuator";
      coap_set_payload(reg_request, (uint8_t *)msg, sizeof(msg) - 1);

      // Send the registration request and handle the response
      COAP_BLOCKING_REQUEST(&server_ep, reg_request, client_chunk_handler);

      if (max_registration_retry > 0)
      {
         // Wait and retry if registration failed
         etimer_set(&timer, 15 * CLOCK_SECOND);
         PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

         // Decrement retry count if registration failed
         max_registration_retry--;
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
   // Wait for registration of the other devices to the server
   etimer_set(&timer, 30 * CLOCK_SECOND);
   PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

   /* ----------------------------DISCOVER-------------------------------*/

   coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);
   coap_init_message(&request, COAP_TYPE_CON, COAP_POST, 0);
   coap_set_header_uri_path(&request, DISCOVER_URL);
   coap_set_header_content_format(&request, APPLICATION_JSON);

   const char msg_1[] = "{\"name\": \"npk\"}";
   coap_set_payload(&request, (uint8_t *)msg_1, sizeof(msg_1) - 1);
   COAP_BLOCKING_REQUEST(&server_ep, &request, discovery_response_callback);

   const char msg_2[] = "{\"name\": \"temperature\"}";
   coap_set_payload(&request, (uint8_t *)msg_2, sizeof(msg_2) - 1);
   COAP_BLOCKING_REQUEST(&server_ep, &request, discovery_response_callback);

   const char msg_3[] = "{\"name\": \"ph\"}";
   coap_set_payload(&request, (uint8_t *)msg_3, sizeof(msg_3) - 1);
   COAP_BLOCKING_REQUEST(&server_ep, &request, discovery_response_callback);

   const char msg_4[] = "{\"name\": \"moisture\"}";
   coap_set_payload(&request, (uint8_t *)msg_4, sizeof(msg_4) - 1);
   COAP_BLOCKING_REQUEST(&server_ep, &request, discovery_response_callback);

   /* ------------------------MAIN LOOP-----------------------*/

   printf("Starting main loop\n");

   while (!exit_flag)
   {
      etimer_set(&timer, 30 * CLOCK_SECOND);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

      // Check the status of mov_data structure
      if (is_movement_active(&mov_data) && !is_move_complete(&mov_data))
      {
         char endpoint_uri[64];

         coap_init_message(&request, COAP_TYPE_CON, COAP_GET, 0);

         coap_set_header_uri_path(&request, NPK_SENSOR_URL);
         snprintf(endpoint_uri, sizeof(endpoint_uri), "coap://[%s]:5683", npk_sensor_ip);
         coap_endpoint_parse(endpoint_uri, strlen(endpoint_uri), &sensor_ep);
         COAP_BLOCKING_REQUEST(&sensor_ep, &request, get_measurement_callback);

         // Request to pH sensor
         coap_init_message(&request, COAP_TYPE_CON, COAP_GET, 0);
         coap_set_header_uri_path(&request, PH_SENSOR_URL);
         snprintf(endpoint_uri, sizeof(endpoint_uri), "coap://[%s]:5683", ph_sensor_ip);
         coap_endpoint_parse(endpoint_uri, strlen(endpoint_uri), &sensor_ep);
         COAP_BLOCKING_REQUEST(&sensor_ep, &request, get_measurement_callback);

         // Request to the temperature sensor
         coap_init_message(&request, COAP_TYPE_CON, COAP_GET, 0);
         coap_set_header_uri_path(&request, TEMP_SENSOR_URL);
         snprintf(endpoint_uri, sizeof(endpoint_uri), "coap://[%s]:5683", temperature_sensor_ip);
         coap_endpoint_parse(endpoint_uri, strlen(endpoint_uri), &sensor_ep);
         COAP_BLOCKING_REQUEST(&sensor_ep, &request, get_measurement_callback);

         // Request to moisture sensor
         coap_init_message(&request, COAP_TYPE_CON, COAP_GET, 0);
         coap_set_header_uri_path(&request, MOISTURE_SENSOR_URL);
         snprintf(endpoint_uri, sizeof(endpoint_uri), "coap://[%s]:5683", moisture_sensor_ip);
         coap_endpoint_parse(endpoint_uri, strlen(endpoint_uri), &sensor_ep);
         COAP_BLOCKING_REQUEST(&sensor_ep, &request, get_measurement_callback);
         seed_type = apply_decision_tree_model(npk_data, ph_data, moisture_data, temperature_data);

         printf("Temperature Data - Temp: %d\n", temperature_data);
         printf("Ph Data - Ph: %d\n", ph_data);
         printf("Moisture Data - Moisture: %d\n", moisture_data);
         printf("NPK Data - n: %d, p: %d, k: %d\n",
                npk_data.nitrogen, npk_data.phosphorus, npk_data.potassium);
         printf("Seed type: %d\n", seed_type);

         /*----------------------SEEDING SIMULATION-------------------------*/

         // Set the timer for 20 seconds
         etimer_set(&sowing_timer, CLOCK_SECOND * 20);

         leds_on(LEDS_GREEN);

         printf("Seeding simulation started. Waiting...\n");

         // Wait until the timer expiration
         PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sowing_timer));
         printf("Simulation complete.\n");
         leds_off(LEDS_GREEN);

         /*--------------------------SEND TO DB------------------------------*/

         char payload[MSG_SIZE]; // One variable for the payload

         // Initialization of the timer
         etimer_set(&timer, 10 * CLOCK_SECOND);

         // Initialization of CoAP request
         coap_init_message(&request, COAP_TYPE_CON, COAP_POST, 0);
         coap_set_header_uri_path(&request, SAVE_URL);
         coap_set_header_content_format(&request, APPLICATION_JSON); // Set the content format to simple text
         coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);

         // Sending of the first payload: information about row, column and field ID
         snprintf(payload, sizeof(payload),
                  "{\"row\":%d, \"col\":%d, \"field_id\":%d}",
                  mov_data.current_row, mov_data.current_col, mov_data.field_id);
         coap_set_payload(&request, (const uint8_t *)payload, strlen(payload));
         COAP_BLOCKING_REQUEST(&server_ep, &request, save_response_callback);
         PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
         etimer_reset(&timer);

         // Sendind the second payload: Invio del secondo payload: NPK data
         snprintf(payload, sizeof(payload),
                  "{\"npk\":{\"n\":%d, \"p\":%d, \"k\":%d}}",
                  (int)round(npk_data.nitrogen), (int)round(npk_data.phosphorus), (int)round(npk_data.potassium));

         coap_set_payload(&request, (const uint8_t *)payload, strlen(payload));
         COAP_BLOCKING_REQUEST(&server_ep, &request, save_response_callback);
         PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
         etimer_reset(&timer);

         // Sending the third payload: moisture
         snprintf(payload, sizeof(payload),
                  "{\"moisture\":%d}", (int)round(moisture_data));
         coap_set_payload(&request, (const uint8_t *)payload, strlen(payload));
         COAP_BLOCKING_REQUEST(&server_ep, &request, save_response_callback);
         PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
         etimer_reset(&timer);

         // Sending the forth payload: temperature
         snprintf(payload, sizeof(payload),
                  "{\"temp\":%d}", (int)round(temperature_data));
         coap_set_payload(&request, (const uint8_t *)payload, strlen(payload));
         COAP_BLOCKING_REQUEST(&server_ep, &request, save_response_callback);
         PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
         etimer_reset(&timer);

         // Sending the fifth payload: pH
         snprintf(payload, sizeof(payload),
                  "{\"ph\":%d}", (int)round(ph_data));
         coap_set_payload(&request, (const uint8_t *)payload, strlen(payload));
         COAP_BLOCKING_REQUEST(&server_ep, &request, save_response_callback);
         PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
         etimer_reset(&timer);

         // Sending the sixth payload: seed type
         snprintf(payload, sizeof(payload),
                  "{\"seed_type\":%d}", seed_type);
         coap_set_payload(&request, (const uint8_t *)payload, strlen(payload));
         COAP_BLOCKING_REQUEST(&server_ep, &request, save_response_callback);
         PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
         etimer_reset(&timer);

         // Update position
         update_position(&mov_data);

         // reset il wake up timer
         etimer_set(&timer, 30 * CLOCK_SECOND);
      }
      else
      {
         if (!is_movement_active(&mov_data))
         {
            printf("Sowing process inactive.\n");
         }
         else if (is_move_complete(&mov_data))
         {
            printf("Sowing process already completed.\n");
         }
      }
   }

   printf("Exiting Actuator\n");
   PROCESS_END();
}

short int is_move_complete()
{
   return move_complete;
}

short int is_movement_active()
{
   return active;
}

void calculate_mat_dimensions()
{
   mov_data.total_rows = (int)ceil((double)mov_data.length / mov_data.square_size);
   mov_data.total_cols = (int)ceil((double)mov_data.width / mov_data.square_size);
}

void allocate_matrix()
{
   calculate_mat_dimensions(mov_data);
   mov_data.matrix = (unsigned int **)malloc(mov_data.total_rows * sizeof(unsigned int *));
   for (int i = 0; i < mov_data.total_rows; i++)
   {
      mov_data.matrix[i] = (unsigned int *)malloc(mov_data.total_cols * sizeof(unsigned int));
      memset(mov_data.matrix[i], 0, mov_data.total_cols * sizeof(unsigned int));
   }
}

void free_matrix()
{
   for (int i = 0; i < mov_data.total_rows; i++)
   {
      free(mov_data.matrix[i]);
   }
   free(mov_data.matrix);
}

void setup_movement_info(int length, int width, int square_size, int field_id)
{
   mov_data.length = length;
   mov_data.width = width;
   mov_data.square_size = square_size;
   mov_data.field_id = field_id;
   calculate_mat_dimensions(mov_data);
   allocate_matrix(mov_data);
}

void clear_movement_info()
{
   // Azzera tutti i campi della struttura
   mov_data.length = 0;
   mov_data.width = 0;
   mov_data.square_size = 0;
   mov_data.current_row = 0;
   mov_data.current_col = 0;
   mov_data.total_rows = 0;
   mov_data.total_cols = 0;
   mov_data.matrix = NULL; // The matrix is cleared, pointer to NULL
   mov_data.direction = 0; // Reset direction (assuming that 0 is a neutral value)
   mov_data.field_id = 0;  // Reset field ID

   // Clear the allocated memory for the matrix (function already defined)
   free_matrix();
}

int apply_decision_tree_model(npk npk_value, int ph, int moisture, int temp)
{
   // Apply the decision tree model to determine the type of seed to use
   int seed_type = seed_classifier_predict((int16_t[]){npk_value.nitrogen, npk_value.phosphorus, npk_value.potassium, ph, moisture, temp}, 6);
   return seed_type;
}

void update_position()
{
   // If movement is not active, exit the function
   if (!is_movement_active())
      return;

   // Mark the current position in the matrix as visited (value 1)
   mov_data.matrix[mov_data.current_row][mov_data.current_col] = 1;

   // Handle movement based on the current direction
   switch (mov_data.direction)
   {
   case 0: // Move right
      // If we are not at the right edge of the grid
      if (mov_data.current_col < mov_data.total_cols - 1)
      {
         mov_data.current_col++; // Move one column to the right
      }
      else
      {
         mov_data.direction = 1; // Otherwise, change direction to down
      }
      break;
   case 1: // Move down
      // If we are not at the bottom edge of the grid
      if (mov_data.current_row < mov_data.total_rows - 1)
      {
         mov_data.current_row++; // Move one row down
         // Change direction: if the row is even, go right, otherwise go left
         mov_data.direction = (mov_data.current_row % 2 == 0) ? 0 : 2;
      }
      else
      {
         set_movement_complete(); // If reaching the edge, mark the movement as complete
      }
      break;
   case 2: // Move left
      // If we are not at the left edge of the grid
      if (mov_data.current_col > 0)
      {
         mov_data.current_col--; // Move one column to the left
      }
      else
      {
         mov_data.direction = 1; // Otherwise, change direction to down
      }
      break;
   case 3: // Move up (not used in this code)
      // If we are not at the top edge of the grid
      if (mov_data.current_row > 0)
      {
         mov_data.current_row--; // Move one row up
         // Change direction: if the row is even, go right, otherwise go left
         mov_data.direction = (mov_data.current_row % 2 == 0) ? 0 : 2;
      }
      else
      {
         set_movement_complete(); // If reaching the edge, mark the movement as complete
      }
      break;
   }

   // If the movement is complete set the flag
   if (is_move_complete())
   {
      stop_movement(active);
   }
}

// Function to parse and identify the type of sensor from the data
int parse_sensors_data(const uint8_t *payload)
{
   if (payload == NULL)
   {
      printf("Payload is null.\n");
      return -1;
   }

   // Convert payload to string for searching
   const char *payload_str = (const char *)payload;

   int sensor_type = -1;

   // Check for different sensor type identifiers in the payload
   if (strstr(payload_str, "\"n\"") != NULL ||
       strstr(payload_str, "\"p\"") != NULL ||
       strstr(payload_str, "\"k\"") != NULL)
   {
      sensor_type = NPK_SENSOR;
   }
   else if (strstr(payload_str, "\"ph\"") != NULL)
   {
      sensor_type = PH_SENSOR;
   }
   else if (strstr(payload_str, "\"moisture\"") != NULL)
   {
      sensor_type = MOISTURE_SENSOR;
   }
   else if (strstr(payload_str, "\"temperature\"") != NULL)
   {
      sensor_type = TEMP_SENSOR;
   }

   // Return identified sensor type, or -1 if not found
   return sensor_type;
}
int evaluate_sensor_type(const char *payload_str)
{

   int sensor_type = -1;
   if (strstr(payload_str, "\"n\"") != NULL ||
       strstr(payload_str, "\"p\"") != NULL ||
       strstr(payload_str, "\"k\"") != NULL)
   {
      sensor_type = NPK_SENSOR;
   }
   else if (strstr(payload_str, "\"ph\"") != NULL)
   {
      sensor_type = PH_SENSOR;
   }
   else if (strstr(payload_str, "\"moisture\"") != NULL)
   {
      sensor_type = MOISTURE_SENSOR;
   }
   else if (strstr(payload_str, "\"temperature\"") != NULL)
   {
      sensor_type = TEMP_SENSOR;
   }

   return sensor_type;
}

void start_movement()
{
   active = ACTIVE;
   leds_single_on(LEDS_YELLOW);
   obs_();
}

void stop_movement() 
{
   active = INACTIVE;
   leds_single_off(LEDS_YELLOW);
   obs_();
}


void set_movement_complete(){
   move_complete=1;
   leds_single_off(LEDS_YELLOW);
}
void set_movement_uncomplete(){
   move_complete=0;
}