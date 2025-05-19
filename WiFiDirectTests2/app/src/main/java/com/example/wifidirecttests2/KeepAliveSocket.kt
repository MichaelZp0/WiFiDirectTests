package com.example.wifidirecttests2

import android.util.Log

class KeepAliveSocket(val hostAddress: String, val port: Int): MySocket(KeepAliveSocket::class.java.name) {

    private var shouldRun = true

    fun stopSocket() {
        shouldRun = false
    }

    override fun run() {
        if (!connect(hostAddress, port, 20000)) {
            return
        }

        try {
            if (!readMessage()) {
                return
            }

            write("Hello from $socketName")

            Log.i(null, "$socketName.Run: HELO done between server and client")

//            while (shouldRun) {
//                if (readMessage(false)) {
//                    write("KeepAliveACK")
//                }
//                sleep(5000)
//            }

            while (shouldRun) {
                if (!write("KeepAlive")) {
                    break
                }
                sleep(5000)
            }

        } catch (ex: Exception) {
            ex.printStackTrace()
            Log.e(null, "$socketName.Run: Exception: '${ex.message}'")
        }

        socket.close()
        connectionState = ConnectionState.Disconnected
    }
}