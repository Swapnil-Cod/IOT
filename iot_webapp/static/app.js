// Global state
let currentRoom = null;
let devicesData = {};
let deviceStates = {};
let roomEnvData = {};

// Room icons mapping
const roomIcons = {
  'bedroom': '🛏️',
  'living': '🛋️',
  'livingroom': '🛋️',
  'kitchen': '🍳',
  'bathroom': '🚿',
  'garage': '🚗',
  'office': '💼',
  'dining': '🍽️',
  'guest': '🚪',
  'default': '🏠'
};

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', function() {
  // Load initial data from server
  if (window.APP_DATA) {
    devicesData = window.APP_DATA.devicesData || {};
    deviceStates = window.APP_DATA.deviceStates || {};
  }

  // Check if room parameter is present in URL
  const urlParams = new URLSearchParams(window.location.search);
  const roomParam = urlParams.get('room');
  if (roomParam && devicesData[roomParam]) {
    // Auto-navigate to the specified room
    showRoom(roomParam);
  }

  // Back button handler
  const backBtn = document.getElementById('backBtn');
  if (backBtn) {
    backBtn.addEventListener('click', goBack);
  }

  // Auto-refresh device states and environment every 5 seconds
  setInterval(refreshDeviceStates, 5000);
  setInterval(refreshEnvironment, 5000);
  setInterval(refreshRoomEnvironment, 5000);
  setInterval(refreshSystemStatus, 10000);
});

// Helper functions
function getRoomIcon(roomName) {
  return roomIcons[roomName.toLowerCase()] || roomIcons['default'];
}

function getDeviceIcon(kind) {
  const icons = {
    'light': '💡',
    'fan': '🌀',
    'ac': '❄️',
    'tv': '📺',
    'socket': '🔌',
    'heater': '🔥',
    'camera': '📷',
    'speaker': '🔊',
    'others': '⚙️'
  };
  return icons[kind] || '🔌';
}

function addDeviceToRoom() {
  if (currentRoom) {
    // Redirect to setup page with room pre-selected
    window.location.href = `/setup?room=${encodeURIComponent(currentRoom)}`;
  }
}

// Navigation functions
function showRoom(roomName) {
  currentRoom = roomName;

  // Update header
  document.getElementById('pageTitle').textContent = roomName.charAt(0).toUpperCase() + roomName.slice(1);
  document.getElementById('backBtn').classList.add('show');

  // Update room info banner
  document.getElementById('roomInfoIcon').textContent = getRoomIcon(roomName);
  document.getElementById('roomInfoName').textContent = roomName.charAt(0).toUpperCase() + roomName.slice(1);

  const devices = devicesData[roomName] || [];
  document.getElementById('roomInfoCount').textContent = `${devices.length} ${devices.length === 1 ? 'device' : 'devices'}`;

  // Show room environment if available
  updateRoomEnvironment(roomName);

  // Show devices
  renderDevices(devices);

  // Toggle views
  document.getElementById('homeView').classList.add('hidden');
  document.getElementById('roomView').classList.add('active');
}

function updateRoomEnvironment(roomName) {
  const envData = roomEnvData[roomName];

  // Always show the card (don't hide it)
  const envCard = document.getElementById('roomEnvCard');
  if (envCard) {
    envCard.style.display = 'block';
  }

  // Update temperature
  const tempValue = document.getElementById('roomTempValue');
  if (tempValue) {
    if (envData && envData.temperature != null) {
      tempValue.textContent = envData.temperature.toFixed(1);
    } else {
      tempValue.textContent = '—';
    }
  }

  // Update humidity
  const humValue = document.getElementById('roomHumValue');
  if (humValue) {
    if (envData && envData.humidity != null) {
      humValue.textContent = envData.humidity.toFixed(1);
    } else {
      humValue.textContent = '—';
    }
  }

  // Update timestamp
  const lastUpdated = document.getElementById('roomLastUpdated');

  if (envData && envData.ts_str) {
    if (lastUpdated) {
      lastUpdated.innerHTML = `Last updated: ${envData.ts_str}`;
    }
  } else {
    if (lastUpdated) {
      lastUpdated.innerHTML = `<span style="color: #dc3545;">No sensor data</span>`;
    }
  }
}

function goBack() {
  currentRoom = null;
  document.getElementById('pageTitle').textContent = 'My Home';
  document.getElementById('backBtn').classList.remove('show');
  document.getElementById('homeView').classList.remove('hidden');
  document.getElementById('roomView').classList.remove('active');
}

// Render functions
function renderDevices(devices) {
  const devicesList = document.getElementById('devicesList');

  if (devices.length === 0) {
    devicesList.innerHTML = `
      <div class="empty-state">
        <div class="empty-state-icon">📦</div>
        <h3>No devices yet</h3>
        <p>Add a device to this room</p>
      </div>
    `;
    return;
  }

  let html = '<div class="devices-grid">';
  devices.forEach(device => {
    const state = deviceStates[device.id] || 'UNKNOWN';
    const isOn = state === 'ON';
    const switchLabel = device.switch ? `<div class="device-switch">Switch: ${device.switch}</div>` : '';

    html += `
      <div class="device-card">
        <div class="device-info">
          <span class="device-icon">${getDeviceIcon(device.kind)}</span>
          <div class="device-details">
            <h4>${device.name}</h4>
            <div class="device-state">State: ${state}</div>
            ${switchLabel}
          </div>
        </div>
        <div class="device-controls">
          <button class="toggle-btn ${isOn ? '' : 'off'}" onclick="toggleDevice('${device.id}')">
            ${isOn ? 'ON' : 'OFF'}
          </button>
          <button class="delete-icon" onclick="deleteDevice('${device.id}', '${device.name}')" title="Delete device">
            🗑️
          </button>
        </div>
      </div>
    `;
  });
  html += '</div>';

  devicesList.innerHTML = html;
}

// Device control functions
async function toggleDevice(deviceId) {
  try {
    const response = await fetch(`/device/${deviceId}/toggle`, {
      method: 'POST'
    });

    if (response.ok) {
      // Refresh device states
      await refreshDeviceStates();
    }
  } catch (error) {
    console.error('Error toggling device:', error);
  }
}

async function deleteDevice(deviceId, deviceName) {
  if (!confirm(`Delete ${deviceName}?`)) return;

  try {
    const response = await fetch(`/device/${deviceId}`, {
      method: 'DELETE',
      headers: { 'Content-Type': 'application/json' }
    });

    const data = await response.json();

    if (data.success) {
      // Reload page to refresh data
      window.location.reload();
    } else {
      alert(data.message || 'Failed to delete device');
    }
  } catch (error) {
    console.error('Error deleting device:', error);
    alert('Network error. Please try again.');
  }
}

// Refresh functions
async function refreshDeviceStates() {
  try {
    const response = await fetch('/api/devices');
    const data = await response.json();

    deviceStates = data.state;
    devicesData = {};

    // Rebuild devices by room
    data.devices.forEach(device => {
      if (!devicesData[device.room]) {
        devicesData[device.room] = [];
      }
      devicesData[device.room].push(device);
    });

    // Re-render current room if viewing one
    if (currentRoom) {
      renderDevices(devicesData[currentRoom] || []);
    }
  } catch (error) {
    console.error('Error refreshing device states:', error);
  }
}

async function refreshEnvironment() {
  try {
    const response = await fetch('/api/env');
    const data = await response.json();

    // Update temperature
    const tempElement = document.getElementById('tempValue');
    if (tempElement) {
      tempElement.textContent = data.temperature != null ? data.temperature.toFixed(1) : '—';
    }

    // Update humidity
    const humElement = document.getElementById('humValue');
    if (humElement) {
      humElement.textContent = data.humidity != null ? data.humidity.toFixed(1) : '—';
    }

    // Update last updated timestamp
    const lastUpdatedElement = document.getElementById('lastUpdated');
    if (lastUpdatedElement && data.ts_str) {
      lastUpdatedElement.textContent = `Last updated: ${data.ts_str}`;
    }
  } catch (error) {
    console.error('Error refreshing environment:', error);
  }
}

async function refreshRoomEnvironment() {
  try {
    const response = await fetch('/api/env/rooms');
    const data = await response.json();

    roomEnvData = data;

    // If currently viewing a room, update its environment display
    if (currentRoom) {
      updateRoomEnvironment(currentRoom);
    }
  } catch (error) {
    console.error('Error refreshing room environment:', error);
  }
}

async function refreshSystemStatus() {
  try {
    const response = await fetch('/api/status');
    const data = await response.json();

    // Update broker status
    const brokerElement = document.getElementById('brokerStatus');
    if (brokerElement) {
      if (data.broker_connected) {
        brokerElement.className = 'status-pill online';
        brokerElement.textContent = 'Connected';
      } else {
        brokerElement.className = 'status-pill offline';
        brokerElement.textContent = 'Disconnected';
      }
    }

    // Update device status
    const deviceElement = document.getElementById('deviceStatus');
    if (deviceElement) {
      if (data.device_online === 'online') {
        deviceElement.className = 'status-pill online';
        deviceElement.textContent = 'Online';
      } else if (data.device_online === 'offline') {
        deviceElement.className = 'status-pill offline';
        deviceElement.textContent = 'Offline';
      } else {
        deviceElement.className = 'status-pill unknown';
        deviceElement.textContent = 'Unknown';
      }
    }
  } catch (error) {
    console.error('Error refreshing system status:', error);
  }
}
