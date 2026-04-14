import json
import os
import threading
from time import time
from typing import Dict, List
import requests
import uuid

from flask import Flask, render_template, request, redirect, url_for, jsonify, flash, session
from flask_cors import CORS
import paho.mqtt.client as mqtt
import logging

app = Flask(__name__)
CORS(app)  # Enable CORS for all routes

# Set logging level to WARNING (only show important messages)
logging.basicConfig(level=logging.WARNING)
app.logger.setLevel(logging.WARNING)

# Load secrets from secrets.json
try:
    with open("secrets.json") as f:
        secrets = json.load(f)
    print("✓ Secrets loaded")
except FileNotFoundError:
    print("✗ ERROR: secrets.json not found! Please create it from secrets.example.json")
    raise
except json.JSONDecodeError as e:
    print(f"✗ ERROR: Invalid JSON in secrets.json: {e}")
    raise

# Flask configuration
app.secret_key = secrets.get("flask", {}).get("secret_key", "change-me-insecure-default")

# Warn if using default secret key
if app.secret_key == "change-me-insecure-default":
    print("⚠ WARNING: Using default secret key! Change it in secrets.json for production!")

# External API base URL
API_BASE_URL = secrets.get("api", {}).get("base_url", "http://13.203.114.29")

# MQTT broker config
mqtt_config = secrets.get("mqtt", {})
MQTT_BROKER = mqtt_config.get("broker", "localhost")
MQTT_PORT = mqtt_config.get("port", 1883)
MQTT_USERNAME = mqtt_config.get("username", "")
MQTT_PASSWORD = mqtt_config.get("password", "")
# Generate unique client ID to avoid conflicts
MQTT_CLIENT_ID = f"{mqtt_config.get('client_id', 'flask-mqtt')}_{uuid.uuid4().hex[:8]}"

# -----------------------------
# Shared state + config
# -----------------------------
state_lock = threading.Lock()
# Per-room environmental state: room_id -> {temperature, humidity, ts, ts_str}
room_env_state: Dict[str, Dict] = {}
device_state: Dict[str, str] = {}  # device_id -> "ON"/"OFF"/"UNKNOWN"
broker_connected = False  # Track MQTT broker connection status
device_online_status = "unknown"  # Track device online/offline status

# Default config shape
config = {
    "rooms": [],  # Now includes: {name, esp32_mac, num_switches, ssid}
    "devices": [],  # Now includes: esp32_mac field
    "sensors": [],  # Now includes: {room, topic: "homi/<mac>/sensor", mac}
    "switches": {},  # Switch assignments: {room_switch: device_id}
    "esp32_devices": {}  # ESP32 mapping: {mac: {room, num_switches}}
}

# -----------------------------
# API Helper Functions for Config
# -----------------------------
def refresh_access_token():
    """Refresh the access token using the refresh token"""
    if 'refresh_token' not in session:
        return False

    try:
        response = requests.post(
            f"{API_BASE_URL}/api/token/refresh",
            json={"refresh_token": session['refresh_token']},
            headers={"Content-Type": "application/json"},
            timeout=10
        )

        if response.status_code == 200:
            data = response.json()
            # Update access token in session
            session['access_token'] = data.get('access_token')
            print("✓ Access token refreshed successfully")
            return True
        else:
            print(f"✗ Token refresh failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Token refresh error: {e}")
        return False

def load_config_from_session():
    """Load config from API based on logged-in user"""
    global config
    if 'phone' in session and 'access_token' in session:
        try:
            headers = {
                "Authorization": f"Bearer {session['access_token']}",
                "Content-Type": "application/json"
            }
            response = requests.get(
                f"{API_BASE_URL}/api/users",
                params={"phone": session['phone']},
                headers=headers,
                timeout=10
            )

            # Handle token expiration with automatic refresh
            if response.status_code == 401:
                print("⚠ Access token expired, attempting refresh...")
                if refresh_access_token():
                    # Retry request with new access token
                    headers["Authorization"] = f"Bearer {session['access_token']}"
                    response = requests.get(
                        f"{API_BASE_URL}/api/users",
                        params={"phone": session['phone']},
                        headers=headers,
                        timeout=10
                    )
                else:
                    # Refresh failed, clear session
                    print("✗ Token refresh failed, clearing session")
                    session.clear()
                    return

            if response.status_code == 200:
                user_data = response.json()

                # API returns user data directly (not wrapped in 'user' key)
                user_config = user_data.get('config')

                if user_config:
                    # Config is already a dict/object from the API
                    if isinstance(user_config, str):
                        # If it comes as string, parse it
                        config = json.loads(user_config)
                    else:
                        config = user_config

                    # Ensure all required fields exist with defaults
                    config.setdefault("rooms", [])
                    config.setdefault("devices", [])
                    config.setdefault("sensors", [])
                    config.setdefault("switches", {})
                    config.setdefault("esp32_devices", {})

                    # Migration: Fix uppercase ON/OFF to lowercase on/off in existing devices
                    devices_updated = False
                    for device in config.get("devices", []):
                        if device.get("payload_on") == "ON":
                            device["payload_on"] = "on"
                            devices_updated = True
                        if device.get("payload_off") == "OFF":
                            device["payload_off"] = "off"
                            devices_updated = True

                    if devices_updated:
                        # Save the updated config back to the database
                        save_config()

                    # Handle legacy "env_topic" vs "env_topic_json"
                    if "env_topic" in config and "env_topic_json" not in config:
                        config["env_topic_json"] = config["env_topic"]

                    with state_lock:
                        for d in config.get("devices", []):
                            device_state.setdefault(d["id"], "UNKNOWN")

                    # Re-subscribe to topics after loading config
                    if broker_connected:
                        subscribe_all()

                    return
            else:
                print(f"✗ API error {response.status_code}")
        except Exception as e:
            print(f"✗ Config load error: {e}")
    config = {
        "rooms": [],
        "devices": [],
        "sensors": [],
        "switches": {},
        "esp32_devices": {}
    }

def save_config():
    """Save config to API for logged-in user"""
    if 'phone' in session and 'access_token' in session:
        try:
            headers = {
                "Content-Type": "application/json",
                "Authorization": f"Bearer {session['access_token']}"
            }

            # API expects phone and config in the body
            payload = {
                "phone": session['phone'],
                "config": config
            }

            response = requests.put(
                f"{API_BASE_URL}/api/users",
                json=payload,
                headers=headers,
                timeout=10
            )

            # Handle token expiration with automatic refresh
            if response.status_code == 401:
                print("⚠ Access token expired during save, attempting refresh...")
                if refresh_access_token():
                    # Retry request with new access token
                    headers["Authorization"] = f"Bearer {session['access_token']}"
                    response = requests.put(
                        f"{API_BASE_URL}/api/users",
                        json=payload,
                        headers=headers,
                        timeout=10
                    )
                else:
                    print("✗ Token refresh failed during save")
                    session.clear()
                    return False

            if response.status_code == 200:
                return True
            else:
                print(f"✗ Save error {response.status_code}")
                return False
        except Exception as e:
            print(f"✗ Config save error: {e}")
            return False
    return False

# -----------------------------
# MQTT Setup
# -----------------------------
# -----------------------------
# MQTT client
# -----------------------------
mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID, clean_session=True)
if MQTT_USERNAME:
    mqtt_client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD or None)

def subscribe_all():
    # Ensure defaults exist
    config.setdefault("rooms", [])
    config.setdefault("devices", [])
    config.setdefault("esp32_devices", {})

    # Subscribe per unique room MAC
    seen_macs = set()
    topic_count = 0
    for r in config.get("rooms", []):
        mac = ""
        if isinstance(r, dict):
            mac = (r.get("esp32_mac") or "").replace(":", "")
        if not mac or mac in seen_macs:
            continue
        seen_macs.add(mac)

        # Sensor topic (env)
        sensor_t = f"homi/{mac}/sensor"
        mqtt_client.subscribe(sensor_t)
        topic_count += 1

        # Switch status topics S1..S4 (ONLY status, NOT cmd - ESP32 handles cmd)
        for sn in ("s1", "s2", "s3", "s4"):
            st = f"homi/{mac}/{sn}/status"
            mqtt_client.subscribe(st)
            topic_count += 1

        # Device online/offline status
        dev_status = f"homi/{mac}/device/status"
        mqtt_client.subscribe(dev_status)
        topic_count += 1

    if topic_count > 0:
        print(f"✓ MQTT: Subscribed to {topic_count} topics")


def on_connect(client, userdata, flags, rc, properties=None):
    global broker_connected
    if rc == 0:
        with state_lock:
            broker_connected = True
        subscribe_all()
        print(f"✓ MQTT: Connected to {MQTT_BROKER}:{MQTT_PORT}")
    else:
        with state_lock:
            broker_connected = False
        print(f"✗ MQTT: Connection failed (rc={rc})")

def on_disconnect(client, userdata, rc, properties=None):
    global broker_connected
    with state_lock:
        broker_connected = False
    if rc != 0:
        print(f"⚠ MQTT: Disconnected (rc={rc})")

def on_message(client, userdata, msg):
    global device_online_status
    topic = msg.topic
    payload = msg.payload.decode("utf-8").strip()

    # Device online/offline: homi/<MAC>/device/status
    if topic.startswith("homi/") and topic.endswith("/device/status"):
        with state_lock:
            device_online_status = payload.lower()
        return

    # Sensor JSON: homi/<MAC>/sensor
    if topic.startswith("homi/") and topic.endswith("/sensor"):
        parts = topic.split("/")
        mac = parts[1] if len(parts) == 3 else ""
        room_name = None

        # Fast lookup via esp32_devices
        if mac and "esp32_devices" in config:
            room_name = (config.get("esp32_devices", {}).get(mac, {}) or {}).get("room")

        # Fallback: scan rooms
        if not room_name:
            for r in config.get("rooms", []):
                if isinstance(r, dict) and (r.get("esp32_mac") or "").replace(":", "") == mac:
                    room_name = r.get("name")
                    break

        if room_name:
            try:
                data = json.loads(payload)
                with state_lock:
                    rm = room_env_state.setdefault(room_name, {})
                    if "temperature" in data:
                        rm["temperature"] = float(data["temperature"])
                    if "humidity" in data:
                        rm["humidity"] = float(data["humidity"])
                    rm["ts_str"] = data.get("timestamp")
                    rm["ts"] = time()
            except Exception:
                pass
        return

    # Switch status: homi/<MAC>/s1..s4/status
    if topic.startswith("homi/") and topic.endswith("/status"):
        parts = topic.split("/")
        if len(parts) == 4 and parts[2].startswith("s") and parts[2][1:].isdigit():
            mac = parts[1]
            switch = parts[2].upper()  # S1..S4
            room_name = None

            # Lookup via esp32_devices first
            if mac and "esp32_devices" in config:
                room_name = (config.get("esp32_devices", {}).get(mac, {}) or {}).get("room")

            # Fallback scan
            if not room_name:
                for r in config.get("rooms", []):
                    if isinstance(r, dict) and (r.get("esp32_mac") or "").replace(":", "") == mac:
                        room_name = r.get("name")
                        break

            if room_name:
                device_id = f"{room_name}_{switch.lower()}"
                with state_lock:
                    device_state[device_id] = payload.upper() if payload else "UNKNOWN"
            return

mqtt_client.on_connect = on_connect
mqtt_client.on_disconnect = on_disconnect
mqtt_client.on_message = on_message

def start_mqtt():
    global broker_connected
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
        mqtt_client.loop_start()
    except Exception as e:
        broker_connected = False
        print(f"✗ MQTT: Failed to connect - {e}")

start_mqtt()

# -----------------------------
# Helpers
# -----------------------------
def find_device(device_id):
    for d in config["devices"]:
        if d["id"] == device_id:
            return d
    return None

def desired_payload_for_device(d: dict, desired_on: bool) -> str:
    # Per-device custom payloads (defaults to "on"/"off")
    return (d.get("payload_on") if desired_on else d.get("payload_off")) or ("on" if desired_on else "off")

# -----------------------------
# Auth Routes
# -----------------------------
@app.route("/register", methods=["GET", "POST"])
def register():
    if request.method == "POST":
        phone = request.form.get("phone", "").strip()
        username = request.form.get("username", "").strip()
        password = request.form.get("password", "").strip()
        device_id = request.form.get("device_id", "").strip()

        if not all([phone, username, password]):
            flash("Phone, username, and password are required", "error")
            return redirect(url_for("register"))

        # Register via external API
        try:
            payload = {
                "phone": phone,
                "username": username,
                "password": password
            }

            # Only include device_id if provided
            if device_id:
                payload["device_id"] = device_id

            # Initialize with default config
            payload["config"] = {
                "rooms": [],
                "devices": [],
                "sensors": [],
                "switches": {},
                "esp32_devices": {}
            }

            response = requests.post(
                f"{API_BASE_URL}/api/users",
                json=payload,
                headers={"Content-Type": "application/json"},
                timeout=10
            )

            if response.status_code in [200, 201]:
                data = response.json()
                flash("Registration successful! Please login.", "success")
                return redirect(url_for("login"))
            else:
                data = response.json()
                flash(data.get("message", data.get("status", "Registration failed")), "error")
                return redirect(url_for("register"))
        except Exception:
            flash("Network error. Please try again.", "error")
            return redirect(url_for("register"))

    return render_template("register.html")

@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        phone = request.form.get("phone", "").strip()
        password = request.form.get("password", "").strip()

        if not phone or not password:
            flash("Phone and password are required", "error")
            return redirect(url_for("login"))

        # Login via external API
        try:
            response = requests.post(
                f"{API_BASE_URL}/api/login",
                json={
                    "phone": phone,
                    "password": password,
                    "device_info": "IoT Web Dashboard"  # Optional: helps track devices
                },
                headers={"Content-Type": "application/json"},
                timeout=10
            )

            if response.status_code == 200:
                data = response.json()

                # Check for success status
                if data.get('status') == 'success':
                    user_data = data.get('user', {})

                    # Store user info and NEW JWT tokens in session
                    session['phone'] = phone
                    session['username'] = user_data.get('username', '')
                    session['access_token'] = data.get('access_token')  # NEW: Store access token
                    session['refresh_token'] = data.get('refresh_token')  # NEW: Store refresh token
                    session['logged_in'] = True

                    flash("Login successful!", "success")
                    return redirect(url_for("index"))
                else:
                    flash(data.get("message", "Login failed"), "error")
                    return redirect(url_for("login"))
            else:
                data = response.json()
                flash(data.get("message", data.get("status", "Login failed")), "error")
                return redirect(url_for("login"))
        except Exception:
            flash("Network error. Please try again.", "error")
            return redirect(url_for("login"))

    return render_template("login.html")

@app.route("/logout")
def logout():
    # Call logout endpoint to revoke refresh token
    if 'access_token' in session and 'refresh_token' in session:
        try:
            requests.post(
                f"{API_BASE_URL}/api/logout",
                json={"refresh_token": session['refresh_token']},
                headers={
                    "Content-Type": "application/json",
                    "Authorization": f"Bearer {session['access_token']}"
                },
                timeout=10
            )
        except Exception as e:
            print(f"⚠ Logout API call failed: {e}")
            # Continue with local session clearing even if API call fails

    session.clear()
    flash("Logged out successfully", "success")
    return redirect(url_for("login"))

# -----------------------------
# Routes
# -----------------------------
@app.get("/")
def index():
    # Check if user is logged in
    if 'logged_in' not in session:
        return redirect(url_for("login"))

    # Load config from database for logged-in user
    load_config_from_session()

    with state_lock:
        # For backward compatibility, get data from first room or global
        # Handle both string and dict room formats
        rooms_list = config.get("rooms", [])
        if rooms_list:
            first_room = rooms_list[0]
            default_room = first_room.get("name") if isinstance(first_room, dict) else first_room
        else:
            default_room = "global"

        default_env = room_env_state.get(default_room, {})
        temp = default_env.get("temperature")
        hum = default_env.get("humidity")
        ts_str = default_env.get("ts_str")
        dev_states = dict(device_state)
        online_status = device_online_status
        room_env_copy = dict(room_env_state)

    # Extract room names and group devices by room
    # Handle both old format (string) and new format (dict)
    room_names = []
    for room in config["rooms"]:
        if isinstance(room, dict):
            room_names.append(room.get("name", ""))
        else:
            room_names.append(room)

    by_room: Dict[str, List[dict]] = {r: [] for r in room_names}
    for d in config["devices"]:
        by_room.setdefault(d["room"], []).append(d)

    return render_template(
        "index.html",
        rooms=room_names,
        by_room=by_room,
        device_state=dev_states,
        room_env_state=room_env_copy,
        temp=temp,
        hum=hum,
        ts_str=ts_str,
        broker_connected=broker_connected,
        device_online=online_status,
        username=session.get('username', '')
    )

@app.post("/device/<device_id>/<action>")
def set_device(device_id, action):
    d = find_device(device_id)
    if not d:
        return "Unknown device", 404

    action_up = action.upper()
    if action_up not in {"ON", "OFF", "TOGGLE"}:
        return "Invalid action", 400

    # Figure out desired ON/OFF
    if action_up == "TOGGLE":
        with state_lock:
            current = device_state.get(device_id, "UNKNOWN")
        desired_on = (current != "ON")
    else:
        desired_on = (action_up == "ON")

    payload = desired_payload_for_device(d, desired_on)
    topic = (d.get("cmd_topic") or "").strip()
    if not topic:
        return "Device has no cmd_topic", 400

    mqtt_client.publish(topic, payload, qos=1, retain=False)

    # Optimistic update (for devices without a state_topic)
    if d.get("optimistic"):
        with state_lock:
            device_state[d["id"]] = "ON" if desired_on else "OFF"

    return redirect(url_for("index"))

@app.get("/api/env")
def api_env():
    """Get environment data (legacy - returns first room or global)"""
    with state_lock:
        # Handle both string and dict room formats
        rooms_list = config.get("rooms", [])
        if rooms_list:
            first_room = rooms_list[0]
            default_room = first_room.get("name") if isinstance(first_room, dict) else first_room
        else:
            default_room = "global"

        default_env = room_env_state.get(default_room, {
            "temperature": None,
            "humidity": None,
            "ts": None,
            "ts_str": None
        })
        return jsonify(default_env)

@app.get("/api/env/rooms")
def api_env_rooms():
    """Get environment data for all rooms"""
    with state_lock:
        return jsonify(room_env_state)

@app.get("/api/devices")
def api_devices():
    # Reload config from database to get latest changes
    if 'logged_in' in session:
        load_config_from_session()

    with state_lock:
        return jsonify({"rooms": config["rooms"], "devices": config["devices"], "state": device_state})

@app.get("/api/status")
def api_status():
    with state_lock:
        broker_status = broker_connected
        device_status = device_online_status

    return jsonify({
        "broker_connected": broker_status,
        "device_online": device_status,
        "rooms_count": len(config["rooms"]),
        "devices_count": len(config["devices"])
    })

# -----------------------------
# Device Setup (Alexa-style)
# -----------------------------
@app.get("/setup")
def setup():
    # Check if user is logged in
    if 'logged_in' not in session:
        return redirect(url_for("login"))

    # Load config from database for logged-in user
    load_config_from_session()

    # Extract room names from config (handle both string and dict formats)
    room_list = []
    for room in config.get("rooms", []):
        if isinstance(room, dict):
            room_list.append(room.get("name", ""))
        else:
            room_list.append(room)

    return render_template("setup.html", rooms=room_list)

@app.get("/rooms")
def rooms():
    # Check if user is logged in
    if 'logged_in' not in session:
        return redirect(url_for("login"))

    return render_template("rooms.html")

@app.post("/setup/device")
def setup_device():
    if 'logged_in' not in session:
        return jsonify({"success": False, "message": "Not logged in"}), 401

    load_config_from_session()

    data = request.get_json()
    device_name = (data.get("name") or "").strip()
    device_type = (data.get("type") or "").strip().lower()  # light | fan | ac | tv | socket | heater | camera | speaker | others
    room_name = (data.get("room") or "").strip().lower()
    switch_number = (data.get("switch") or "").strip()  # Mandatory: S1, S2, S3, S4

    if not all([device_name, device_type, room_name, switch_number]):
        return jsonify({"success": False, "message": "Name, type, room, and switch number are required"}), 400

    # Validate switch number format
    if switch_number not in ["S1", "S2", "S3", "S4"]:
        return jsonify({"success": False, "message": "Invalid switch number. Must be S1, S2, S3, or S4"}), 400

    # Find room info to get ESP32 MAC address
    room_info = next((r for r in config["rooms"] if (isinstance(r, dict) and r.get("name") == room_name) or (isinstance(r, str) and r == room_name)), None)

    if not room_info:
        return jsonify({"success": False, "message": "Room not found. Please create the room first."}), 404

    # Get ESP32 MAC from room (handle both old string format and new dict format)
    esp32_mac = room_info.get("esp32_mac", "") if isinstance(room_info, dict) else ""

    if not esp32_mac:
        return jsonify({"success": False, "message": "Room does not have an ESP32 device configured"}), 400

    # Auto-generate device ID
    device_id = f"{room_name}_{switch_number.lower()}"

    # Check if device with same name already exists in room
    if any(d["name"] == device_name and d["room"] == room_name for d in config["devices"]):
        return jsonify({"success": False, "message": "Device with this name already exists in this room"}), 400

    # Generate MQTT topics based on ESP32 MAC and switch number
    # Format: homi/<mac>/s1/cmd and homi/<mac>/s1/status
    mac_clean = esp32_mac.replace(":", "")  # Remove colons from MAC
    switch_num = switch_number.lower()  # s1, s2, s3, s4
    cmd_topic = f"homi/{mac_clean}/{switch_num}/cmd"
    state_topic = f"homi/{mac_clean}/{switch_num}/status"

    # Initialize switches dict if not exists (room-based)
    if "switches" not in config:
        config["switches"] = {}

    # Rebuild switches mapping from existing devices to ensure consistency
    # This ensures that if config is loaded without switches dict, we rebuild it
    temp_switches = {}
    for device in config.get("devices", []):
        if device.get("room") and device.get("switch"):
            room_switch_key = f"{device['room']}_{device['switch']}"
            temp_switches[room_switch_key] = device["id"]
    config["switches"] = temp_switches

    # Check if switch is already assigned IN THIS ROOM
    # Key format: "room_name_S1" (e.g., "bedroom_S1", "kitchen_S1")
    room_switch_key = f"{room_name}_{switch_number}"

    if room_switch_key in config["switches"]:
        existing_device_id = config["switches"][room_switch_key]
        # Find the existing device name
        existing_device = next((d for d in config["devices"] if d["id"] == existing_device_id), None)
        existing_name = existing_device["name"] if existing_device else "another device"
        return jsonify({
            "success": False,
            "message": f"Switch {switch_number} is already assigned to '{existing_name}' in this room"
        }), 400

    # Add device to config
    new_device = {
        "id": device_id,
        "name": device_name,
        "room": room_name,
        "kind": device_type,
        "esp32_mac": esp32_mac,
        "cmd_topic": cmd_topic,
        "state_topic": state_topic,
        "payload_on": "on",
        "payload_off": "off",
        "optimistic": False,
        "switch": switch_number
    }

    config["devices"].append(new_device)

    # Store switch assignment in switches config (per-room)
    config["switches"][room_switch_key] = device_id

    with state_lock:
        device_state[device_id] = "UNKNOWN"

    save_config()

    # Subscribe to state topic
    mqtt_client.subscribe(state_topic)

    return jsonify({"success": True, "device": new_device}), 200

@app.post("/setup/esp32/provision")
def provision_esp32():
    """Provision ESP32 device - send WiFi credentials and get MAC + device count"""
    if 'logged_in' not in session:
        return jsonify({"success": False, "message": "Not logged in"}), 401

    data = request.get_json()

    room_name = (data.get("room_name") or "").strip().lower()
    wifi_ssid = (data.get("wifi_ssid") or "").strip()
    wifi_password = (data.get("wifi_password") or "").strip()

    if not all([room_name, wifi_ssid, wifi_password]):
        return jsonify({"success": False, "message": "Room name, SSID, and password are required"}), 400

    try:
        # Connect to ESP32 AP and send WiFi credentials
        esp32_url = "http://192.168.4.1/connect"
        payload = {
            "ssid": wifi_ssid,
            "password": wifi_password
        }

        response = requests.post(esp32_url, json=payload, timeout=15)
        esp32_response = response.json()

        if esp32_response.get("status") == "failed":
            return jsonify({
                "success": False,
                "message": "ESP32 failed to connect to WiFi. Please check credentials."
            }), 400

        if esp32_response.get("status") != "ok":
            return jsonify({
                "success": False,
                "message": "ESP32 returned unexpected response"
            }), 400

        mac_address = esp32_response.get("mac", "").strip()
        num_devices = int(esp32_response.get("devices", 4))

        if not mac_address:
            return jsonify({"success": False, "message": "ESP32 did not return MAC address"}), 400

        print(f"✓ ESP32 provisioned: MAC={mac_address}, Switches={num_devices}")

        # Return ESP32 info to frontend
        return jsonify({
            "success": True,
            "mac": mac_address,
            "num_devices": num_devices,
            "room": room_name
        }), 200

    except requests.exceptions.Timeout:
        return jsonify({
            "success": False,
            "message": "Timeout connecting to ESP32. Make sure you're connected to the ESP32 hotspot."
        }), 500
    except requests.exceptions.ConnectionError:
        return jsonify({
            "success": False,
            "message": "Cannot reach ESP32 at 192.168.4.1. Ensure you're connected to the ESP32's WiFi hotspot."
        }), 500
    except Exception as e:
        return jsonify({"success": False, "message": str(e)}), 500

@app.post("/setup/room")
def setup_room():
    """Create room with ESP32 configuration"""
    if 'logged_in' not in session:
        return jsonify({"success": False, "message": "Not logged in"}), 401

    load_config_from_session()

    data = request.get_json()
    room_name = (data.get("name") or "").strip().lower()
    esp32_mac = (data.get("esp32_mac") or "").strip()
    num_switches = int(data.get("num_switches", 4))
    wifi_ssid = (data.get("wifi_ssid") or "").strip()

    if not room_name:
        return jsonify({"success": False, "message": "Room name required"}), 400

    # Check if room already exists
    existing_room = next((r for r in config["rooms"] if (isinstance(r, str) and r == room_name) or (isinstance(r, dict) and r.get("name") == room_name)), None)
    if existing_room:
        return jsonify({"success": False, "message": "Room already exists"}), 400

    # Create room with ESP32 info
    room_data = {
        "name": room_name,
        "esp32_mac": esp32_mac,
        "num_switches": num_switches,
        "wifi_ssid": wifi_ssid
    }

    config["rooms"].append(room_data)

    # Store ESP32 mapping
    if esp32_mac:
        config["esp32_devices"][esp32_mac] = {
            "room": room_name,
            "num_switches": num_switches
        }

        # Auto-create sensor for this room
        sensor_topic = f"homi/{esp32_mac.replace(':', '')}/sensor"
        config["sensors"].append({
            "room": room_name,
            "topic": sensor_topic,
            "mac": esp32_mac,
            "type": "dht"
        })

    save_config()

    return jsonify({"success": True, "room": room_data}), 200

@app.delete("/room/<room_name>")
def delete_room(room_name):
    if 'logged_in' not in session:
        return jsonify({"success": False, "message": "Not logged in"}), 401

    load_config_from_session()

    room_name = room_name.strip().lower()

    # Check if room has devices
    if any(d["room"] == room_name for d in config["devices"]):
        return jsonify({"success": False, "message": "Cannot delete room with devices. Remove devices first."}), 400

    # Find and remove room (handle both string and dict format)
    room_to_remove = None
    for room in config["rooms"]:
        room_check = room.get("name") if isinstance(room, dict) else room
        if room_check == room_name:
            room_to_remove = room
            break

    if room_to_remove is None:
        return jsonify({"success": False, "message": "Room not found"}), 404

    config["rooms"].remove(room_to_remove)

    # Remove ESP32 mapping if exists
    if isinstance(room_to_remove, dict) and room_to_remove.get("esp32_mac"):
        mac = room_to_remove.get("esp32_mac")
        config.get("esp32_devices", {}).pop(mac, None)

    save_config()

    return jsonify({"success": True}), 200

@app.delete("/device/<device_id>")
def delete_device(device_id):
    if 'logged_in' not in session:
        return jsonify({"success": False, "message": "Not logged in"}), 401

    load_config_from_session()

    d = find_device(device_id)
    if not d:
        return jsonify({"success": False, "message": "Device not found"}), 404

    if d.get("state_topic"):
        try:
            mqtt_client.unsubscribe(d["state_topic"])
        except Exception:
            pass

    # Remove switch assignment if exists (room-based key)
    if "switches" in config and d.get("room") and d.get("switch"):
        room_switch_key = f"{d['room']}_{d['switch']}"
        if room_switch_key in config["switches"]:
            del config["switches"][room_switch_key]

    config["devices"] = [x for x in config["devices"] if x["id"] != device_id]

    with state_lock:
        device_state.pop(device_id, None)

    save_config()

    return jsonify({"success": True}), 200

# -----------------------------
# Dev server
# -----------------------------
if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0", port=int(os.environ.get("PORT", "5000")))
