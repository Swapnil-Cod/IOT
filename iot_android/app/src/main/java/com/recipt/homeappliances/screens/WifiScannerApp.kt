package com.recipt.homeappliances
import android.content.Context
import android.net.wifi.ScanResult
import android.net.wifi.WifiManager
import android.os.Handler
import android.os.Looper
import android.widget.Toast
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.navigation.NavController
@Composable
fun WifiScannerApp(
    navController: NavController,
    connectToNetwork: (Context, String, String, () -> Unit) -> Unit,
    sendSsidPasswordToApi: (Context, String, String, NavController) -> Unit,
    scanWifiNetworks: (Context, WifiManager) -> List<ScanResult>
) {
    val context = LocalContext.current
    val wifiManager = context.getSystemService(Context.WIFI_SERVICE) as WifiManager

    var wifiList by remember { mutableStateOf<List<ScanResult>>(emptyList()) }
    var showDialog by remember { mutableStateOf(false) }
    var selectedSsid by remember { mutableStateOf("") }
    var enteredPassword by remember { mutableStateOf("") }
    var isConnectedToIotDevice by remember { mutableStateOf(false) }
    var connectionStep by remember { mutableStateOf(1) } // 1 = connect to IoT, 2 = send home creds

    fun rescanWifi() {
        wifiList = scanWifiNetworks(context, wifiManager)
    }

    LaunchedEffect(Unit) {
        rescanWifi()
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        val heading = if (!isConnectedToIotDevice) "Connect to IoT Device" else "Select Available Wi-Fi Network"
        Text(heading, style = MaterialTheme.typography.headlineSmall)

        if (wifiList.isEmpty()) {
            Text("No Wi-Fi networks found. Please ensure Wi-Fi is enabled and location permission is granted.")
        } else {
            LazyColumn(modifier = Modifier.fillMaxSize()) {
                items(wifiList) { wifi ->
                    WifiNetworkItem(wifi = wifi) {
                        selectedSsid = wifi.SSID
                        enteredPassword = ""
                        showDialog = true
                    }
                }
            }
        }
    }

    if (showDialog) {
        WifiPasswordDialog(
            ssid = selectedSsid,
            password = enteredPassword,
            onPasswordChange = { enteredPassword = it },
            onDismiss = { showDialog = false },
            onConnect = {
                showDialog = false
                if (connectionStep == 1) {
                    connectToNetwork(context, selectedSsid, enteredPassword) {
                        isConnectedToIotDevice = true
                        connectionStep = 2
                        Toast.makeText(context, "Connected to IoT Device", Toast.LENGTH_SHORT).show()
                        Handler(Looper.getMainLooper()).postDelayed({
                            rescanWifi()
                        }, 3000)
                    }
                } else {
                    sendSsidPasswordToApi(context, selectedSsid, enteredPassword, navController)
                }
            }
        )
    }
}

@Composable
fun WifiNetworkItem(wifi: ScanResult, onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(8.dp)
            .clickable { onClick() },
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = wifi.SSID,
            modifier = Modifier.weight(1f),
            style = MaterialTheme.typography.bodyLarge
        )
    }
}

@Composable
fun WifiPasswordDialog(
    ssid: String,
    password: String,
    onPasswordChange: (String) -> Unit,
    onDismiss: () -> Unit,
    onConnect: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Connect to $ssid") },
        text = {
            Column {
                TextField(
                    value = password,
                    onValueChange = onPasswordChange,
                    label = { Text("Password") },
                    singleLine = true,
                    visualTransformation = PasswordVisualTransformation()
                )
            }
        },
        confirmButton = {
            Button(onClick = onConnect) { Text("Connect") }
        },
        dismissButton = {
            Button(onClick = onDismiss) { Text("Cancel") }
        }
    )
}
