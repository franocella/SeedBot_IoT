#!/bin/bash

# Check if the "venv" directory exists
if [ -d "venv" ]; then
    echo "Virtual environment already exists."
else
    echo "Virtual environment not found. Creating one..."
    
    # Create the virtual environment
    python3 -m venv venv

    # Upgrade pip to the latest version
    source venv/bin/activate
    python3 -m pip install --upgrade pip

    # Install the required libraries
    pip install -r requirements.txt

    echo "Virtual environment created and libraries installed."
fi

# Activate the virtual environment
source venv/bin/activate
echo "Virtual environment activated."

# Start the CoAP server in a new terminal window
echo "Starting the CoAP server in a new terminal..."
gnome-terminal -- bash -c "source venv/bin/activate; python3 Source_Python/Flask/coap_server.py; exec bash" &

# Start the Flask application
echo "Starting the Flask application..."
python3 Source_Python/Flask/app.py &

# Run the make command in a new terminal window
echo "Running make command in a new terminal..."
gnome-terminal -- bash -c "make -C /home/iot_ubuntu_intel/contiki-ng/examples/rpl-border-router TARGET=nrf52840 BOARD=dongle connect-router; exec bash" &

# Keep the terminal window open (if running the script from a terminal)
exec bash

