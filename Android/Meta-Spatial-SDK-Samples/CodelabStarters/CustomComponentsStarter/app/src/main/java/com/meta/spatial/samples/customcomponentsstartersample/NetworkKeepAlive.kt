package com.meta.spatial.samples.customcomponentsstartersample

import android.net.ConnectivityManager
import android.net.Network
import android.util.Log
import java.net.Socket
import java.net.URL

class NetworkKeepAlive(val network: Network, val connectivityManager: ConnectivityManager): Thread() {
    private var shouldRun = true

    fun stopSocket() {
        shouldRun = false
    }

    override fun run() {
        while (shouldRun) {
            try {
//                network.bindSocket(Socket())
//                network.openConnection(URL("www.noting-should-exist-here-right-now-in-this-place-aslkjsakdjflaksdjf.co.ui.ce.de.com.fin"))

//                for (netw in connectivityManager.allNetworks) {
//                    Log.i(null, "WIFIDIRECT - NetworkKeepAlive - Found network: $netw")
//                }
            } catch (ex: Exception) {
                // Do nothing
                Log.e(null, "WIFIDIRECT - NetworkKeepAlive - Error: ${ex.message}")
            }
            sleep(2000)
        }
    }
}