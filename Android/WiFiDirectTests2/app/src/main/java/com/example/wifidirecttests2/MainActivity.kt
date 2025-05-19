package com.example.wifidirecttests2

import android.Manifest
import android.annotation.SuppressLint
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.net.wifi.p2p.WifiP2pManager
import android.os.Bundle
import android.util.Log
import android.widget.ArrayAdapter
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.example.wifidirecttests2.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private val intentFilter = IntentFilter()

    private lateinit var channel: WifiP2pManager.Channel
    private lateinit var manager: WifiP2pManager

    private val discoveredDevices = mutableListOf<String>()
    private lateinit var discoveredDevicesAdapter: ArrayAdapter<String>

    private val logMessages = mutableListOf<String>()
    private lateinit var logMessageAdapter: ArrayAdapter<String>

    @SuppressLint("InlinedApi")
    private var permissionsToRequest = arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.NEARBY_WIFI_DEVICES, Manifest.permission.ACCESS_NETWORK_STATE)
    private lateinit var permissionsRequest: ActivityResultLauncher<Array<String>>

    @SuppressLint("InlinedApi")
    private var notifyPermissionsToRequest = arrayOf(Manifest.permission.FOREGROUND_SERVICE, Manifest.permission.FOREGROUND_SERVICE_DATA_SYNC)

    companion object {
        const val CHANNEL_ID = "KeepAliveWiFiDirectServiceChannelId"
        private lateinit var receiver: MyBroadcastReceiver
    }

    @SuppressLint("MissingPermission")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        val binding: ActivityMainBinding = ActivityMainBinding.inflate(layoutInflater)

        setContentView(binding.root)

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        // Indicates a change in the Wi-Fi Direct status.
        intentFilter.addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION)

        // Indicates a change in the list of available peers.
        intentFilter.addAction(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION)

        // Indicates the state of Wi-Fi Direct connectivity has changed.
        intentFilter.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION)

        // Indicates this device's details have changed.
        intentFilter.addAction(WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION)

        manager = getSystemService(WIFI_P2P_SERVICE) as WifiP2pManager
        channel = manager.initialize(this, mainLooper, null)

        discoveredDevicesAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, discoveredDevices)
        binding.listDiscoveredDevices.adapter = discoveredDevicesAdapter
        setDiscoveredDevices(listOf<String>())

        @SuppressLint("MissingPermission") // Checked with areAllPermissionsGranted
        binding.listDiscoveredDevices.setOnItemClickListener { parent, view, position, id ->

            if (position == 0) {
                return@setOnItemClickListener
            }

            val deviceName = discoveredDevices[position]
            showInfo("listDiscoveredDevices.ClickListener", "Clicked on item[$position]: '$deviceName'")
            receiver.connect(position - 1) // -1 as we add one item as a label
        }

        logMessageAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, logMessages)
        binding.listLog.adapter = logMessageAdapter
        logMessageAdapter.notifyDataSetChanged()

        permissionsRequest = registerForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions()) { results ->
                var allPermsGranted = true
                results.forEach { result ->
                    if (result.value) {
                        showInfo("btnStartDiscovery", "Permission ${result.key} is now granted after requesting it.")
                    }
                    else {
                        showError("btnStartDiscovery", "Permission ${result.key} was denied after requesting it.")
                        allPermsGranted = false
                    }
                }

                if (allPermsGranted) {
                    checkPermsAndStartDiscovery(false)
                }
        }

        var foregroundServicePermReq = registerForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions()) { results ->
            var allPermsGranted = true
            results.forEach { result ->
                if (result.value) {
                    showInfo("btnRequestNotifyPerms", "Permission ${result.key} is now granted after requesting it.")
                }
                else {
                    showError("btnRequestNotifyPerms", "Permission ${result.key} was denied after requesting it.")
                    allPermsGranted = false
                }
            }
        }

        binding.btnRequestNotifyPerms.setOnClickListener {
            foregroundServicePermReq.launch(notifyPermissionsToRequest)
        }

        binding.btnStartDiscovery.setOnClickListener {
            checkPermsAndStartDiscovery(true)
        }

        createNotificationChannel()

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.FOREGROUND_SERVICE) == PackageManager.PERMISSION_GRANTED) {
            startForegroundService(Intent(this, KeepAliveWiFiDirectService::class.java))
        }
    }

    private fun createNotificationChannel() {
        // Create the NotificationChannel, but only on API 26+ because
        // the NotificationChannel class is not in the Support Library.
        val name = "ChannelName"
        val descriptionText = "Channel description"
        val importance = NotificationManager.IMPORTANCE_DEFAULT
        val channel = NotificationChannel(CHANNEL_ID, name, importance).apply {
            description = descriptionText
        }
        // Register the channel with the system.
        val notificationManager: NotificationManager =
            getSystemService(NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.createNotificationChannel(channel)
    }

    private fun checkPermsAndStartDiscovery(fromButton : Boolean) {
        showInfo("checkPermsAndStartDiscovery", "Trying to start discovery. FromButton = $fromButton")
        var allPermissionsGranted = true
        permissionsToRequest.forEach { perm ->
            if (ActivityCompat.checkSelfPermission(this, perm) == PackageManager.PERMISSION_DENIED) {
                allPermissionsGranted = false
            }
        }

        if (allPermissionsGranted) {
            receiver.discoverPeers()
        } else if (fromButton) {
            permissionsRequest.launch(permissionsToRequest)
        }
    }

    fun setDiscoveredDevices(devices: List<String>) {
        discoveredDevices.clear()
        discoveredDevices.add("Discovered devices:")
        discoveredDevices.addAll(devices)
        discoveredDevicesAdapter.notifyDataSetChanged()
    }

    public override fun onResume() {
        super.onResume()
        receiver = MyBroadcastReceiver(channel, manager, this)

        registerReceiver(receiver, intentFilter)
    }

    override fun onPause() {
        super.onPause()
        unregisterReceiver(receiver)
    }

    private fun concatLogString(msg: String, funcName: String) : String {
        return "$funcName: $msg"
    }

    fun showError(funcName: String, msg: String) {
        val msgToDisplay = concatLogString(msg, funcName)
        Toast.makeText(this, msgToDisplay, Toast.LENGTH_LONG).show()
        logMessages.add("ERR: $msgToDisplay")
        logMessageAdapter.notifyDataSetChanged()
        Log.e(null, msgToDisplay)
    }

    fun showWarning(funcName: String, msg: String) {
        val msgToDisplay = concatLogString(msg, funcName)
        Toast.makeText(this, msgToDisplay, Toast.LENGTH_SHORT).show()
        logMessages.add("WRN: $msgToDisplay")
        logMessageAdapter.notifyDataSetChanged()
        Log.w(null, msgToDisplay)
    }

    fun showInfo(funcName: String, msg: String) {
        val msgToDisplay = concatLogString(msg, funcName)
        logMessages.add("NFO: $msgToDisplay")
        logMessageAdapter.notifyDataSetChanged()
        Log.i(null, msgToDisplay)
    }
}