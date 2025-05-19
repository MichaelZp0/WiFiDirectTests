/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.meta.spatial.samples.customcomponentsstartersample

import android.Manifest
import android.annotation.SuppressLint
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.Uri
import android.net.wifi.p2p.WifiP2pConfig
import android.net.wifi.p2p.WifiP2pManager
import android.os.Bundle
import android.util.Log
import androidx.core.app.ActivityCompat
import com.meta.spatial.castinputforward.CastInputForwardFeature
import com.meta.spatial.core.Entity
import com.meta.spatial.core.Pose
import com.meta.spatial.core.SpatialFeature
import com.meta.spatial.core.Vector3
import com.meta.spatial.datamodelinspector.DataModelInspectorFeature
import com.meta.spatial.debugtools.HotReloadFeature
import com.meta.spatial.okhttp3.OkHttpAssetFetcher
import com.meta.spatial.ovrmetrics.OVRMetricsDataModel
import com.meta.spatial.ovrmetrics.OVRMetricsFeature
import com.meta.spatial.runtime.LayerConfig
import com.meta.spatial.runtime.NetworkedAssetLoader
import com.meta.spatial.runtime.ReferenceSpace
import com.meta.spatial.runtime.SceneMaterial
import com.meta.spatial.runtime.panel.style
import com.meta.spatial.toolkit.AppSystemActivity
import com.meta.spatial.toolkit.Material
import com.meta.spatial.toolkit.Mesh
import com.meta.spatial.toolkit.MeshCollision
import com.meta.spatial.toolkit.PanelRegistration
import com.meta.spatial.toolkit.Transform
import com.meta.spatial.vr.VRFeature
import java.io.File
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlin.reflect.KMutableProperty
import kotlin.reflect.full.declaredMemberProperties

// default activity
class CustomComponentsSampleActivity : AppSystemActivity() {
    private val intentFilter = IntentFilter()

    private lateinit var channel: WifiP2pManager.Channel
    private lateinit var manager: WifiP2pManager
//    private var permissionsToRequest = arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.NEARBY_WIFI_DEVICES, Manifest.permission.ACCESS_NETWORK_STATE)
    private var permissionsToRequest = arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_NETWORK_STATE)

    private var gltfxEntity: Entity? = null
    private val activityScope = CoroutineScope(Dispatchers.Main)

    companion object {
        const val CHANNEL_ID = "KeepAliveWiFiDirectServiceChannelId"
        private lateinit var receiver: MyBroadcastReceiver
    }

    override fun registerFeatures(): List<SpatialFeature> {
        val features = mutableListOf<SpatialFeature>(VRFeature(this))
        if (BuildConfig.DEBUG) {
            features.add(CastInputForwardFeature(this))
            features.add(HotReloadFeature(this))
            features.add(
                OVRMetricsFeature(
                    this,
                    OVRMetricsDataModel() {
                        numberOfMeshes()
                        numberOfGrabbables()
                    },
                    LookAtMetrics {
                        pos()
                        pitch()
                        yaw()
                        roll()
                    })
            )
            features.add(DataModelInspectorFeature(spatial, this.componentManager))
        }
        return features
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        NetworkedAssetLoader.init(
            File(applicationContext.getCacheDir().canonicalPath), OkHttpAssetFetcher()
        )

        // TODO: register the LookAt system and component

        loadGLXF().invokeOnCompletion {
            val composition = glXFManager.getGLXFInfo("example_key_name")

            // set the environment to be unlit
            val environmentEntity: Entity? = composition.getNodeByName("Environment").entity
            val environmentMesh = environmentEntity?.getComponent<Mesh>()
            environmentMesh?.defaultShaderOverride = SceneMaterial.UNLIT_SHADER
            environmentEntity?.setComponent(environmentMesh!!)

            // TODO: get the robot and the basketBall entities from the composition
        }

        doForegroundStuff()

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.FOREGROUND_SERVICE) == PackageManager.PERMISSION_GRANTED) {
            Log.i(null, "WIFIDIRECT - Trying to start foreground service...")
            startForegroundService(Intent(this, KeepAliveWiFiDirectService::class.java))
            startService(Intent(this, KeepAliveWiFiDirectService::class.java))
        } else {
            Log.w(null, "WIFIDIRECT - Permissions for foreground service are missing.")
        }

        doConnectivityManagerStuff()

        doWiFiDirectStuff()
    }

    fun doConnectivityManagerStuff() {
//        val mgr = getSystemService(CONNECTIVITY_SERVICE) as ConnectivityManager
//        mgr.registerNetworkCallback(NetworkRequest.Builder().addCapability(NetworkCapabilities.NET_CAPABILITY_WIFI_P2P).build(), object : ConnectivityManager.NetworkCallback() {
//            override fun onAvailable(network: Network) {
//                super.onAvailable(network)
//                Log.i(null, "WIFIDIRECT - ConnectivityManager - p2p network available")
//                receiver.updateNetwork(network)
//            }
//
//            override fun onUnavailable() {
//                super.onUnavailable()
//            }
//        })
//
//        mgr.req
    }

    @SuppressLint("MissingPermission")
    fun doWiFiDirectStuff() {

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

        receiver = MyBroadcastReceiver(channel, manager, this, getSystemService(CONNECTIVITY_SERVICE) as ConnectivityManager)
        registerReceiver(receiver, intentFilter)

        ActivityCompat.requestPermissions(this, permissionsToRequest, 1234)

//        val permissionsRequest = registerForActivityResult(
//            ActivityResultContracts.RequestMultiplePermissions()) { results ->
//            var allPermsGranted = true
//            results.forEach { result ->
//                if (result.value) {
//                    showInfo("btnStartDiscovery", "Permission ${result.key} is now granted after requesting it.")
//                }
//                else {
//                    showError("btnStartDiscovery", "Permission ${result.key} was denied after requesting it.")
//                    allPermsGranted = false
//                }
//            }
//
//            if (allPermsGranted) {
//                checkPermsAndStartDiscovery(false)
//            }
//        }

        var allPermissionsGranted = true
        permissionsToRequest.forEach { perm ->
            if (ActivityCompat.checkSelfPermission(this, perm) == PackageManager.PERMISSION_DENIED) {
                Log.w(null, "WIFIDIRECT - Permission $perm is missing")
                allPermissionsGranted = false
            }
        }

        if (allPermissionsGranted) {
            receiver.discoverPeers()

//            receiver.createGroup()

            Log.i(null, "WIFIDIRECT - Starting device discovery")
        } else {
            Log.w(null, "WIFIDIRECT - Device discorvery failed due to missing permissions")
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        when (requestCode) {
            1234 -> {
                if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    // Permission granted, proceed with location-related tasks
                } else {
                    // Permission denied, handle accordingly
                }
            }
        }
    }

    fun doForegroundStuff() {
//        var foregroundServicePermReq = registerForActivityResult(
//            ActivityResultContracts.RequestMultiplePermissions()) { results ->
//            var allPermsGranted = true
//            results.forEach { result ->
//                if (result.value) {
//                    showInfo("btnRequestNotifyPerms", "Permission ${result.key} is now granted after requesting it.")
//                }
//                else {
//                    showError("btnRequestNotifyPerms", "Permission ${result.key} was denied after requesting it.")
//                    allPermsGranted = false
//                }
//            }
//        }
//
//        foregroundServicePermReq.launch(notifyPermissionsToRequest)

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

    override fun onResume() {
        super.onResume()
        receiver = MyBroadcastReceiver(channel, manager, this, getSystemService(CONNECTIVITY_SERVICE) as ConnectivityManager)

        registerReceiver(receiver, intentFilter)
    }

    override fun onPause() {
        super.onPause()
        unregisterReceiver(receiver)
    }

    override fun registerPanels(): List<PanelRegistration> {
        return listOf(
            PanelRegistration(R.layout.ui_example) { entity ->
                config {
                    themeResourceId = R.style.PanelAppThemeTransparent
                    includeGlass = false
                    width = 2.0f
                    height = 1.5f
                    layerConfig = LayerConfig()
                    enableTransparent = true
                }
            })
    }

    override fun onSceneReady() {
        super.onSceneReady()

        // set the reference space to enable recentering
        scene.setReferenceSpace(ReferenceSpace.LOCAL_FLOOR)

        scene.setLightingEnvironment(
            ambientColor = Vector3(0f),
            sunColor = Vector3(7.0f, 7.0f, 7.0f),
            sunDirection = -Vector3(1.0f, 3.0f, -2.0f),
            environmentIntensity = 0.3f
        )
        scene.updateIBLEnvironment("environment.env")

        scene.setViewOrigin(0.0f, 0.0f, -1.0f, 0.0f)

        Entity.create(
            listOf(
                Mesh(Uri.parse("mesh://skybox"), hittable = MeshCollision.NoCollision),
                Material().apply {
                    baseTextureAndroidResourceId = R.drawable.skydome
                    unlit = true // Prevent scene lighting from affecting the skybox
                },
                Transform(Pose(Vector3(x = 0f, y = 0f, z = 0f)))
            )
        )
    }

    private fun loadGLXF(): Job {
        gltfxEntity = Entity.create()
        return activityScope.launch {
            glXFManager.inflateGLXF(
                Uri.parse("apk:///scenes/Composition.glxf"),
                rootEntity = gltfxEntity!!,
                keyName = "example_key_name"
            )
        }
    }
}
