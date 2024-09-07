from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
from flask_cors import CORS

import threading
import datetime
from db_manager_mysql import get_field_progress, FieldNotFoundError
from db_manager_mysql import add_field, get_sensor_by_name
from coapthon.client.helperclient import HelperClient
import json


app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*") 


# Global variables to store sowing state and initialization status
sowing_initialized = False
sowing_status = "Not started"

# Create a lock object to manage access to global variables
lock = threading.Lock()


# Classe CoAPObserver
class CoAPObserver:
    def __init__(self, server_host, server_port, resource_path):
        self.server_host = server_host
        self.server_port = server_port
        self.resource_path = resource_path
        self.client = HelperClient(server=(self.server_host, self.server_port))
        self.lock = threading.Lock()

    def observe(self):
        """
        Subscribes the observer in the CoAP resource and handles notifications.        
        """
        try:
            self.client.observe(self.resource_path, self.handle_notification)
            print(f"Subscribed to {self.resource_path}. Waiting for notifications...")
        except Exception as e:
            print(f"Failed to subscribe: {e}")

    def handle_notification(self, response):
        """
        Handles notifications received from the observable resource and sends via WebSocket.        
        """
        if response:
            payload_str = response.payload.decode('utf-8') if isinstance(response.payload, bytes) else response.payload
            print(f"Raw payload: {payload_str}")
            global sowing_status
            try:
                data = json.loads(payload_str)
                print(f"Parsed JSON data: {data}")

                complete = data.get("complete")
                active = data.get("active")

                if complete is not None and active is not None:
                    print(f"Complete: {complete}, Active: {active}")
                else:
                    print("Error: Missing expected keys in the data.")
                with self.lock:
                    if complete:
                        print("Sowing process complete.")
                        self.stop_observing()
                        sowing_status = "Complete"
                        self.send_websocket_update("Complete")
                    elif not complete and not active:
                        print("Sowing process paused.")
                        sowing_status = "Paused"
                        self.send_websocket_update("Paused")
                    elif not complete and active:
                        print("Sowing process resumed.")
                        sowing_status = "In progress"
                        self.send_websocket_update("In progress")

            except json.JSONDecodeError:
                print("Error: Invalid JSON payload")
        else:
            print("No response received")

    def send_websocket_update(self, status):
        """
        Send updated status via WebSocket.
        """
        with app.app_context():
            socketio.emit('sowing_status', {'status': status})
    
    def stop_observing(self):
        """
        End the observation and close the client.
        """
        self.client.stop()
        print("Stopped observing.")


# Function to send COAP messages
def send_coap_msg_to_actuator(method, ip_address, payload=None):
    """
    Sends a COAP message to a specified IP address.

    :param method: The method to use ('POST', 'PUT', 'DELETE')
    :param ip_address: The IP address of the COAP server
    :param payload: The payload to send (as a dictionary or None)
    :return: The response from the COAP server, or None if there was an error
    """
    try:
        client = HelperClient(server=(ip_address, 5683))  # Default COAP port is 5683
        response = None
        if method == "POST":
            response = client.post("sowing_actuator", payload)
        elif method == "PUT":
            response = client.put("sowing_actuator", payload)
        elif method == "DELETE":
            response = client.delete("sowing_actuator")
        client.stop()
        return response
    except Exception as e:
        print(f"Error sending COAP message: {e}")
        return None

# Serve HTML pages
@app.route("/")
def index():
    # Render the SeedBot HTML page
    return render_template("SeedBot.html")

@app.route("/sowing_control")
def sowing_control():
    # Render the Sowing Control HTML page
    return render_template("Sowing_control.html")

# RESTful endpoints
@app.route("/sowing", methods=["POST"])
def start_sowing():
    """
    Initializes and starts the sowing process, if not already started.

    Returns:
        Response: A JSON response indicating the result of the operation:
            - 200 if the sowing process is successfully initialized.
            - 400 if the sowing process has already been initialized or if required fields are missing.
            - 500 if an error occurs, such as actuator not found or failed COAP communication.
    """  
    global sowing_initialized, sowing_status
    with lock:
        if sowing_initialized:
            # Return error if sowing has already been initialized
            return jsonify({"message": "Sowing already initialized"}), 400
        else:
            data = request.get_json()
            if not data or 'length' not in data or 'width' not in data or 'square_size' not in data:
                # Return error if required fields are missing
                return jsonify({"message": "Missing required fields"}), 400
            
            length = data['length']
            width = data['width']
            square_size = data['square_size']
            
            # Logic to start the sowing process
            sowing_initialized = True
            
            # Service to start the sowing process
            start_sowing_date = datetime.datetime.now()

            # Get the actuator IP from the database
            actuator = get_sensor_by_name("sowing_actuator")
            actuator_ip = actuator['ipv6_address'] if actuator else None

            if not actuator_ip:
                # Return error if actuator is not found
                return jsonify({"message": "Actuator not found"}), 500

            # Call the add_field function with the relevant data
            field_id = add_field(length, width, square_size, start_sowing_date)

            # Send COAP message to the sowing actuator
            coap_payload = f'{{"length": {length}, "width": {width}, "square_size": {square_size}, "field_id": {field_id}}}'

            print(f"Sending COAP message to actuator {actuator_ip} with payload: {coap_payload}")

            coap_response = send_coap_msg_to_actuator("POST", actuator_ip, coap_payload)

            # Check COAP response
            if coap_response is None:
                # Return error if COAP message failed
                return jsonify({"message": "Failed to send COAP message"}), 500

            observer = CoAPObserver(server_host=actuator_ip, server_port=5683, resource_path='sowing_actuator/status')
            observer.observe()

            sowing_status = "In progress"

            # Return success message
            return jsonify({"message": "Sowing initialized", "field_id": field_id}), 200


@app.route("/sowing", methods=["PUT"])
def change_sowing_status():
    """
    Toggles the sowing process between 'In progress' and 'Paused' states, if sowing has been initialized.

    Returns:
        Response: A JSON response indicating the result of the operation:
            - 200 if the sowing process was successfully paused or resumed.
            - 400 if the sowing process was not initialized.
            - 500 if an error occurred, such as the failure to find the actuator's IP or change the sowing status.
    """    
    global sowing_initialized, sowing_status
    with lock:
        if sowing_initialized:
            # Get the actuator IP from the database
            actuator = get_sensor_by_name("sowing_actuator")
            if not actuator or 'ipv6_address' not in actuator:
                # Return error if actuator IP not found
                return jsonify({"message": "Actuator IP not found"}), 500

            actuator_ip = actuator['ipv6_address']

            if sowing_status == "In progress":
                sowing_status = "Paused"
                # Send COAP message to pause the sowing process
                response = send_coap_msg_to_actuator("PUT", actuator_ip, "stop")
                if response is None:
                    # Return error if failed to pause sowing process
                    return jsonify({"message": "Failed to pause sowing process"}), 500
                # Return success message
                return jsonify({"message": "Sowing paused"}), 200
            elif sowing_status == "Paused":
                sowing_status = "In progress"
                # Send COAP message to resume the sowing process
                response = send_coap_msg_to_actuator("PUT", actuator_ip, "start")
                if response is None:
                    # Return error if failed to resume sowing process
                    return jsonify({"message": "Failed to resume sowing process"}), 500
                # Return success message
                return jsonify({"message": "Sowing resumed"}), 200
        else:
            # Return error if sowing was not initialized
            return jsonify({"message": "Sowing not initialized"}), 400


@app.route("/sowing", methods=["DELETE"])
def stop_sowing():
    """ 
    Stops the sowing process if it has been initialized.

    Returns:
        Response: A JSON response indicating the result of the operation:
            - 200 if the sowing process was successfully stopped.
            - 400 if the sowing process was not initialized.
            - 500 if an error occurred, such as the failure to find the actuator's IP or stop the sowing process.

    """    
    global sowing_initialized, sowing_status
    with lock:
        if sowing_initialized:
            # Get the actuator IP from the database
            actuator = get_sensor_by_name("sowing_actuator")
            if not actuator or 'ipv6_address' not in actuator:
                # Return error if actuator IP not found
                return jsonify({"message": "Actuator IP not found"}), 500

            actuator_ip = actuator['ipv6_address']

            # Send COAP message to stop the sowing process
            response = send_coap_msg_to_actuator("DELETE", actuator_ip)
            if response is None:
                # Return error if failed to stop sowing process
                return jsonify({"message": "Failed to stop sowing process"}), 500

            # Reset sowing state
            sowing_initialized = False
            sowing_status = "Not started"
            # Return success message
            return jsonify({"message": "Sowing stopped"}), 200
        else:
            # Return error if sowing was not initialized
            return jsonify({"message": "Sowing not initialized"}), 400



@app.route("/sowing", methods=["GET"])
def get_sowing_percentage():
    """
    Retrieves the sowing progress for a specific field, if the sowing process is in progress.

    Returns:
        Response: A JSON response containing:
            - "progress_info" (dict or None): The progress data of the requested field, if available.
            - "status" (str): The current sowing status.
            - Appropriate HTTP status codes:
                - 200 if the request is successful.
                - 400 if the field_id parameter is invalid.
                - 404 if the field is not found.
                - 409 if the sowing process is not in progress.
                - 500 if an unexpected error occurs.
    """
    global sowing_status
    with lock:
        if sowing_status == "In progress":
            # Get the field ID from the request
            field_id = request.args.get("field_id")
            
            # Validate the field ID
            if not field_id or not field_id.isdigit():
                return jsonify({"message": "Invalid field_id parameter"}), 400

            field_id = int(field_id)
            
            try:
                # Get the progress of the field
                progress_info = get_field_progress(field_id)
                
                if progress_info:
                    # Combine the progress data with the sowing status
                    response_data = {
                        "progress_info": progress_info,
                        "status": sowing_status
                    }
                    return jsonify(response_data), 200
                else:
                    return jsonify({"message": "Field not found"}), 404
            
            except FieldNotFoundError:
                return jsonify({"message": "Field not found"}), 404
            except Exception as e:
                # Log the exception and return a generic error message
                app.logger.error(f"An unexpected error occurred: {e}")
                return jsonify({"message": "An unexpected error occurred"}), 500
        
        else:
            # Return an error if sowing is not in progress, including the sowing status
            response_data = {
                "progress_info": None,
                "status": sowing_status
            }
            return jsonify(response_data), 409

if __name__ == "__main__":
    # Run the Flask application in debug mode
    app.run(debug=True)
