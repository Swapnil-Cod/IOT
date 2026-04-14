# IoT App Backend API Documentation

**Base URL:** `http://13.203.114.29`

**API Version:** 1.0

**Last Updated:** 2025-10-15

---

## Table of Contents
- [Authentication](#authentication)
- [API Endpoints](#api-endpoints)
  - [User Registration](#1-user-registration)
  - [User Login](#2-user-login)
  - [Refresh Access Token](#3-refresh-access-token)
  - [Logout](#4-logout)
  - [Get User Profile](#5-get-user-profile)
  - [Update User Profile](#6-update-user-profile)
  - [Delete User](#7-delete-user)
- [Error Responses](#error-responses)
- [Status Codes](#status-codes)

---

## Authentication

This API uses **JWT (JSON Web Tokens)** for authentication with a dual-token system:

- **Access Token:** Short-lived token (60 minutes) used for API requests
- **Refresh Token:** Long-lived token (30 days) used to obtain new access tokens

### How to Use Tokens

1. After successful login, you'll receive both `access_token` and `refresh_token`
2. Include the access token in the `Authorization` header for protected endpoints:
   ```
   Authorization: Bearer <access_token>
   ```
3. When the access token expires, use the refresh token to get a new access token
4. Store the refresh token securely (e.g., secure storage, not localStorage)

### Protected Endpoints

The following endpoints require authentication (Bearer token in Authorization header):
- `GET /api/users`
- `PUT /api/users`
- `DELETE /api/users`
- `POST /api/logout`

---

## API Endpoints

### 1. User Registration

**Endpoint:** `POST /api/users`

**Description:** Register a new user account

**Authentication:** Not required

**Request Headers:**
```
Content-Type: application/json
```

**Request Body:**
```json
{
  "phone": "1234567890",           // Required: User's phone number (unique)
  "username": "John Doe",          // Required: User's display name
  "password": "mySecurePass123",   // Required: User's password (will be hashed)
  "device_id": "device_001"        // Optional: Device identifier
}
```

**Success Response (201 Created):**
```json
{
  "status": "ok",
  "message": "Registration successful"
}
```

**Error Responses:**
- `400 Bad Request` - Missing required fields or invalid JSON
- `400 Bad Request` - Phone number already exists (will update existing user)

**Example cURL:**
```bash
curl -X POST http://13.203.114.29/api/users \
  -H "Content-Type: application/json" \
  -d '{
    "phone": "1234567890",
    "username": "John Doe",
    "password": "mySecurePass123",
    "device_id": "device_001"
  }'
```

---

### 2. User Login

**Endpoint:** `POST /api/login`

**Description:** Authenticate user and receive access & refresh tokens

**Authentication:** Not required

**Request Headers:**
```
Content-Type: application/json
```

**Request Body:**
```json
{
  "phone": "1234567890",           // Required: User's phone number
  "password": "mySecurePass123",   // Required: User's password
  "device_info": "iPhone 13 iOS"   // Optional: Device information for token management
}
```

**Success Response (200 OK):**
```json
{
  "status": "success",
  "message": "Login successful",
  "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "refresh_token": "abc123def456...",
  "token_type": "Bearer",
  "expires_in": 3600,              // Access token expiration in seconds
  "user": {
    "id": 1,
    "username": "John Doe",
    "device_id": "device_001"
  }
}
```

**Error Responses:**
- `400 Bad Request` - Missing required fields or invalid JSON
- `404 Not Found` - User not found (phone number doesn't exist)
- `401 Unauthorized` - Incorrect password

**Example cURL:**
```bash
curl -X POST http://13.203.114.29/api/login \
  -H "Content-Type: application/json" \
  -d '{
    "phone": "1234567890",
    "password": "mySecurePass123",
    "device_info": "iPhone 13 iOS"
  }'
```

---

### 3. Refresh Access Token

**Endpoint:** `POST /api/token/refresh`

**Description:** Obtain a new access token using refresh token

**Authentication:** Not required (but requires valid refresh token in body)

**Request Headers:**
```
Content-Type: application/json
```

**Request Body:**
```json
{
  "refresh_token": "abc123def456..."  // Required: Valid refresh token from login
}
```

**Success Response (200 OK):**
```json
{
  "status": "success",
  "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

**Error Responses:**
- `400 Bad Request` - Missing refresh token
- `401 Unauthorized` - Invalid, expired, or revoked refresh token

**Example cURL:**
```bash
curl -X POST http://13.203.114.29/api/token/refresh \
  -H "Content-Type: application/json" \
  -d '{
    "refresh_token": "abc123def456..."
  }'
```

---

### 4. Logout

**Endpoint:** `POST /api/logout`

**Description:** Revoke refresh token and log out user

**Authentication:** Required (Bearer token)

**Request Headers:**
```
Content-Type: application/json
Authorization: Bearer <access_token>
```

**Request Body:**
```json
{
  "refresh_token": "abc123def456..."  // Required: Refresh token to revoke
}
```

**Success Response (200 OK):**
```json
{
  "status": "success",
  "message": "Logged out successfully"
}
```

**Error Responses:**
- `400 Bad Request` - Invalid or missing refresh token
- `401 Unauthorized` - Missing or invalid access token

**Example cURL:**
```bash
curl -X POST http://13.203.114.29/api/logout \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..." \
  -d '{
    "refresh_token": "abc123def456..."
  }'
```

---

### 5. Get User Profile

**Endpoint:** `GET /api/users`

**Description:** Retrieve user profile information by phone number

**Authentication:** Required (Bearer token)

**Request Headers:**
```
Authorization: Bearer <access_token>
```

**Query Parameters:**
- `phone` (required): User's phone number

**Success Response (200 OK):**
```json
{
  "id": 1,
  "phone": "1234567890",
  "username": "John Doe",
  "device_id": "device_001",
  "ssid": "MyWiFi",
  "config": {
    "rooms": [
      {
        "id": "room1",
        "name": "Living Room",
        "devices": [
          {
            "id": "dev1",
            "name": "Smart Light",
            "topic": "home/livingroom/light"
          }
        ]
      }
    ]
  },
  "created_at": "2025-10-15T10:30:00",
  "updated_at": "2025-10-15T12:45:00"
}
```

**Error Responses:**
- `400 Bad Request` - Missing phone query parameter
- `401 Unauthorized` - Missing or invalid access token
- `404 Not Found` - User not found

**Example cURL:**
```bash
curl -X GET "http://13.203.114.29/api/users?phone=1234567890" \
  -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
```

---

### 6. Update User Profile

**Endpoint:** `PUT /api/users`

**Description:** Update user profile information (partial update supported)

**Authentication:** Required (Bearer token)

**Request Headers:**
```
Content-Type: application/json
Authorization: Bearer <access_token>
```

**Request Body:**
```json
{
  "phone": "1234567890",              // Required: User's phone number (identifier)
  "username": "Jane Doe",             // Optional: New username
  "password": "newPassword123",       // Optional: New password (will be hashed)
  "device_id": "device_002",          // Optional: New device ID
  "ssid": "MyNewWiFi",                // Optional: WiFi SSID
  "ssid_password": "wifiPass123",     // Optional: WiFi password
  "config": {                         // Optional: IoT configuration (JSON object)
    "rooms": [
      {
        "id": "room1",
        "name": "Living Room",
        "devices": [
          {
            "id": "dev1",
            "name": "Smart Light",
            "topic": "home/livingroom/light"
          }
        ]
      }
    ]
  }
}
```

**Success Response (200 OK):**
```json
{
  "status": "ok",
  "message": "Update successful"
}
```

**Error Responses:**
- `400 Bad Request` - Missing phone field or invalid JSON
- `401 Unauthorized` - Missing or invalid access token
- `404 Not Found` - User not found

**Example cURL:**
```bash
curl -X PUT http://13.203.114.29/api/users \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..." \
  -d '{
    "phone": "1234567890",
    "username": "Jane Doe",
    "ssid": "MyNewWiFi",
    "ssid_password": "wifiPass123"
  }'
```

---

### 7. Delete User

**Endpoint:** `DELETE /api/users`

**Description:** Delete user account permanently

**Authentication:** Required (Bearer token)

**Request Headers:**
```
Content-Type: application/json
Authorization: Bearer <access_token>
```

**Request Body:**
```json
{
  "phone": "1234567890"  // Required: Phone number of user to delete
}
```

**Success Response (200 OK):**
```json
{
  "status": "ok",
  "deleted_phone": "1234567890"
}
```

**Error Responses:**
- `400 Bad Request` - Missing phone field
- `401 Unauthorized` - Missing or invalid access token
- `404 Not Found` - User not found

**Example cURL:**
```bash
curl -X DELETE http://13.203.114.29/api/users \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..." \
  -d '{
    "phone": "1234567890"
  }'
```

---

## Error Responses

All error responses follow this general format:

```json
{
  "status": "error",
  "message": "Description of the error"
}
```

### Common Error Messages

**Authentication Errors (401):**
```json
{
  "status": "error",
  "message": "Authentication token is missing"
}
```
```json
{
  "status": "error",
  "message": "Invalid authorization header format. Use: Bearer <token>"
}
```
```json
{
  "status": "error",
  "message": "Token has expired. Please login again."
}
```
```json
{
  "status": "error",
  "message": "Invalid token. Please login again."
}
```

**Validation Errors (400):**
```json
{
  "status": "error",
  "message": "Expected JSON body."
}
```
```json
{
  "status": "error",
  "message": "Missing fields: phone, password"
}
```

---

## Status Codes

| Status Code | Description |
|------------|-------------|
| 200 | OK - Request successful |
| 201 | Created - Resource created successfully |
| 400 | Bad Request - Invalid request format or missing fields |
| 401 | Unauthorized - Authentication failed or token invalid/expired |
| 404 | Not Found - Resource not found |
| 500 | Internal Server Error - Server encountered an error |

---

## Token Management Best Practices

### For Frontend Developers

1. **Store Tokens Securely:**
   - **Access Token:** Can be stored in memory or sessionStorage (short-lived)
   - **Refresh Token:** Store in secure storage (HttpOnly cookies recommended, or secure mobile storage)

2. **Handle Token Expiration:**
   ```javascript
   // Pseudo-code example
   async function makeAuthenticatedRequest(url, options) {
     let accessToken = getAccessToken();

     try {
       const response = await fetch(url, {
         ...options,
         headers: {
           ...options.headers,
           'Authorization': `Bearer ${accessToken}`
         }
       });

       if (response.status === 401) {
         // Token expired, try to refresh
         const newAccessToken = await refreshAccessToken();

         // Retry original request with new token
         return fetch(url, {
           ...options,
           headers: {
             ...options.headers,
             'Authorization': `Bearer ${newAccessToken}`
           }
         });
       }

       return response;
     } catch (error) {
       console.error('Request failed:', error);
       throw error;
     }
   }

   async function refreshAccessToken() {
     const refreshToken = getRefreshToken();

     const response = await fetch('http://13.203.114.29/api/token/refresh', {
       method: 'POST',
       headers: {
         'Content-Type': 'application/json'
       },
       body: JSON.stringify({ refresh_token: refreshToken })
     });

     if (!response.ok) {
       // Refresh token expired or invalid, redirect to login
       redirectToLogin();
       throw new Error('Session expired');
     }

     const data = await response.json();
     saveAccessToken(data.access_token);
     return data.access_token;
   }
   ```

3. **Logout Flow:**
   - Call `/api/logout` endpoint with refresh token
   - Clear all stored tokens
   - Redirect to login page

4. **Error Handling:**
   - Always check response status codes
   - Handle 401 errors by attempting token refresh
   - Handle 404 errors with user-friendly messages
   - Handle network errors gracefully

---

## Configuration Object Structure

The `config` field in user profiles stores IoT device configuration as JSON:

```json
{
  "rooms": [
    {
      "id": "string",           // Unique room identifier
      "name": "string",         // Room display name
      "devices": [
        {
          "id": "string",       // Unique device identifier
          "name": "string",     // Device display name
          "topic": "string",    // MQTT topic for device
          "type": "string"      // Device type (optional)
        }
      ]
    }
  ]
}
```

---

## Testing Endpoints

You can use tools like **Postman**, **Insomnia**, or **cURL** to test these endpoints.

### Example Testing Flow:

1. **Register a user:**
   ```bash
   curl -X POST http://13.203.114.29/api/users \
     -H "Content-Type: application/json" \
     -d '{"phone":"1234567890","username":"Test User","password":"test123"}'
   ```

2. **Login:**
   ```bash
   curl -X POST http://13.203.114.29/api/login \
     -H "Content-Type: application/json" \
     -d '{"phone":"1234567890","password":"test123"}'
   ```

3. **Use the access_token from step 2 for authenticated requests:**
   ```bash
   curl -X GET "http://13.203.114.29/api/users?phone=1234567890" \
     -H "Authorization: Bearer <access_token_from_step_2>"
   ```

---

## Support

For questions or issues, please contact the backend development team.

**Admin Panel:** http://13.203.114.29/admin/ (requires admin credentials)
