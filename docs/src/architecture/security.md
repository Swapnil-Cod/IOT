# Security Measures in the Project

## TLS Usage
All communications between devices and the MQTT broker, as well as between the backend and clients, are secured using Transport Layer Security (TLS). This ensures that data in transit is encrypted, protecting it from eavesdropping and man-in-the-middle attacks.

## Authentication Methods
Each device is assigned unique credentials for authentication with the MQTT broker. Options include:
- **Client Certificates**: Each device can use a unique client certificate for authentication, ensuring that only authorized devices can connect.
- **Username/Password**: Alternatively, devices can authenticate using a username and password, which can be rotated periodically for enhanced security.

## Secure Provisioning Practices
Provisioning devices securely is critical to prevent unauthorized access. The following practices are implemented:
- **One-Time Provisioning Tokens**: Devices can only accept configuration over an open access point if a valid one-time provisioning token is provided.
- **QR Code Pairing**: For user-friendly provisioning, devices can utilize QR codes that contain secure provisioning information.
- **SoftAP Mode**: Devices enter a SoftAP mode for initial setup, allowing users to connect and configure them securely.

## Broker Access Control Lists (ACLs)
Access Control Lists (ACLs) are enforced on the MQTT broker to restrict which topics each device or user can publish to or subscribe from. This minimizes the risk of unauthorized access to sensitive topics.

## Signed Firmware Updates
Firmware updates are signed to ensure authenticity. Devices verify the signature before installing updates, preventing the installation of malicious firmware.

## Secure Storage of Secrets
All sensitive information, such as credentials and tokens, is stored securely using the ESP32 Non-Volatile Storage (NVS) to prevent exposure in logs or during runtime.