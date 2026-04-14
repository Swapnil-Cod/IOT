# MQTT Topics Reference

This document provides an overview of the MQTT topics used in the project, including conventions and examples for effective communication between devices and the backend.

## Topic Structure

The topic structure follows a hierarchical format, allowing for organized and clear communication. The general format for device-related topics is:

```
home/device/<device_id>/<topic_type>
```

### Topic Types

1. **Config** (Retained)
   - **Topic:** `home/device/<device_id>/config`
   - **Description:** This topic is used to publish the configuration settings for a device. The configuration is retained, allowing new or returning devices to retrieve their settings upon connection.
   - **Example Payload:**
     ```json
     {
       "id": "device1234",
       "type": "light",
       "gpio": 16,
       "cmd_topic": "home/device/device1234/cmd",
       "state_topic": "home/device/device1234/state",
       "payload_on": "ON",
       "payload_off": "OFF",
       "friendly_name": "Bedroom Lamp",
       "hw_version": "v1",
       "sw_version": "1.0.0"
     }
     ```

2. **Commands**
   - **Topic:** `home/device/<device_id>/cmd`
   - **Description:** This topic is used for sending commands to the device. Messages published to this topic are not retained.
   - **Example Command:** 
     ```
     {"action": "turn_on"}
     ```

3. **State** (Retained)
   - **Topic:** `home/device/<device_id>/state`
   - **Description:** This topic is used by the device to publish its current state. The state is retained to ensure that subscribers receive the last known state.
   - **Example Payload:**
     ```json
     {
       "status": "ON",
       "brightness": 75
     }
     ```

4. **Availability** (LWT - Last Will and Testament)
   - **Topic:** `home/device/<device_id>/avail`
   - **Description:** This topic is used to indicate the availability of the device. It is retained, allowing subscribers to know if the device is online or offline.
   - **Example Messages:**
     - On connect: `online`
     - On disconnect: `offline`

5. **Telemetry / Sensors**
   - **Topic:** `home/device/<device_id>/sensor/<sensor_type>`
   - **Description:** This topic is used for publishing telemetry data from sensors associated with the device. The retention policy can vary based on the type of data.
   - **Example Topic:** `home/device/device1234/sensor/temperature`
   - **Example Payload:**
     ```json
     {
       "temperature": 22.5
     }
     ```

### Home Assistant Discovery

To facilitate integration with Home Assistant, the following topic is used for device discovery:

- **Topic:** `homeassistant/<component>/<unique_id>/config`
- **Description:** This topic is used to publish the configuration for Home Assistant discovery, allowing automatic addition of the device to the Home Assistant interface.
- **Example Payload:**
  ```json
  {
    "name": "Bedroom Lamp",
    "uniq_id": "device1234_light",
    "cmd_t": "home/device/device1234/cmd",
    "stat_t": "home/device/device1234/state",
    "avty_t": "home/device/device1234/avail",
    "pl_on": "ON",
    "pl_off": "OFF"
  }
  ```

## Conclusion

This document outlines the MQTT topic conventions used in the project. Adhering to these conventions ensures effective communication and integration between devices and the backend, as well as compatibility with Home Assistant and other ecosystems.