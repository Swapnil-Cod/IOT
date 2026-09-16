package com.iot.distancemonitor

import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.google.android.material.button.MaterialButton
import org.eclipse.paho.client.mqttv3.*
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.security.KeyStore
import java.security.cert.CertificateFactory
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocketFactory
import javax.net.ssl.TrustManagerFactory

class MainActivity : AppCompatActivity() {

    // -------- CHANGE THESE TO MATCH YOUR SETUP --------
    private val brokerIp   = "13.127.43.131"
    private val brokerPort = 8883
    private val topic      = "sensor/ultrasonic/distance"
    private val statusTopic = "sensor/ultrasonic/status"  // device online/offline, via MQTT Last Will
    // Credentials come from local.properties (mqtt.user / mqtt.password), not checked into VCS
    private val mqttUser   = BuildConfig.MQTT_USER
    private val mqttPass   = BuildConfig.MQTT_PASSWORD
    // ---------------------------------------------------

    private val brokerUri  = "ssl://$brokerIp:$brokerPort"
    private val clientId   = "android-distance-${System.currentTimeMillis()}"
    private val timeFormatter = SimpleDateFormat("HH:mm:ss", Locale.getDefault())

    private var mqttClient: MqttAsyncClient? = null
    private val mainHandler = Handler(Looper.getMainLooper())

    // Tank depth entered manually by user
    private var tankDepthCm: Float = 500f

    private lateinit var tvDistance: TextView
    private lateinit var tvWaterLevel: TextView
    private lateinit var tvPercent: TextView
    private lateinit var tvStatus: TextView
    private lateinit var tvLastUpdated: TextView
    private lateinit var tvDeviceStatus: TextView
    private lateinit var statusDot: View
    private lateinit var deviceStatusDot: View
    private lateinit var btnConnect: MaterialButton
    private lateinit var btnSetDepth: MaterialButton
    private lateinit var etTankDepth: EditText
    private lateinit var waterTankView: WaterTankView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        tvDistance    = findViewById(R.id.tvDistance)
        tvWaterLevel = findViewById(R.id.tvWaterLevel)
        tvPercent    = findViewById(R.id.tvPercent)
        tvStatus     = findViewById(R.id.tvStatus)
        tvLastUpdated = findViewById(R.id.tvLastUpdated)
        tvDeviceStatus = findViewById(R.id.tvDeviceStatus)
        statusDot    = findViewById(R.id.statusDot)
        deviceStatusDot = findViewById(R.id.deviceStatusDot)
        btnConnect   = findViewById(R.id.btnConnect)
        btnSetDepth  = findViewById(R.id.btnSetDepth)
        etTankDepth  = findViewById(R.id.etTankDepth)
        waterTankView = findViewById(R.id.waterTankView)

        // Restore previously saved tank depth
        val prefs = getSharedPreferences("tank_prefs", Context.MODE_PRIVATE)
        tankDepthCm = prefs.getFloat("tank_depth", 500f)
        etTankDepth.setText(tankDepthCm.toInt().toString())

        // User manually sets tank depth
        btnSetDepth.setOnClickListener {
            val input = etTankDepth.text.toString().toFloatOrNull()
            if (input != null && input > 0) {
                tankDepthCm = input
                prefs.edit().putFloat("tank_depth", tankDepthCm).apply()
                Toast.makeText(this, "Tank depth set to ${tankDepthCm.toInt()} cm", Toast.LENGTH_SHORT).show()
            } else {
                Toast.makeText(this, "Enter a valid depth", Toast.LENGTH_SHORT).show()
            }
        }

        btnConnect.setOnClickListener {
            if (mqttClient?.isConnected == true) {
                disconnect()
            } else {
                connect()
            }
        }
    }

    private fun connect() {
        updateStatus("Connecting...", false)

        try {
            mqttClient = MqttAsyncClient(brokerUri, clientId, null)
            mqttClient?.setCallback(object : MqttCallbackExtended {
                override fun connectComplete(reconnect: Boolean, serverURI: String?) {
                    mainHandler.post {
                        updateStatus("Connected", true)
                        btnConnect.text = "Disconnect"
                    }
                    mqttClient?.subscribe(topic, 0)
                    mqttClient?.subscribe(statusTopic, 1)
                }

                override fun connectionLost(cause: Throwable?) {
                    mainHandler.post {
                        updateStatus("Disconnected", false)
                        btnConnect.text = "Connect"
                        resetDisplays()
                    }
                }

                override fun messageArrived(topic: String?, message: MqttMessage?) {
                    val payload = message?.toString() ?: return
                    when (topic) {
                        this@MainActivity.topic -> mainHandler.post { handleMessage(payload) }
                        statusTopic -> mainHandler.post { handleDeviceStatus(payload) }
                    }
                }

                override fun deliveryComplete(token: IMqttDeliveryToken?) {}
            })

            val options = MqttConnectOptions().apply {
                isCleanSession = true
                connectionTimeout = 10
                keepAliveInterval = 30
                isAutomaticReconnect = true
                userName = mqttUser
                password = mqttPass.toCharArray()
                socketFactory = tlsSocketFactory()
            }

            mqttClient?.connect(options)
        } catch (e: Exception) {
            mainHandler.post {
                updateStatus("Error: ${e.message}", false)
            }
        }
    }

    private fun tlsSocketFactory(): SSLSocketFactory {
        val cert = resources.openRawResource(R.raw.ca).use {
            CertificateFactory.getInstance("X.509").generateCertificate(it)
        }
        val trustStore = KeyStore.getInstance(KeyStore.getDefaultType()).apply {
            load(null, null)
            setCertificateEntry("mqtt-ca", cert)
        }
        val tmf = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm()).apply {
            init(trustStore)
        }
        val sslContext = SSLContext.getInstance("TLSv1.2").apply {
            init(null, tmf.trustManagers, null)
        }
        return sslContext.socketFactory
    }

    private fun disconnect() {
        try {
            mqttClient?.disconnect()
            mqttClient?.close()
            mqttClient = null
        } catch (_: Exception) {}

        updateStatus("Disconnected", false)
        btnConnect.text = "Connect"
        resetDisplays()
    }

    private fun handleMessage(payload: String) {
        try {
            val json = JSONObject(payload)
            if (json.has("distance_cm")) {
                val sensorDistance = json.getDouble("distance_cm").toFloat()

                // Sensor measures distance from top of tank to water surface
                // Water level = tank depth - sensor distance
                val waterLevel = (tankDepthCm - sensorDistance).coerceIn(0f, tankDepthCm)
                val percent = if (tankDepthCm > 0) (waterLevel / tankDepthCm) * 100f else 0f

                tvDistance.text = String.format("%.1f cm", sensorDistance)
                tvWaterLevel.text = String.format("%.1f cm", waterLevel)
                tvPercent.text = String.format("%.1f%%", percent)
                waterTankView.waterLevelPercent = percent
                tvLastUpdated.text = "Last updated: ${timeFormatter.format(Date())}"

            } else if (json.has("error")) {
                tvDistance.text = "ERR"
                tvWaterLevel.text = "ERR"
                tvPercent.text = "--%"
            }
        } catch (_: Exception) {
            tvDistance.text = "?"
            tvWaterLevel.text = "?"
            tvPercent.text = "--%"
        }
    }

    private fun handleDeviceStatus(payload: String) {
        val online = payload.trim().equals("online", ignoreCase = true)
        tvDeviceStatus.text = if (online) "Device: Online" else "Device: Offline"
        deviceStatusDot.setBackgroundColor(
            getColor(if (online) R.color.status_connected else R.color.status_disconnected)
        )
    }

    private fun resetDisplays() {
        tvDistance.text = "-- cm"
        tvWaterLevel.text = "-- cm"
        tvPercent.text = "--%"
        waterTankView.waterLevelPercent = 0f
        tvLastUpdated.text = "Last updated: --"
        tvDeviceStatus.text = "Device: --"
        deviceStatusDot.setBackgroundColor(getColor(R.color.text_secondary))
    }

    private fun updateStatus(text: String, connected: Boolean) {
        tvStatus.text = text
        val color = if (connected) {
            getColor(R.color.status_connected)
        } else {
            getColor(R.color.status_disconnected)
        }
        statusDot.setBackgroundColor(color)
    }

    override fun onDestroy() {
        disconnect()
        super.onDestroy()
    }
}
