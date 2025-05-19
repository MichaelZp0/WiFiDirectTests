package com.meta.spatial.samples.customcomponentsstartersample

import android.net.Network
import android.util.Log
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.net.InetSocketAddress
import java.net.Socket

open class MySocket(protected val socketName: String): Thread() {

    enum class ConnectionState {
        Unknown,
        Connecting,
        Connected,
        Disconnected,
        Error
    }

    var connectionState: ConnectionState = ConnectionState.Unknown
        protected set

    protected lateinit var inputStream: InputStream
    protected lateinit var outputStream: OutputStream
    protected lateinit var socket: Socket

    fun write(string: String): Boolean {
        try {
            Log.i(null, "WIFIDIRECT - $socketName.write: Sending message: '$string'")

            val lengthArray = ByteArray(4)

            lengthArray[3] = (string.length and 0xFF000000.toInt() shr 24).toByte()
            lengthArray[2] = (string.length and 0x00FF0000.toInt() shr 16).toByte()
            lengthArray[1] = (string.length and 0x0000FF00.toInt() shr 8).toByte()
            lengthArray[0] = (string.length and 0x000000FF.toInt() shr 0).toByte()

            outputStream.write(lengthArray)
            outputStream.write(string.toByteArray())
        } catch (ex:IOException) {
            ex.printStackTrace()
            Log.e(null, "WIFIDIRECT - $socketName.write: IO exception: '${ex.message}'")
            return false
        }

        return true
    }

    fun readMessage(blockOnNoInput: Boolean = true) : Boolean {
        val buffer = ByteArray(1024)
        var msgLength = 0
        var readBytes = 0
        try {
            if (!blockOnNoInput) {
                if (inputStream.available() < 4) {
                    return false
                }
            }

            msgLength = 0
            for (i in 0..3) {
                val tempValue = inputStream.read() shl (i * 8)

                if (tempValue == -1) {
                    Log.i(null, "WIFIDIRECT - $socketName.ReadMessage: Server closed connection")
                    return false
                }

                msgLength += tempValue
            }

            Log.i(null, "WIFIDIRECT - $socketName.ReadMessage: Got message of length: $msgLength")

            if (msgLength > 0) {

                while (true) {
                    readBytes += inputStream.read(buffer, readBytes, msgLength - readBytes)

                    if (readBytes >= msgLength) {
                        val tmpMessage = String(buffer, 0, msgLength)
                        Log.i(null, "WIFIDIRECT - $socketName.ReadMessage: Message: '$tmpMessage'")
                        break
                    }
                }
            }
        } catch (ex:IOException) {
            ex.printStackTrace()
            Log.e(null, "WIFIDIRECT - $socketName.ReadMessage: Exception: '${ex.message}'")
        }

        return true
    }

    protected fun connect(hostAddress: String, port: Int, localAddress: String, network: Network, @Suppress("SameParameterValue") timeout: Int): Boolean {
        connectionState = ConnectionState.Connecting

        var connected = false

        for (i in 1..3) {
            try {
                socket = Socket()
//                network.bindSocket(socket)
                Log.i(null, "WIFIDIRECT - $socketName.Connect: Trying to connect")
                socket.bind(InetSocketAddress(localAddress, 0))
                socket.connect(InetSocketAddress(hostAddress, port), timeout)
                inputStream = socket.getInputStream()
                outputStream = socket.getOutputStream()
                Log.i(null, "WIFIDIRECT - $socketName.Connect: Connected")
                connected = true
                break
            } catch (ex:IOException) {
                ex.printStackTrace()
                if (i != 3) {
                    Log.i(null, "WIFIDIRECT - $socketName.Connect: Failed to connect on attempt $i. Retrying.")
                } else {
                    Log.i(null, "WIFIDIRECT - $socketName.Connect: Failed to connect on attempt $i. Aborting.")
                }
                Log.e(null, "WIFIDIRECT - $socketName.Connect: Exception: '${ex.message}'")
                connectionState = ConnectionState.Error
            }
        }

        connectionState = ConnectionState.Connected
        return connected
    }
}