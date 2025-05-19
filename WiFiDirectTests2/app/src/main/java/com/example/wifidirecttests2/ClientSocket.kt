package com.example.wifidirecttests2

import android.util.Log

class ClientSocket(val hostAddress: String, val port: Int, val keepAliveSocket: KeepAliveSocket): MySocket(ClientSocket::class.java.name) {

    override fun run() {
        if (!connect(hostAddress, port, 60000)) {
            return
        }

        try {
            if (!readMessage()) {
                return
            }

            write("Hello from $socketName")

            Log.i(null, "$socketName.Run: HELO done between server and client")

            while (readMessage()) {
                // Read message returns false if connection closes
                write("ClientSocketACK")
            }
        } catch (ex: Exception) {
            ex.printStackTrace()
            Log.e(null, "$socketName.Run: Exception: '${ex.message}'")
        }

        socket.close()
        connectionState = ConnectionState.Disconnected
        keepAliveSocket.stopSocket()
    }
}