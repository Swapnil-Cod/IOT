# API Documentation

## Overview

This document provides an overview of the API endpoints available in the project, including their request and response formats.

## API Endpoints

### Provision Device

- **Endpoint:** `POST /api/provision`
- **Description:** Exchanges onboarding token and assigns device to account (optional).
- **Request Body:**
  ```json
  {
    "token": "string",
    "device_id": "string"
  }
  ```
- **Response:**
  - **Status Code:** 200 OK
  - **Body:**
  ```json
  {
    "message": "Device provisioned successfully",
    "device_id": "string"
  }
  ```

### Get User Devices

- **Endpoint:** `GET /api/devices`
- **Description:** Returns the list of devices associated with the user.
- **Response:**
  - **Status Code:** 200 OK
  - **Body:**
  ```json
  [
    {
      "id": "string",
      "friendly_name": "string",
      "type": "string"
    }
  ]
  ```

### Add Device

- **Endpoint:** `POST /api/devices`
- **Description:** Adds a new device and publishes retained config.
- **Request Body:**
  ```json
  {
    "device": {
      "id": "string",
      "friendly_name": "string",
      "type": "string",
      "gpio_map": {}
    }
  }
  ```
- **Response:**
  - **Status Code:** 201 Created
  - **Body:**
  ```json
  {
    "message": "Device added successfully",
    "device_id": "string"
  }
  ```

### Delete Device

- **Endpoint:** `DELETE /api/devices/<id>`
- **Description:** Removes a device and clears retained config.
- **Response:**
  - **Status Code:** 204 No Content

### Upload Firmware

- **Endpoint:** `POST /api/firmware`
- **Description:** Uploads firmware and notifies devices.
- **Request Body:**
  ```json
  {
    "firmware": "binary"
  }
  ```
- **Response:**
  - **Status Code:** 200 OK
  - **Body:**
  ```json
  {
    "message": "Firmware uploaded successfully"
  }
  ```

## Error Handling

All API responses will include an error message in the following format in case of failure:

- **Response:**
  - **Status Code:** 400 Bad Request (or other relevant status)
  - **Body:**
  ```json
  {
    "error": "Error message describing the issue"
  }
  ```