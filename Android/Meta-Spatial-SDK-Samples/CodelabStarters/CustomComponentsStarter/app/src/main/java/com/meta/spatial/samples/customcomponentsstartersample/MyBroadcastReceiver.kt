package com.meta.spatial.samples.customcomponentsstartersample

import android.Manifest
import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.net.ConnectivityManager
import android.net.LinkProperties
import android.net.MacAddress
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkInfo
import android.net.NetworkRequest
import android.net.wifi.WpsInfo
import android.net.wifi.p2p.WifiP2pConfig
import android.net.wifi.p2p.WifiP2pDevice
import android.net.wifi.p2p.WifiP2pGroup
import android.net.wifi.p2p.WifiP2pInfo
import android.net.wifi.p2p.WifiP2pManager
import android.util.Log
import androidx.annotation.RequiresPermission
import kotlinx.coroutines.sync.Mutex
import java.net.NetworkInterface
import java.util.Optional


class MyBroadcastReceiver(private val channel: WifiP2pManager.Channel,
                          private val manager: WifiP2pManager,
                          private val activity: CustomComponentsSampleActivity,
                          private val connectivityManager: ConnectivityManager
) : BroadcastReceiver() {

    private var isWifiP2pEnabled : Boolean = false
    private val peers = mutableListOf<WifiP2pDevice>()

    private var oldState = -1234

    private var wifiDirectNetwork: Optional<Network> = Optional.empty()

    private var networkKeepAlive: NetworkKeepAlive? = null

    companion object {
        val peerMutex = Mutex()
        val socketMutex = Mutex()
        val connectMutex = Mutex()
        var keepAliveSocket: KeepAliveSocket? = null
        var dataTransferSocket: ClientSocket? = null
    }

    init {
        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .build()

        connectivityManager.registerNetworkCallback(request, object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(availableNetwork: Network) {
                super.onAvailable(availableNetwork)
                Log.i(null, "WIFIDIRECT - ConnectivityManager - TRANSPORT_WIFI - $availableNetwork - available")
            }

            override fun onUnavailable() {
                super.onUnavailable()
                Log.i(null, "WIFIDIRECT - ConnectivityManager - TRANSPORT_WIFI unavailable")
            }

            override fun onBlockedStatusChanged(network: Network, blocked: Boolean) {
                super.onBlockedStatusChanged(network, blocked)
                Log.i(null, "WIFIDIRECT - ConnectivityManager - TRANSPORT_WIFI - $network - onBlockedStatusChanged - $blocked")
            }

            override fun onCapabilitiesChanged(
                network: Network,
                networkCapabilities: NetworkCapabilities
            ) {
                super.onCapabilitiesChanged(network, networkCapabilities)
                Log.i(null, "WIFIDIRECT - ConnectivityManager - TRANSPORT_WIFI - $network - onCapabilitiesChanged - $networkCapabilities")

                if (!networkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)) {
                    Log.i(null, "WIFIDIRECT - ConnectivityManager - TRANSPORT_WIFI - $network - onCapabilitiesChanged - Choose Network")
                    wifiDirectNetwork = Optional.of(network)
                }
            }

            override fun onLinkPropertiesChanged(
                network: Network,
                linkProperties: LinkProperties
            ) {
                super.onLinkPropertiesChanged(network, linkProperties)
                Log.i(null, "WIFIDIRECT - ConnectivityManager - TRANSPORT_WIFI - $network - onLinkPropertiesChanged - $linkProperties")
            }

            override fun onLosing(network: Network, maxMsToLive: Int) {
                super.onLosing(network, maxMsToLive)
                Log.i(null, "WIFIDIRECT - ConnectivityManager - TRANSPORT_WIFI - $network - onLosing - $maxMsToLive")
            }

            override fun onLost(network: Network) {
                super.onLost(network)
                Log.i(null, "WIFIDIRECT - ConnectivityManager - TRANSPORT_WIFI - $network - onLost")
            }
        })
    }

    @SuppressLint("MissingPermission")
    @RequiresPermission(allOf = [Manifest.permission.ACCESS_FINE_LOCATION])
    fun discoverPeers() {
        manager.discoverPeers(channel, object : WifiP2pManager.ActionListener {
            override fun onSuccess() {
                // Code for when the discovery initiation is successful goes here.
                // No services have actually been discovered yet, so this method
                // can often be left blank. Code for peer discovery goes in the
                // onReceive method, detailed below.
                Log.i(null, "WIFIDIRECT - discoverPeers:onSuccess - started discovering devices...")
            }

            override fun onFailure(reason: Int) {
                // Code for when the discovery initiation fails goes here.
                // Alert the user that something went wrong.
                Log.i(null, "WIFIDIRECT - discoverPeers:onFailure - Discovery failed!")
            }
        })
    }

    @SuppressLint("MissingPermission")
    private val peerListListener = WifiP2pManager.PeerListListener { peerList ->
        val refreshedPeers = peerList.deviceList
        if (refreshedPeers != peers) {
            var hasLock = false
            try {
                if (!peerMutex.tryLock()) {
                    return@PeerListListener
                }
                hasLock = true

                peers.clear()
                peers.addAll(refreshedPeers)

                val nameList = mutableListOf<String>()
                peerList.deviceList.forEach { device ->
                    nameList.add(device.deviceName)
                }

                // activity.setDiscoveredDevices(nameList)
                for (i in 0..(peers.size - 1)) {
                    if (peers[i].deviceName == "ENI-11145695" || peers[i].deviceName == "DESKTOP-33O1PJN") {
                        connect(i)
                        break
                    }
                }
            } finally {
                if (hasLock) {
                    peerMutex.unlock()
                }
            }
        }

        if (peers.isEmpty()) {
            Log.i(null, "WIFIDIRECT - peerListListener: No devices found")
            return@PeerListListener
        }
    }

    @SuppressLint("MissingPermission")
    @RequiresPermission(allOf = [Manifest.permission.ACCESS_FINE_LOCATION])
    override fun onReceive(context: Context, intent: Intent) {
        when(intent.action) {
            WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION -> {
                // Determine if Wi-Fi Direct mode is enabled or not, alert
                // the Activity.
                val state = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE, -1)
                isWifiP2pEnabled = state == WifiP2pManager.WIFI_P2P_STATE_ENABLED

                Log.i(null, "WIFIDIRECT - onReceive: WIFI_P2P_STATE_CHANGED_ACTION fired. Changed from $oldState to $state")
                oldState = state
            }
            WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION -> {

                // The peer list has changed! We should probably do something about
                // that.
                // Log.i(null, "WIFIDIRECT - onReceive: WIFI_P2P_PEERS_CHANGED_ACTION fired.")

                manager.requestPeers(channel, peerListListener)
            }
            WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION -> {

                // Connection state changed! We should probably do something about
                // that.

                try {
                    Log.i(null, "WIFIDIRECT - onReceive: WIFI_P2P_CONNECTION_CHANGED_ACTION fired.")

                    manager.let { manager ->

                        val networkInfo: NetworkInfo? = intent.getParcelableExtra(
                            WifiP2pManager.EXTRA_NETWORK_INFO)

                        if (networkInfo != null) {
                            Log.w(null, "WIFIDIRECT - connect: Got network info $networkInfo")
                        }

                        val groupInfo: WifiP2pGroup? = intent.getParcelableExtra(
                            WifiP2pManager.EXTRA_WIFI_P2P_GROUP)

                        if (groupInfo != null) {
                            Log.w(null, "WIFIDIRECT - connect: Got group info $groupInfo")
                        }

                        val p2pInfo: WifiP2pInfo? = intent.getParcelableExtra(
                            WifiP2pManager.EXTRA_WIFI_P2P_INFO)

                        if (p2pInfo != null) {
                            Log.w(null, "WIFIDIRECT - connect: Got p2p info $p2pInfo")
                        }

                        // Replacing this with ConnectivityManager doesn't look like it will work
                        // When listening for changes on wifi/p2p networks there nothing happens
                        manager.requestNetworkInfo(channel) { info ->
                            if (info.isConnected == true) {

                                // We are connected with the other device, request connection
                                // info to find group owner IP

                                Log.i(null, "WIFIDIRECT - onReceive: Network is connected.")
                                manager.requestConnectionInfo(channel, connectionListener)
                            } else {
                                Log.w(null, "WIFIDIRECT - onReceive: Network isn't connected.")
                            }
                        }
                    }
                } catch (ex: Exception) {
                    Log.e(null, "WIFIDIRECT - onReceive: Exception message: ${ex.message}")
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

                Log.i(null, "WIFIDIRECT - onReceive: WIFI_P2P_THIS_DEVICE_CHANGED_ACTION fired.")
            }
        }
    }

    @SuppressLint("MissingPermission")
    @RequiresPermission(allOf = [Manifest.permission.ACCESS_FINE_LOCATION])
    fun connect(idx: Int) {
        try {
            if (!connectMutex.tryLock()) {
                return
            }

            // Picking the first device found on the network.
            val device = peers[idx]

            Log.i(null, "WIFIDIRECT - MyBroadcastReceiver.connect: Connecting to device ${device.deviceName}")

            val macAddress = MacAddress.fromString(device.deviceAddress)

            manager.requestGroupInfo(channel) { groupInfo ->
                if (groupInfo == null) {
                    Log.i(null, "WIFIDIRECT - connect.requestGroupInfo on connect is null")
                    subConnect("tempPass", "DIRECT-EmptyGroupGroupName", macAddress, device)
                } else {
                    Log.i(
                        null,
                        "WIFIDIRECT, - connect.requestGroupInfo set group password to ${groupInfo.passphrase} and group name ${groupInfo.networkName}"
                    )
                    subConnect(groupInfo.passphrase, groupInfo.networkName, macAddress, device)
                }
            }
        } catch (ex: Exception) {
            Log.e(null, "WIFIDIRECT - connect.Exception: ${ex.message}")
        }
    }

    @SuppressLint("MissingPermission")
    fun subConnect(passphrase: String, networkName: String, macAddress: MacAddress, device: WifiP2pDevice) {
//        val config = WifiP2pConfig.Builder()
//            .setPassphrase("1234567890\\@K\$J~si1kdzn?]Bw@[uo7RytxYjFF|)R(`uU7T%4=XK%\"q!qUtS+")
////            .setPassphrase("tempPass")
////            .setPassphrase("")
////            .setPassphrase(passphrase)
//            .setDeviceAddress(macAddress)
//            .enablePersistentMode(false) // Reconnecting with a fully paired device fails
//            // disable persistent mode to prevent saving the
//            // pairing info on the Android side
////            .setNetworkName(networkName)
//            .setNetworkName("DIRECT-MrLinkNetwork")
////            .setNetworkName("DIRECT-Meta-9x8f")
////            .setNetworkName("")
//            .setGroupOperatingBand(WifiP2pConfig.GROUP_OWNER_BAND_5GHZ)
//            .build()

        val config = WifiP2pConfig()

//        config.networkId = WifiP2pGroup.NETWORK_ID_TEMPORARY
//        val netId = WifiP2pConfig::class.declaredMemberProperties.find { it.name == "netId" } as KMutableProperty<Any>

        config.wps.setup = WpsInfo.PBC
        config.groupOwnerIntent = WifiP2pConfig.GROUP_OWNER_INTENT_MIN
        config.deviceAddress = device.deviceAddress

        val createGroup = false

        manager.removeGroup(channel, object : WifiP2pManager.ActionListener {
            override fun onSuccess() {
                // WiFiDirectBroadcastReceiver notifies us. Ignore for now.
                Log.i(null, "WIFIDIRECT - removeGroup.onSuccess")
                if (createGroup) {
                    //createGroupInternal(config)
                } else {
                    connectInternal(config)
                }
            }

            override fun onFailure(reason: Int) {
                Log.e(null, "WIFIDIRECT - removeGroup.onFailure: $reason")
                if (createGroup) {
                    //createGroupInternal(config)
                } else {
                    connectInternal(config)
                }
            }
        })
    }

    @SuppressLint("MissingPermission")
    fun createGroupInternal(config: WifiP2pConfig) {
        manager.createGroup(channel, config, object : WifiP2pManager.ActionListener {

            override fun onSuccess() {
                // WiFiDirectBroadcastReceiver notifies us. Ignore for now.
                Log.i(null, "WIFIDIRECT - createGroup.onSuccess")

                manager.requestConnectionInfo(channel, connectionListener)
                manager.requestNetworkInfo(channel) { networkInfo ->
                    Log.i(
                        null,
                        "WIFIDIRECT, - createGroup.requestNetworkInfo got network info $networkInfo"
                    )
                }
                manager.requestDeviceInfo(channel) { deviceInfo ->
                    Log.i(
                        null,
                        "WIFIDIRECT, - createGroup.requestDeviceInfo got device info $deviceInfo"
                    )
                }
                manager.requestGroupInfo(channel) { groupInfo ->
                    Log.i(
                        null,
                        "WIFIDIRECT, - createGroup.requestGroupInfo got group info $groupInfo"
                    )
                }
                manager.requestP2pState(channel) { p2pState ->
                    Log.i(
                        null,
                        "WIFIDIRECT, - createGroup.requestNetworkInfo got p2pState $p2pState"
                    )
                }
            }

            override fun onFailure(reason: Int) {
                Log.e(null, "WIFIDIRECT - createGroup.onFailure: $reason")
            }
        })
    }

    @SuppressLint("MissingPermission")
    fun connectInternal(config: WifiP2pConfig) {
        manager.connect(channel, config, object : WifiP2pManager.ActionListener {

            override fun onSuccess() {
                // WiFiDirectBroadcastReceiver notifies us. Ignore for now.
                Log.i(null, "WIFIDIRECT - connect.onSuccess")

                manager.requestConnectionInfo(channel, connectionListener)
//                manager.requestNetworkInfo(channel) { networkInfo ->
//                    Log.i(
//                        null,
//                        "WIFIDIRECT, - connect.requestNetworkInfo got network info $networkInfo"
//                    )
//                }
//                manager.requestDeviceInfo(channel) { deviceInfo ->
//                    Log.i(
//                        null,
//                        "WIFIDIRECT, - connect.requestDeviceInfo got device info $deviceInfo"
//                    )
//                }
//                manager.requestGroupInfo(channel) { groupInfo ->
//                    Log.i(
//                        null,
//                        "WIFIDIRECT, - connect.requestGroupInfo got group info $groupInfo"
//                    )
//                }
//                manager.requestP2pState(channel) { p2pState ->
//                    Log.i(
//                        null,
//                        "WIFIDIRECT, - connect.requestNetworkInfo got p2pState $p2pState"
//                    )
//                }
            }

            override fun onFailure(reason: Int) {
                Log.e(null, "WIFIDIRECT - connect.onFailure: $reason")
            }
        })
    }

    @SuppressLint("MissingPermission")
    @Suppress("NULLABILITY_MISMATCH_BASED_ON_JAVA_ANNOTATIONS")
    private val connectionListener = WifiP2pManager.ConnectionInfoListener { info ->


        // After the group negotiation, we can determine the group owner
        // (server).

        manager.requestGroupInfo(channel) { groupInfo ->
            Log.i(null, "WIFIDIRECT - ConnectionListener: Group Info: $groupInfo")
        }

        if (info.groupFormed && info.isGroupOwner) {
            // Do whatever tasks are specific to the group owner.
            // One common case is creating a group owner thread and accepting
            // incoming connections.
            Log.i(null, "WIFIDIRECT - ConnectionListener: Group formed as group owner")

        } else if (info.groupFormed) {
            // The other device acts as the peer (client). In this case,
            // you'll want to create a peer thread that connects
            // to the group owner.
            Log.i(null, "WIFIDIRECT - ConnectionListener: Group formed as group member")

            // Will be picked up by the KeepAliveWiFiDirectService foreground service
            while (!socketMutex.tryLock()) {
                Thread.sleep(1)
            }

            // String from WifiP2pInfo struct
            val groupOwnerAddress: String = info.groupOwnerAddress.hostAddress

            val hostNetworkParts = groupOwnerAddress.split(".")
            var hostNetworkStart = ""
            if (hostNetworkParts.size == 4) {
                hostNetworkStart = hostNetworkParts[0] + "." + hostNetworkParts[1] + "." + hostNetworkParts[2] + "."
            }

            if (hostNetworkStart == "") {
                socketMutex.unlock()
                return@ConnectionInfoListener
            }

            var localAddress = ""
            var attemptNumber = 1
            var maxAttempts = 3
            do {
                for (netinterface in NetworkInterface.getNetworkInterfaces()) {
                    for (inetaddress in netinterface.inetAddresses) {
                        Log.i(null, "WIFIDIRECT - ConnectionListener: Found local address of: ${inetaddress.hostAddress}")
                        if (inetaddress?.hostAddress?.startsWith(hostNetworkStart) == true) {
                            localAddress = inetaddress.hostAddress
                            Log.i(null, "WIFIDIRECT - ConnectionListener: Found fitting local address of: ${inetaddress.hostAddress}")
                            break
                        }
                    }
                    if (localAddress != "") {
                        break
                    }
                }

                attemptNumber++

                if (localAddress == "") {
                    Log.i(null, "WIFIDIRECT - ConnectionListener: Found no fitting local address on attempt $attemptNumber of $maxAttempts. Will wait a second and try again.")
                    Thread.sleep(1000)
                }

            } while (localAddress == "" && attemptNumber <= maxAttempts)

            if (localAddress == "") {
                socketMutex.unlock()
                Log.e(null, "WIFIDIRECT - ConnectionListener: Got no fitting local address")
                return@ConnectionInfoListener
            }

            while (wifiDirectNetwork.isEmpty) {
                Thread.sleep(1000)
                Log.i(null, "WIFIDIRECT - wifiDirectNetwork is empty. waiting")
            }

            Log.i(null, "WIFIDIRECT - start creating sockets")

            connectivityManager.bindProcessToNetwork(wifiDirectNetwork.get())

            networkKeepAlive = NetworkKeepAlive(wifiDirectNetwork.get(), connectivityManager)
            networkKeepAlive!!.start()

            keepAliveSocket = KeepAliveSocket(groupOwnerAddress, 50001, localAddress, wifiDirectNetwork.get(), connectivityManager)
            dataTransferSocket = ClientSocket(groupOwnerAddress, 50011, localAddress, wifiDirectNetwork.get(), keepAliveSocket!!)

            socketMutex.unlock()
        } else {
            Log.i(null, "WIFIDIRECT - ConnectionListener: Something else $info")
        }
    }

    fun checkAllNetworks() {
        for (netw in connectivityManager.allNetworks) {
            Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - $netw")
            val netCap = connectivityManager.getNetworkCapabilities(netw)
            Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - net caps - $netCap")
            val netInfo = connectivityManager.getNetworkInfo(netw)
            Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - net info - $netInfo")

            if (netCap?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true && netCap.hasCapability(
                    NetworkCapabilities.NET_CAPABILITY_VALIDATED) != true
            ) {
                Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - chose network $netw")
            }

            if (netCap?.hasTransport(NetworkCapabilities.TRANSPORT_USB) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has transport NetworkCapabilities.TRANSPORT_USB") }
            if (netCap?.hasTransport(NetworkCapabilities.TRANSPORT_VPN) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has transport NetworkCapabilities.TRANSPORT_VPN") }
            if (netCap?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has transport NetworkCapabilities.TRANSPORT_WIFI") }
            if (netCap?.hasTransport(NetworkCapabilities.TRANSPORT_LOWPAN) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has transport NetworkCapabilities.TRANSPORT_LOWPAN") }
            if (netCap?.hasTransport(NetworkCapabilities.TRANSPORT_THREAD) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has transport NetworkCapabilities.TRANSPORT_THREAD") }
            if (netCap?.hasTransport(NetworkCapabilities.TRANSPORT_BLUETOOTH) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has transport NetworkCapabilities.TRANSPORT_BLUETOOTH") }
            if (netCap?.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has transport NetworkCapabilities.TRANSPORT_CELLULAR") }
            if (netCap?.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has transport NetworkCapabilities.TRANSPORT_ETHERNET") }
            if (netCap?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI_AWARE) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has transport NetworkCapabilities.TRANSPORT_WIFI_AWARE") }

            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_MMS) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_MMS") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_SUPL) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_SUPL") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_DUN) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_DUN") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_FOTA) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_FOTA") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_IMS) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_IMS") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_CBS) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_CBS") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_WIFI_P2P) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_WIFI_P2P") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_IA) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_IA") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_RCS) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_RCS") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_XCAP) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_XCAP") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_EIMS) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_EIMS") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_NOT_METERED") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_INTERNET") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_NOT_RESTRICTED") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_TRUSTED) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_TRUSTED") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_NOT_VPN") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_VALIDATED") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_CAPTIVE_PORTAL") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_ROAMING) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_NOT_ROAMING") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_FOREGROUND) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_FOREGROUND") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_CONGESTED) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_NOT_CONGESTED") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_SUSPENDED) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_NOT_SUSPENDED") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_MCX) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_MCX") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_TEMPORARILY_NOT_METERED) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_TEMPORARILY_NOT_METERED") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_ENTERPRISE) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_ENTERPRISE") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_HEAD_UNIT) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_HEAD_UNIT") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_MMTEL) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_MMTEL") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_PRIORITIZE_LATENCY) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_PRIORITIZE_LATENCY") }
            if (netCap?.hasCapability(NetworkCapabilities.NET_CAPABILITY_PRIORITIZE_BANDWIDTH) == true) { Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - has network capability NET_CAPABILITY_PRIORITIZE_BANDWIDTH") }
        }
    }
}