package com.example.wifidirecttests2

import android.util.Log
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.net.InetSocketAddress
import java.net.Socket
import kotlin.experimental.and

class ClientSocket(val hostAddress: String, val port: Int, val activity: MainActivity): Thread() {

    lateinit var inputStream: InputStream
    lateinit var outputStream: OutputStream
    lateinit var socket: Socket

    fun write(string: String) {
        try {
            // activity.showInfo("ClientSocket.write", "Sending message: '$string'")
            Log.i(null, "ClientSocket.write: Sending message: '$string'")

            val lengthArray = ByteArray(4)

            lengthArray[3] = (string.length and 0xFF000000.toInt() shr 24).toByte()
            lengthArray[2] = (string.length and 0x00FF0000.toInt() shr 16).toByte()
            lengthArray[1] = (string.length and 0x0000FF00.toInt() shr 8).toByte()
            lengthArray[0] = (string.length and 0x000000FF.toInt() shr 0).toByte()

            outputStream.write(lengthArray)
            outputStream.write(string.toByteArray())
        } catch (ex:IOException) {
            ex.printStackTrace()
            Log.e(null, "ClientSocket.write: IO exception: '${ex.message}'")
            // activity.showError("ClientSocket.write", "IO exception: '${ex.message}'")
        }
    }

    fun readMessage() : Boolean {
        val u32Buffer = ByteArray(4)
        val buffer = ByteArray(1024)
        var msgLength = 0
        var readBytes = 0
        try {

            msgLength = 0
            msgLength += inputStream.read() shl 0
            msgLength += inputStream.read() shl 8
            msgLength += inputStream.read() shl 16
            msgLength += inputStream.read() shl 24

            Log.i(null, "ClientSocket.ReadMessage: Got message of length: $msgLength")

            if (msgLength == -1) {
                Log.i(null, "ClientSocket.ReadMessage: Server closed connection")
                // activity.showInfo("ClientSocket.run", "Server closed connection")
                return false
            }

            if (msgLength > 0) {

                while (true) {
                    readBytes += inputStream.read(buffer, readBytes, msgLength - readBytes)

                    if (readBytes >= msgLength) {
                        val tmpMessage = String(buffer, 0, msgLength)
                        Log.i(null, "ClientSocket.ReadMessage: Message: '$tmpMessage'")
                        // activity.showInfo("ClientSocket.run", "Message: '$tmpMessage'")
                        break
                    }
                }
            }
        } catch (ex:IOException) {
            ex.printStackTrace()
            Log.e(null, "ClientSocket.run: Exception: '${ex.message}'")
            // activity.showError("ClientSocket.run", "Exception: '${ex.message}'")
        }

        return true
    }

    override fun run() {
        try {
            Log.i(null, "ClientSocket.Init: Waiting for server to start listening")
            sleep(7500)
            Log.i(null, "ClientSocket.Init: Trying to connect")

            socket = Socket()
            socket.connect(InetSocketAddress(hostAddress, port), 500)
            inputStream = socket.getInputStream()
            outputStream = socket.getOutputStream()
        } catch (ex:IOException) {
            ex.printStackTrace()
            Log.e(null, "ClientSocket.Init: Exception: '${ex.message}'")
            // activity.showError("ClientSocket.Init", "Exception: '${ex.message}'")
        }

        try {
            if (!readMessage()) {
                return
            }

            write("Hello")

            // activity.showInfo("ClientSocket.run", "HELO done between server/client.")

            sleep(1000)
            write("3...")
            sleep(1000)
            write("2...")
            sleep(1000)
            write("1...")
            sleep(1000)
            write("bye")
            sleep(1000)
        } catch (ex: Exception) {
            ex.printStackTrace()
            Log.e(null, "ClientSocket.ReadWrite: Exception: '${ex.message}'")
            // activity.showError("ClientSocket.ReadWrite", "Exception: '${ex.message}'")
        }

        socket.close()
    }

}