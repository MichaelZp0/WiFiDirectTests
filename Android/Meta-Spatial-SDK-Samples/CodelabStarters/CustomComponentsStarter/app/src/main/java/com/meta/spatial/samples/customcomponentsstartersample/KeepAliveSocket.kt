package com.meta.spatial.samples.customcomponentsstartersample

import android.net.ConnectivityManager
import android.net.Network
import android.util.Log

class KeepAliveSocket(val hostAddress: String, val port: Int, val localAddress: String, val network: Network, val connectivityManager: ConnectivityManager): MySocket(KeepAliveSocket::class.java.name) {

    private var shouldRun = true

    fun stopSocket() {
        shouldRun = false
    }

    override fun run() {
        if (!connect(hostAddress, port, localAddress, network, 20000)) {
            return
        }

        try {
            if (!readMessage()) {
                return
            }

            write("Hello from $socketName")

            Log.i(null, "WIFIDIRECT - $socketName.Run: HELO done between server and client")

            while (shouldRun) {
                if (!write("KeepAlive")) {
                    break
                }
//                sleep(5000)
                sleep(1000)
                if (!socket.isConnected) {
                    Log.i(null, "WIFIDIRECT - $socketName.Run: Socket is no longer connected")
                    break
                }
                if (socket.isClosed) {
                    Log.i(null, "WIFIDIRECT - $socketName.Run: Socket is closed")
                    break
                }
                Log.i(null, "WIFIDIRECT - $socketName.Run: Socket: $socket")
                Log.i(null, "WIFIDIRECT - $socketName.Run: Active network: ${connectivityManager.activeNetwork}")
                Log.i(null, "WIFIDIRECT - $socketName.Run: Network: $network")
                val netCap = connectivityManager.getNetworkCapabilities(network)
                Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - net caps - $netCap")
                val netInfo = connectivityManager.getNetworkInfo(network)
                Log.i(null, "WIFIDIRECT - ConnectivityManager - get all networks - net info - $netInfo")


                socket.outputStream.flush()
            }

        } catch (ex: Exception) {
            ex.printStackTrace()
            Log.e(null, "WIFIDIRECT - $socketName.Run: Exception: '${ex.message}'")
        }

        socket.close()
        connectionState = ConnectionState.Disconnected
    }
}