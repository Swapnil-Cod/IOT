package com.recipt.homeappliances
import androidx.compose.runtime.Composable
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.NavController
import org.eclipse.paho.client.mqttv3.MqttClient

@Composable
fun AppNavHost(
    client: MqttClient,
    isMqttConnected: Boolean,
    connectToNetwork: (context: android.content.Context, ssid: String, password: String, onSuccess: () -> Unit) -> Unit,
    sendSsidPasswordToApi: (context: android.content.Context, ssid: String, password: String, navController: NavController) -> Unit,
    scanWifiNetworks: (context: android.content.Context, wifiManager: android.net.wifi.WifiManager) -> List<android.net.wifi.ScanResult>
) {
    val navController = rememberNavController()

    NavHost(navController = navController, startDestination = "wifiScanner") {
        composable("wifiScanner") {
            WifiScannerApp(
                navController = navController,
                connectToNetwork = connectToNetwork,
                sendSsidPasswordToApi = sendSsidPasswordToApi,
                scanWifiNetworks = scanWifiNetworks
            )
        }
        composable("lightingControl") {
            LightingControlScreen(client = client, isMqttConnected = isMqttConnected)
        }
    }
}
