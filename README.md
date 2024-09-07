### SeedBot: Automated Sowing Device

Welcome to the repository for SeedBot, developed as part of the IoT exam at the University of Pisa. This project focuses on automating the sowing process in agriculture by using a network of sensors and actuators to monitor soil conditions and control seed distribution.

## Table of Contents

- [Project Overview](#project-overview)
- [System Architecture](#system-architecture)
- [Machine Learning Model](#machine-learning-model)
- [Hardware and Software Requirements](#hardware-and-software-requirements)
- [License](#license)

## Project Overview

This project is divided into two main phases:
1. **Data Analysis and Machine Learning Model Construction:** Analyze datasets to develop a machine learning model for determining the most suitable crop for a specific plot of land.
2. **IoT System Implementation:** Deploy an IoT network to monitor soil conditions and control the seed distribution mechanism.

## System Architecture

The system consists of the following components:

- **IoT Devices:** Sensors for soil NPK levels, moisture, pH and average temperature; actuators for movement and seed distribution.
- **Communication Protocols:** Use of CoAP (Constrained Application Protocol) for communication between sensors/actuators and the cloud application.
- **Machine Learning:** Model implemented on IoT devices for autonomous decision-making regarding crop suitability and seed distribution.
- **Cloud Application:** Collects data from sensors, stores it in a MySQL database, and provides a web-based interface using Grafana.
- **Remote Control Application:** Implements control logic to adjust actuators based on sensor data.
- **User Input:** Flask GUI.

### System Scheme
#### With Machine Learning Model

![System Scheme With ML](ML/media/SeedBot_architecture.svg)

## Machine Learning Model

The machine learning model is trained to determine the most suitable crop for a specific plot of land based on soil conditions. The model is trained using open datasets. The Jupyter notebook used for training the model is included in the repository.

The initial notebook used is the following:
https://www.kaggle.com/code/mdshariaremonshaikat/optimizing-agricultural-production-with-7-ml-model/notebook

## Hardware and Software Requirements

- **Hardware:** 
  - 6 nRF52840 devices for sensors and actuators
  - Actuators compatible with CoAP
  - Border router for network deployment

- **Software:**
  - Python for application development
  - Grafana for data visualization
  - MySQL for database management
  - CoAP libraries for communication

## License

This project is licensed under Creative Commons Attribution-NonCommercial 4.0 International License. See the [LICENSE](LICENSE) file for details.

---

For more details, please refer to the project documentation and the [attached PDF](Documentation/documentation.pdf).

---
