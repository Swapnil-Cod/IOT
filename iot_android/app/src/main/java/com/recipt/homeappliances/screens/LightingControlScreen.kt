package com.recipt.homeappliances

import android.widget.Toast
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import org.eclipse.paho.client.mqttv3.MqttClient
import org.eclipse.paho.client.mqttv3.MqttException
import org.eclipse.paho.client.mqttv3.MqttMessage
import org.json.JSONObject

@Composable
fun LightingControlScreen(client: MqttClient, isMqttConnected: Boolean) {
    var isLightOn by remember { mutableStateOf(false) }
    var temperature by remember { mutableStateOf("--") }
    var humidity by remember { mutableStateOf("--") }
    val context = LocalContext.current

    // ✅ MQTT callback to handle sensor data
    DisposableEffect(Unit) {
        client.setCallback(object : org.eclipse.paho.client.mqttv3.MqttCallback {
            override fun connectionLost(cause: Throwable?) {}

            override fun messageArrived(topic: String?, message: MqttMessage?) {
                if (topic == "Swap_SensorData" && message != null) {
                    try {
                        val json = JSONObject(message.toString())
                        temperature = json.optString("temperature", "--")
                        humidity = json.optString("humidity", "--")
                    } catch (e: Exception) {
                        e.printStackTrace()
                    }
                }
            }

            override fun deliveryComplete(token: org.eclipse.paho.client.mqttv3.IMqttDeliveryToken?) {}
        })

        onDispose { }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text("Lighting Control", style = MaterialTheme.typography.headlineSmall)
        Spacer(modifier = Modifier.height(16.dp))

        // 🔘 Toggle Switch for Light Control
        Switch(
            checked = isLightOn,
            onCheckedChange = { isChecked ->
                if (isMqttConnected) {
                    isLightOn = isChecked
                    val messageContent = if (isChecked) "on" else "off"
                    publishMqttMessage(client, "Swap_CommandRequest", messageContent)
                } else {
                    Toast.makeText(context, "MQTT not connected", Toast.LENGTH_SHORT).show()
                }
            },
            enabled = isMqttConnected
        )
        Text(text = if (isLightOn) "Light is ON" else "Light is OFF")

        Spacer(modifier = Modifier.height(32.dp))

        // 🔘 Button to fetch sensor data
        Button(
            onClick = {
                if (isMqttConnected) {
                    try {
                        // Subscribe to Swap_SensorData
                        client.subscribe("Swap_SensorData", 2)

                        // Set callback every time we subscribe
                        client.setCallback(object : org.eclipse.paho.client.mqttv3.MqttCallback {
                            override fun connectionLost(cause: Throwable?) {}

                            override fun messageArrived(topic: String?, message: MqttMessage?) {
                                if (topic == "Swap_SensorData" && message != null) {
                                    val payload = message.toString()
                                    try {
                                        // Try JSON parse first
                                        val json = org.json.JSONObject(payload)
                                        temperature = json.optString("temperature", "--")
                                        humidity = json.optString("humidity", "--")
                                    } catch (e: Exception) {
                                        // Fallback if CSV format "25,60"
                                        val parts = payload.split(",")
                                        if (parts.size >= 2) {
                                            temperature = parts[0]
                                            humidity = parts[1]
                                        }
                                    }
                                }
                            }

                            override fun deliveryComplete(token: org.eclipse.paho.client.mqttv3.IMqttDeliveryToken?) {}
                        })

                        Toast.makeText(context, "Subscribed to Sensor Data", Toast.LENGTH_SHORT).show()
                    } catch (e: MqttException) {
                        e.printStackTrace()
                    }
                } else {
                    Toast.makeText(context, "MQTT not connected", Toast.LENGTH_SHORT).show()
                }
            },
            enabled = isMqttConnected
        ) {
            Text("Get Temperature & Humidity")
        }


        Spacer(modifier = Modifier.height(16.dp))
        Text("🌡 Temperature: $temperature °C", style = MaterialTheme.typography.bodyLarge)
        Text("💧 Humidity: $humidity %", style = MaterialTheme.typography.bodyLarge)
    }
}

private fun publishMqttMessage(client: MqttClient, topic: String, messageContent: String) {
    val message = MqttMessage(messageContent.toByteArray()).apply {
        qos = 2
        isRetained = false
    }
    try {
        client.publish(topic, message)
    } catch (e: MqttException) {
        e.printStackTrace()
    }
}
