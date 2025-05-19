package com.meta.spatial.samples.customcomponentsstartersample

import android.net.Network
import android.util.Log

class ClientSocket(val hostAddress: String, val port: Int, val localAddress: String, val network: Network, val keepAliveSocket: KeepAliveSocket): MySocket(ClientSocket::class.java.name) {

    override fun run() {
        if (!connect(hostAddress, port, localAddress, network, 60000)) {
            return
        }

        try {
            if (!readMessage()) {
                return
            }

            write("Hello from $socketName")

            Log.i(null, "WIFIDIRECT - $socketName.Run: HELO done between server and client")

            while (readMessage()) {
                // Read message returns false if connection closes
                write("ClientSocketACK")
            }
        } catch (ex: Exception) {
            ex.printStackTrace()
            Log.e(null, "WIFIDIRECT - $socketName.Run: Exception: '${ex.message}'")
        }

        socket.close()
        connectionState = ConnectionState.Disconnected
        keepAliveSocket.stopSocket()
    }
}