# Architecture Components Overview

This document provides an overview of the various components involved in the system architecture, detailing their roles and interactions.

## ESP32 Device (Firmware)

The ESP32 device serves as the core hardware component, responsible for:

- Wi-Fi provisioning
- Establishing a secure MQTT connection
- Handling Last Will and Testament (LWT) messages
- Subscribing to configuration and command topics
- Publishing state and availability messages
- Supporting Over-The-Air (OTA) updates

## MQTT Broker

The MQTT broker acts as the message backbone of the system. Key responsibilities include:

- Facilitating communication between devices and the backend
- Supporting TLS for secure connections
- Managing authentication and access control lists (ACLs)
- Retaining messages for persistent storage

## Backend / Controller (Flask)

The backend, initially built with Flask, manages:

- User accounts and authentication
- Device metadata, including configurations and topics
- An admin UI for device management
- Publishing retained configurations to the MQTT broker
- Optionally persisting device telemetry for analytics

## Web / Mobile App (Optional)

The web or mobile app provides a user interface for:

- Device provisioning and management
- Triggering OTA updates
- Setting automation rules and viewing logs

## OTA Server

The OTA server hosts firmware images, allowing devices to pull updates securely via HTTP(S). It ensures:

- Secure signed updates in production
- Efficient management of firmware versions

## CI/CD & Build Pipeline

The CI/CD pipeline automates the build process for firmware artifacts, ensuring:

- Versioning of firmware images
- Signing of images for security
- Streamlined release management

## Monitoring & Logging

Monitoring components capture:

- Device health and status
- Broker metrics and backend performance
- Crash reports and error logs for troubleshooting

## Conclusion

Understanding these components and their interactions is crucial for maintaining and extending the system architecture effectively.