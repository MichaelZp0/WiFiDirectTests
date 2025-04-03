package com.example.wifidirecttests2

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.net.wifi.p2p.WifiP2pManager
import android.os.Bundle
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
import kotlinx.coroutines.sync.Mutex

class MainActivity : AppCompatActivity() {

    private val intentFilter = IntentFilter()

    private lateinit var channel: WifiP2pManager.Channel
    private lateinit var manager: WifiP2pManager
    private lateinit var receiver: MyBroadcastReceiver

    private val discoveredDevices = mutableListOf<String>()
    private lateinit var discoveredDevicesAdapter: ArrayAdapter<String>

    private val backgroundLog = mutableListOf<String>()
    private val logMessages = mutableListOf<String>()
    private lateinit var logMessageAdapter: ArrayAdapter<String>

    private var permissionsToRequest = listOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.NEARBY_WIFI_DEVICES)
    enum class PermissionState {
        Unknown,
        Granted,
        Denied
    }

    private var permissionStates = HashMap<String, PermissionState>()
    private var permissionRequests = HashMap<String, ActivityResultLauncher<String>>()

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

        manager = getSystemService(Context.WIFI_P2P_SERVICE) as WifiP2pManager
        channel = manager.initialize(this, mainLooper, null)

        discoveredDevicesAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, discoveredDevices)
        binding.listDiscoveredDevices.adapter = discoveredDevicesAdapter
        setDiscoveredDevices(listOf<String>())

        @SuppressLint("MissingPermission") // Checked with areAllPermissionsGranted
        binding.listDiscoveredDevices.setOnItemClickListener { parent, view, position, id ->
            val deviceName = discoveredDevices[position]
            showInfo("listDiscoveredDevices.ClickListener", "Clicked on item[$position]: '$deviceName'")
            if (areAllPermissionsGranted()) {
                receiver.connect(position - 1) // -1 as we add one item as a label
            }
        }

        logMessageAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, logMessages)
        binding.listLog.adapter = logMessageAdapter
        logMessageAdapter.notifyDataSetChanged()

        permissionsToRequest.forEach { perm ->
            permissionStates.put(perm, PermissionState.Unknown)
        }

        permissionsToRequest.forEach { perm ->
            var reg = registerForActivityResult(ActivityResultContracts.RequestPermission()) { result ->
                if (result) {
                    permissionStates[perm] = PermissionState.Granted
                    showInfo("btnRequestPermissions", "Permission $perm is now granted after requesting it.")
                }
                else {
                    permissionStates[perm] = PermissionState.Denied
                    showError("btnRequestPermissions", "Permission $perm was denied after requesting it.")
                }
            }
            permissionRequests.put(perm, reg)
        }

        binding.btnRequestPermissions.setOnClickListener {
            permissionsToRequest.forEach { perm ->
                when(permissionStates[perm]) {
                    PermissionState.Unknown -> {
                        val permissionState = ActivityCompat.checkSelfPermission(this, perm)
                        if (permissionState == PackageManager.PERMISSION_GRANTED) {
                            permissionStates[perm] = PermissionState.Granted
                        }
                        else {
                            permissionRequests[perm]?.launch(perm)
                        }
                    }
                    PermissionState.Granted -> {
                        showInfo("btnRequestPermissions", "Permission $perm was already granted.")
                    }
                    PermissionState.Denied -> {
                        permissionRequests[perm]?.launch(perm)
                    }
                    null -> {
                        showError("btnRequestPermissions", "Called with empty param")
                    }
                }
            }
        }

        binding.btnStartDiscovery.setOnClickListener {
            if (areAllPermissionsGranted()) {
                showInfo("btnRequestPermissions", "Started discovery.")
                receiver.discoverPeers()
            }
            else {
                showWarning("btnRequestPermissions", "Did not start discovery, as not all permissions were granted.")
            }
        }

        binding.btnDeleteAllWifiGroups.setOnClickListener {
            receiver.deletePersistentGroups()
        }

        binding.btnListAllWifiGroups.setOnClickListener {
            // receiver.listPersistentGroups()
            logMessages.addAll(backgroundLog)
            backgroundLog.clear()
            logMessageAdapter.notifyDataSetChanged()
        }
    }

    private fun areAllPermissionsGranted() : Boolean {
        var allPermsGranted = true
        permissionStates.forEach { item ->
            if (item.value != PermissionState.Granted) {
                allPermsGranted = false
                showError("btnStartDiscovery", "Permission ${item.key} was not granted.")
            }
        }
        return allPermsGranted
    }

    public fun setDiscoveredDevices(devices: List<String>) {
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
    }

    fun showWarning(funcName: String, msg: String) {
        val msgToDisplay = concatLogString(msg, funcName)
        Toast.makeText(this, msgToDisplay, Toast.LENGTH_SHORT).show()
        logMessages.add("WRN: $msgToDisplay")
        logMessageAdapter.notifyDataSetChanged()
    }

    fun showInfo(funcName: String, msg: String) {
        val msgToDisplay = concatLogString(msg, funcName)
        logMessages.add("NFO: $msgToDisplay")
        logMessageAdapter.notifyDataSetChanged()
    }

    fun enqueueInfo(funcName: String, msg: String) {
        val msgToDisplay = concatLogString(msg, funcName)
        backgroundLog.add("NFO: $msgToDisplay")
    }
}