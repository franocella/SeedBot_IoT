import os
import json
import threading
import signal
from coapthon.server.coap import CoAP
from coapthon.resources.resource import Resource
from db_manager_mysql import add_device, create_database_and_tables, get_sensor_by_name, add_cell, npk
from coapthon import defines
import re

expected_keys = {'npk', 'ph', 'moisture', 'temp', 'seed_type', 'row', 'col', 'field_id'}
received_data = {}


class RegistrationResource(Resource):
    def __init__(self, name="RegistrationResource", coap_server=None):
        super(RegistrationResource, self).__init__(name, coap_server, visible=True, observable=True)
        self.content_format = "text/plain"
        self.payload = "Register a device by sending its name."

    def render_POST_advanced(self, request, response):
        """
        Method to handle POST requests for device registration.
        :param request: The incoming CoAP request
        :param response: The outgoing CoAP response
        :return: A tuple containing the resource and the response
        """
        try:
            device_name = request.payload.strip()
            print(f"Received device name: {device_name}")  # Debug log

            if not device_name:
                response.code = defines.Codes.BAD_REQUEST.number  # 4.00 Bad Request
                response.payload = "Error: Device name is required."
                print("Error: Device name is required.")  # Debug log
                return self, response

            ip_address = request.source[0]
            print(f"Received from IP: {ip_address}")  # Debug log

            # Add the device to the database
            return_code = add_device(device_name, ip_address)

            if return_code == 1:
                response.code = defines.Codes.CREATED.number  # 2.01 Created
                response.payload = f"Device '{device_name}' registered successfully from IP {ip_address}."
            elif return_code == 2:
                response.code = defines.Codes.CHANGED.number  # 2.04 Changed
                response.payload = f"Device '{device_name}' updated successfully, new IP {ip_address}."

            print(response.payload)  # Debug log

        except Exception as e:
            response.code = defines.Codes.INTERNAL_SERVER_ERROR.number  # 5.00 Internal Server Error
            response.payload = f"Error in registration: {str(e)}"
            print(f"Error in registration: {str(e)}")  # Debug log

        return self, response
    


class DeviceNameDiscoverResource(Resource):
    def __init__(self, name="DeviceNameDiscoverResource", coap_server=None):
        super(DeviceNameDiscoverResource, self).__init__(name, coap_server, visible=True, observable=True)
        self.content_format = "application/json"

    def render_POST_advanced(self, request, response):
        """
        Handle advanced POST request for device discovery by name.
        """
        try:
            # Parse JSON payload
            payload_str = request.payload.decode('utf-8') if isinstance(request.payload, bytes) else request.payload
            
            print(f"RICEVUTO QUESTO PAYLOAD {payload_str}")
            
            payload = json.loads(payload_str)
            print(f"Received JSON payload: {payload}")

            # Extract the device name from the payload
            device_name = payload.get('name')
            if not device_name:
                # If 'name' is not present in the payload, return a bad request response
                response.code = defines.Codes.BAD_REQUEST.number  # 4.00 Bad Request
                response.payload = json.dumps({"error": "Device name is required"})
                return self, response

            print(f"Received device name: {device_name}")

            # Get the device information using the device name
            device_dict = get_sensor_by_name(device_name)

            if device_dict and isinstance(device_dict, dict):
                # If device is found and is a dictionary
                if 'name' in device_dict and 'ipv6_address' in device_dict:
                    # Properly serialize device information to JSON
                    response.payload = json.dumps({
                        device_dict['name']: device_dict['ipv6_address']
                    })
                    response.code = defines.Codes.CONTENT.number  # 2.05 Content
                    response.content_type = defines.Content_types["application/json"]
                else:
                    # If device data is incomplete
                    response.code = defines.Codes.INTERNAL_SERVER_ERROR.number  # 5.00 Internal Server Error
                    response.payload = json.dumps({"error": "Device data is incomplete"})
            else:
                # If device is not found
                response.code = defines.Codes.NOT_FOUND.number  # 4.04 Not Found
                response.payload = json.dumps({"error": "Device not found"})

        except json.JSONDecodeError:
            # JSON decoding failed
            response.code = defines.Codes.BAD_REQUEST.number  # 4.00 Bad Request
            response.payload = json.dumps({"error": "Invalid JSON payload"})
        
        except Exception as e:
            # General exception handling
            response.code = defines.Codes.INTERNAL_SERVER_ERROR.number  # 5.00 Internal Server Error
            response.payload = json.dumps({"error": str(e)})
            print(f"Error in render_POST_advanced: {str(e)}")  # Debug log

        print(f"Response payload: {response.payload}")  # Debug log
        return self, response


class SaveResource(Resource):
    def __init__(self, name="SaveResource", coap_server=None):
        super(SaveResource, self).__init__(name, coap_server=coap_server, visible=True, observable=True)
        self.content_format = "application/json"  

    def render_POST_advanced(self, request, response):
        """
        Method for handling POST requests. Accumulates data and updates the DB when all necessary data has been received.
        :param request: The incoming CoAP request.
        :param response: The outgoing CoAP response
        :return: A tuple containing the resource and the response
        """
        print("Received advanced POST request.")
        
        try:
            payload_str = request.payload
            print(f"Raw payload: {payload_str}")

            if not payload_str:
                response.code = defines.Codes.BAD_REQUEST.number
                response.payload = "No payload received"
                return self, response

            # Parse the JSON payload
            payload = json.loads(payload_str)
            print(f"Parsed JSON payload: {payload}")
            
            # Update data received globally
            global received_data
            received_data.update(payload)

            # Check if all the necessary data are present            
            if all(k in received_data for k in expected_keys):
                npk_values = received_data.get('npk', {})
                npk_data = npk(n=npk_values.get('n'), p=npk_values.get('p'), k=npk_values.get('k'))

                return_code = add_cell(
                    field_id=received_data.get('field_id'),
                    c_row=received_data.get('row'),
                    c_col=received_data.get('col'),
                    npk=npk_data,
                    moisture=received_data.get('moisture'),
                    ph=received_data.get('ph'),
                    temperature=received_data.get('temp'),
                    sowed=received_data.get('seed_type')
                )                    
                print(f"Received data: {received_data.values()}")

                # Clear the accumulated data after saving
                received_data.clear()

                # Set the response based on the return code
                if return_code == 1:
                    response.code = defines.Codes.CREATED.number  # 2.01 Created
                    response.payload = "New cell added"
                elif return_code == 2:
                    response.code = defines.Codes.CHANGED.number  # 2.04 Changed
                    response.payload = "Cell updated"
                else:
                    response.code = defines.Codes.CONTENT.number  # 2.05 Content
                    response.payload = "Cell data unchanged"
            else:
                # Still missing data, wait for more messages
                print("Still missing data, waiting for more.")
                response.code = defines.Codes.VALID.number  # 2.03 Valid
                response.payload = "Data received, waiting for more."

        except json.JSONDecodeError:
            print("Error: Invalid JSON payload")
            response.code = defines.Codes.BAD_REQUEST.number
            response.payload = "Invalid JSON payload"
        except Exception as e:
            print(f"Error: {str(e)}")
            response.code = defines.Codes.INTERNAL_SERVER_ERROR.number
            response.payload = f"Error: {str(e)}"

        return self, response

class CoAPServer(CoAP):
    def __init__(self, host, port=5683):
        super(CoAPServer, self).__init__((host, port))
        self.add_resource('register', RegistrationResource())
        self.add_resource('discover', DeviceNameDiscoverResource()) 
        self.add_resource('save', SaveResource())
        self._running = threading.Event()
        self._running.set()

    def start(self):
        print(f"Starting CoAP server...")
        while self._running.is_set():
            self.listen(10)

    def stop(self):
        """
        Shutdown the CoAP server by closing the socket.
        """
        if self._socket:  # Check if the socket exists before closing it
            self._running.clear()  # Indicate that the server should stop
            self._socket.close()
            print("CoAP Server stopped.")

def signal_handler(signal, frame):
    print("\nInterrupt received, stopping server...")
    coap_server.stop()
    os._exit(0)

if __name__ == "__main__":

    # Create the database and tables if they don't exist
    create_database_and_tables()

    host = "::"
    port = 5683
    coap_server = CoAPServer(host, port)

    # Set up the signal handler for interrupts
    signal.signal(signal.SIGINT, signal_handler)

    try:
        print("CoAP Server is running...")
        coap_server.start()
    except Exception as e:
        print(f"Exception occurred: {e}")
        coap_server.stop()
        os._exit(1)
