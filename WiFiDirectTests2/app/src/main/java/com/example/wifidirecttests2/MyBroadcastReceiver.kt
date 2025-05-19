package com.example.wifidirecttests2

import android.Manifest
import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.net.MacAddress
import android.net.NetworkInfo
import android.net.wifi.WpsInfo
import android.net.wifi.p2p.WifiP2pConfig
import android.net.wifi.p2p.WifiP2pDevice
import android.net.wifi.p2p.WifiP2pGroup
import android.net.wifi.p2p.WifiP2pInfo
import android.net.wifi.p2p.WifiP2pManager
import androidx.annotation.RequiresPermission
import kotlinx.coroutines.sync.Mutex


class MyBroadcastReceiver(private val channel: WifiP2pManager.Channel,
                          private val manager: WifiP2pManager,
                          private val activity: MainActivity
) : BroadcastReceiver() {

    private var isWifiP2pEnabled : Boolean = false
    private val peers = mutableListOf<WifiP2pDevice>()

    private var oldState = -1234


    companion object {
        val socketMutex = Mutex()
        var keepAliveSocket: KeepAliveSocket? = null
        var dataTransferSocket: ClientSocket? = null
    }

    @RequiresPermission(allOf = [Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.NEARBY_WIFI_DEVICES])
    fun discoverPeers() {
        manager.discoverPeers(channel, object : WifiP2pManager.ActionListener {
            override fun onSuccess() {
                // Code for when the discovery initiation is successful goes here.
                // No services have actually been discovered yet, so this method
                // can often be left blank. Code for peer discovery goes in the
                // onReceive method, detailed below.
                activity.showInfo("discoverPeers:onSuccess", "Started discovering devices...")
            }

            override fun onFailure(reason: Int) {
                // Code for when the discovery initiation fails goes here.
                // Alert the user that something went wrong.
                activity.showError("discoverPeers:onFailure", "Discovery failed!")
            }
        })
    }

    private val peerListListener = WifiP2pManager.PeerListListener { peerList ->
        val refreshedPeers = peerList.deviceList
        if (refreshedPeers != peers) {
            peers.clear()
            peers.addAll(refreshedPeers)

            val nameList = mutableListOf<String>()
            peerList.deviceList.forEach { device ->
                nameList.add(device.deviceName)
            }

            activity.setDiscoveredDevices(nameList)
        }

        if (peers.isEmpty()) {
            activity.showWarning("peerListListener", "No devices found")
            return@PeerListListener
        }
    }

    @SuppressLint("MissingPermission")
    @RequiresPermission(allOf = [Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.NEARBY_WIFI_DEVICES])
    override fun onReceive(context: Context, intent: Intent) {
        when(intent.action) {
            WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION -> {
                // Determine if Wi-Fi Direct mode is enabled or not, alert
                // the Activity.
                val state = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE, -1)
                isWifiP2pEnabled = state == WifiP2pManager.WIFI_P2P_STATE_ENABLED

                activity.showInfo("onReceive", "WIFI_P2P_STATE_CHANGED_ACTION fired. Changed from $oldState to $state")
                oldState = state
            }
            WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION -> {

                // The peer list has changed! We should probably do something about
                // that.
                activity.showInfo("onReceive", "WIFI_P2P_PEERS_CHANGED_ACTION fired.")

                manager.requestPeers(channel, peerListListener)
            }
            WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION -> {

                // Connection state changed! We should probably do something about
                // that.

                activity.showInfo("onReceive", "WIFI_P2P_CONNECTION_CHANGED_ACTION fired.")

                manager.let { manager ->
                    val networkInfo: NetworkInfo? = intent.getParcelableExtra<NetworkInfo>(
                        WifiP2pManager.EXTRA_NETWORK_INFO, NetworkInfo::class.java)

                    val groupInfo: WifiP2pGroup? = intent.getParcelableExtra<WifiP2pGroup>(
                        WifiP2pManager.EXTRA_WIFI_P2P_GROUP, WifiP2pGroup::class.java)

                    val p2pInfo: WifiP2pInfo? = intent.getParcelableExtra<WifiP2pInfo>(
                        WifiP2pManager.EXTRA_WIFI_P2P_INFO, WifiP2pInfo::class.java)

                    if (groupInfo != null) {
                        activity.showInfo("connect", "Got group info")
                    }

                    // Replacing this with ConnectivityManager doesn't look like it will work
                    // When listening for changes on wifi/p2p networks there nothing happens
                    manager.requestNetworkInfo(channel) { info ->
                        if (info.isConnected == true) {

                            // We are connected with the other device, request connection
                            // info to find group owner IP

                            activity.showInfo("onReceive", "Network is connected.")
                            manager.requestConnectionInfo(channel, connectionListener)
                        } else {
                            activity.showWarning("onReceive", "Network isn't connected.")
                        }
                    }
                }
            }
            WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION -> {
//                (activity.supportFragmentManager.findFragmentById(R.id.frag_list) as DeviceListFragment)
//                    .apply {
//                        updateThisDevice(
//                            intent.getParcelableExtra(
//                                WifiP2pManager.EXTRA_WIFI_P2P_DEVICE) as WifiP2pDevice
//                        )
//                    }

                activity.showInfo("onReceive", "WIFI_P2P_THIS_DEVICE_CHANGED_ACTION fired.")
            }
        }
    }

    @RequiresPermission(allOf = [Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.NEARBY_WIFI_DEVICES])
    fun connect(idx: Int) {
        // Picking the first device found on the network.
        val device = peers[idx]

        activity.showInfo("MyBroadcastReceiver.connect", "Connecting to device ${device.deviceName}")

        val configBuilder = WifiP2pConfig.Builder()
        configBuilder.setDeviceAddress(MacAddress.fromString(device.deviceAddress))
        configBuilder.enablePersistentMode(false) // Reconnecting with a fully paired device fails
                                                  // so disable persistent mode to prevent saving the
                                                  // pairing info on the Android side
        val config = configBuilder.build()
        config.wps.setup = WpsInfo.PBC

        manager.connect(channel, config, object : WifiP2pManager.ActionListener {

            override fun onSuccess() {
                // WiFiDirectBroadcastReceiver notifies us. Ignore for now.
                activity.showInfo("connect.onSuccess", "")
            }

            override fun onFailure(reason: Int) {
                activity.showError("connect.onFailure", "" + reason)
            }
        })
    }

    @Suppress("NULLABILITY_MISMATCH_BASED_ON_JAVA_ANNOTATIONS")
    private val connectionListener = WifiP2pManager.ConnectionInfoListener { info ->

        // String from WifiP2pInfo struct
        val groupOwnerAddress: String = info.groupOwnerAddress.hostAddress

        // After the group negotiation, we can determine the group owner
        // (server).
        if (info.groupFormed && info.isGroupOwner) {
            // Do whatever tasks are specific to the group owner.
            // One common case is creating a group owner thread and accepting
            // incoming connections.
            activity.showInfo("ConnectionListener", "Group formed as group owner")
        } else if (info.groupFormed) {
            // The other device acts as the peer (client). In this case,
            // you'll want to create a peer thread that connects
            // to the group owner.
            activity.showInfo("ConnectionListener", "Group formed as group member")

            // Will be picked up by the KeepAliveWiFiDirectService foreground service
            while (!socketMutex.tryLock()) {
                Thread.sleep(1)
            }

            keepAliveSocket = KeepAliveSocket(groupOwnerAddress, 50001)
            dataTransferSocket = ClientSocket(groupOwnerAddress, 50011, keepAliveSocket!!)

            socketMutex.unlock()
        } else {
            activity.showInfo("ConnectionListener", "Something else")
        }
    }
}