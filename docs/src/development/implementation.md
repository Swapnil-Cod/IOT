# Implementation Choices & Trade-offs

## Broker Choice
- **LAN-only / Simple**: Mosquitto on Raspberry Pi or on the same LAN.
- **Cloud Launch**: Managed MQTT (HiveMQ Cloud, AWS IoT Core) — easier TLS, scaling, and authentication.

## Provisioning
- **SoftAP** for initial prototype.
- For a polished UX, consider **BLE provisioning** or hotspot + companion app with device claims.

## Backend Hosting
- Start with a single **Flask instance** + PostgreSQL + Mosquitto.
- For scale, move to **containerized microservices**, load balancer, and managed database.

## Device Lifecycle
- Implemented a robust device lifecycle to ensure seamless provisioning, configuration, and updates.
- Focus on user experience during the initial setup and ongoing management.

## Security Measures
- Emphasized the importance of security throughout the implementation, including:
  - **TLS** for MQTT and HTTPS for backend APIs.
  - Unique per-device credentials and secure provisioning methods.

## OTA Updates
- Designed the OTA update process to ensure secure and reliable firmware updates.
- Implemented signature verification and a two-stage update process for safety.

## Testing Strategy
- Adopted a comprehensive testing strategy to cover unit, integration, and system tests.
- Included security audits and interoperability tests to ensure robustness.

## Trade-offs
- Balancing ease of use with security and scalability was a key consideration.
- Chose technologies that provide flexibility for future enhancements while meeting current project needs.