# Messaging Design

## Overview

This document outlines the messaging design for the ESP32 firmware and backend system, focusing on best practices for topic design and message formats used in the system.

## Topic Design

### Best Practices

1. **Per-Device Retained Config**: Each device should have its own retained configuration topic to ensure that the latest configuration is always available upon connection.
   - Example: `home/device/<device_id>/config`

2. **Command Topics**: Use dedicated command topics for sending commands to devices. These topics should not retain messages.
   - Example: `home/device/<device_id>/cmd`

3. **State Topics**: Devices should publish their state to a retained state topic to allow subscribers to know the last known state.
   - Example: `home/device/<device_id>/state`

4. **Availability Topics**: Implement Last Will and Testament (LWT) messages to indicate device availability.
   - Example: `home/device/<device_id>/avail`

5. **Telemetry Topics**: Use specific topics for telemetry data, allowing for easy monitoring of device metrics.
   - Example: `home/device/<device_id>/sensor/<sensor_type>`

6. **Home Assistant Discovery**: Publish Home Assistant discovery messages to facilitate automatic integration with Home Assistant.
   - Example: `homeassistant/<component>/<unique_id>/config`

## Message Formats

### Configuration Message (Retained JSON)

```json
{
  "id": "<device_id>",
  "type": "<device_type>",
  "gpio": "<gpio_mapping>",
  "cmd_topic": "home/device/<device_id>/cmd",
  "state_topic": "home/device/<device_id>/state",
  "payload_on": "<payload_on>",
  "payload_off": "<payload_off>",
  "friendly_name": "<friendly_name>",
  "hw_version": "<hardware_version>",
  "sw_version": "<software_version>"
}
```

### Command Message

The command message format is flexible and can vary based on the command being issued. It is typically a simple string or JSON object.

### State Message

The state message format should reflect the current state of the device and can include additional telemetry data as needed.

### Availability Message

The availability message should indicate whether the device is online or offline, typically using a simple string.

## Conclusion

Following these messaging design principles will ensure a robust and scalable communication framework for the ESP32 devices and their integration with the backend and other systems.