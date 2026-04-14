package com.recipt.homeappliances

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.net.*
import android.net.wifi.ScanResult
import android.net.wifi.WifiManager
import android.net.wifi.WifiNetworkSpecifier
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody
import org.eclipse.paho.client.mqttv3.*
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence
import org.json.JSONObject
import java.io.IOException

class MainActivity : ComponentActivity() {

    private val brokerUrl = "tcp://13.234.149.187:1883"
    private val clientId = "KotlinMqttClient"

    private var client by mutableStateOf<MqttClient?>(null)
    private var isMqttConnected by mutableStateOf(false)

    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
            val locationGranted = permissions[Manifest.permission.ACCESS_FINE_LOCATION] ?: false
            val wifiStateGranted = permissions[Manifest.permission.ACCESS_WIFI_STATE] ?: false

            if (locationGranted && wifiStateGranted) {
                launchApp()
            } else {
                Toast.makeText(this, "Permissions denied. Cannot scan Wi-Fi.", Toast.LENGTH_SHORT).show()
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        connectMqtt()

        if (checkPermissions()) {
            launchApp()
        } else {
            requestPermissionLauncher.launch(
                arrayOf(
                    Manifest.permission.ACCESS_FINE_LOCATION,
                    Manifest.permission.ACCESS_WIFI_STATE
                )
            )
        }
    }

    private fun launchApp() {
        setContent {
            client?.let { mqttClient ->
                AppNavHost(
                    client = mqttClient,
                    isMqttConnected = isMqttConnected,
                    connectToNetwork = ::connectToNetwork,
                    sendSsidPasswordToApi = ::sendSsidPasswordToApi,
                    scanWifiNetworks = ::scanWifiNetworks
                )
            }
        }
    }

    private fun connectMqtt() {
        lifecycleScope.launch(Dispatchers.IO) {
            try {
                val mqttClient = MqttClient(brokerUrl, clientId, MemoryPersistence())
                val options = MqttConnectOptions().apply {
                    isCleanSession = true
                    userName = "swap123"
                    password = "swap@123".toCharArray()
                }
                mqttClient.connect(options)
                withContext(Dispatchers.Main) {
                    client = mqttClient
                    isMqttConnected = true
                    Toast.makeText(this@MainActivity, "MQTT Connected", Toast.LENGTH_SHORT).show()
                }
            } catch (e: MqttException) {
                withContext(Dispatchers.Main) {
                    Toast.makeText(this@MainActivity, "MQTT connection failed: ${e.message}", Toast.LENGTH_LONG).show()
                }
            }
        }
    }

    private fun reconnectMqtt() {
        lifecycleScope.launch(Dispatchers.IO) {
            try {
                val connectivityManager = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
                connectivityManager.bindProcessToNetwork(null)

                client?.takeIf { it.isConnected }?.disconnect()

                val mqttClient = MqttClient(brokerUrl, clientId, MemoryPersistence())
                val options = MqttConnectOptions().apply {
                    isCleanSession = true
                    userName = "swap123"
                    password = "swap@123".toCharArray()
                }

                mqttClient.connect(options)

                withContext(Dispatchers.Main) {
                    client = mqttClient
                    isMqttConnected = true
                    Toast.makeText(this@MainActivity, "MQTT Reconnected", Toast.LENGTH_SHORT).show()
                }
            } catch (e: MqttException) {
                withContext(Dispatchers.Main) {
                    Toast.makeText(this@MainActivity, "Reconnect failed: ${e.message}", Toast.LENGTH_SHORT).show()
                }
            }
        }
    }

    private fun checkPermissions(): Boolean {
        return ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED &&
                ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_WIFI_STATE) == PackageManager.PERMISSION_GRANTED
    }

    private fun scanWifiNetworks(context: Context, wifiManager: WifiManager): List<ScanResult> {
        if (!wifiManager.isWifiEnabled) {
            Toast.makeText(context, "Wi-Fi is disabled. Please enable it.", Toast.LENGTH_SHORT).show()
            return emptyList()
        }

        if (ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            return emptyList()
        }

        return if (wifiManager.startScan()) wifiManager.scanResults else emptyList()
    }

    private fun connectToNetwork(
        context: Context,
        ssid: String,
        password: String,
        onConnectSuccess: () -> Unit
    ) {
        val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val wifiSpecifier = WifiNetworkSpecifier.Builder()
                .setSsid(ssid)
                .setWpa2Passphrase(password)
                .build()

            val networkRequest = NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .setNetworkSpecifier(wifiSpecifier)
                .build()

            connectivityManager.requestNetwork(networkRequest, object : ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: Network) {
                    connectivityManager.bindProcessToNetwork(network)
                    onConnectSuccess()
                }

                override fun onUnavailable() {
                    Toast.makeText(context, "Connection failed.", Toast.LENGTH_SHORT).show()
                }
            })
        } else {
            Toast.makeText(context, "Unsupported Android version.", Toast.LENGTH_SHORT).show()
        }
    }

    private fun sendSsidPasswordToApi(
        context: Context,
        ssid: String,
        password: String,
        navController: androidx.navigation.NavController
    ) {
        val url = "http://192.168.4.1/connect"
        val requestBody = JSONObject().apply {
            put("ssid", ssid)
            put("password", password)
        }.toString()

        val client = OkHttpClient()
        val request = Request.Builder()
            .url(url)
            .post(RequestBody.create("application/json".toMediaType(), requestBody))
            .build()

        client.newCall(request).enqueue(object : okhttp3.Callback {
            override fun onFailure(call: okhttp3.Call, e: IOException) {
                Handler(Looper.getMainLooper()).post {
                    Toast.makeText(context, "API Call Failed", Toast.LENGTH_SHORT).show()
                }
            }

            override fun onResponse(call: okhttp3.Call, response: okhttp3.Response) {
                Handler(Looper.getMainLooper()).post {
                    if (response.isSuccessful) {
                        Toast.makeText(context, "Credentials sent successfully to IoT device!", Toast.LENGTH_SHORT).show()

                        val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
                        connectivityManager.bindProcessToNetwork(null)

                        Handler(Looper.getMainLooper()).postDelayed({
                            reconnectMqtt()
                            navController.navigate("lightingControl")
                        }, 3000)
                    } else {
                        Toast.makeText(context, "Failed to send credentials to IoT device.", Toast.LENGTH_SHORT).show()
                    }
                }
            }
        })
    }

    override fun onDestroy() {
        super.onDestroy()
        client?.takeIf { it.isConnected }?.disconnect()
    }
}
