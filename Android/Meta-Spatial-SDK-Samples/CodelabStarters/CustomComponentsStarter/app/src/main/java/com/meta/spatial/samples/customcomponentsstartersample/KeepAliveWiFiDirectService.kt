package com.meta.spatial.samples.customcomponentsstartersample

import android.app.Service
import android.content.Intent
import android.os.Handler
import android.os.HandlerThread
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Process
import android.util.Log
import androidx.core.app.NotificationCompat

class KeepAliveWiFiDirectService : Service() {

    private var serviceLooper: Looper? = null
    private var serviceHandler: ServiceHandler? = null

    private inner class ServiceHandler(looper: Looper) : Handler(looper) {
        override fun handleMessage(msg: Message) {
            super.handleMessage(msg)
            Log.i(null, "WIFIDIRECT - Enter service handler handleMessage")
            // Do work

            while (true) {
                Thread.sleep(250)

                if (!MyBroadcastReceiver.socketMutex.tryLock()) {
                    continue
                }

                if (MyBroadcastReceiver.keepAliveSocket == null && MyBroadcastReceiver.dataTransferSocket == null) {
                    MyBroadcastReceiver.socketMutex.unlock()
                    continue
                }

                MyBroadcastReceiver.socketMutex.unlock()
                break
            }

            Log.i(null, "WIFIDIRECT - Got sockets in foreground task")

            MyBroadcastReceiver.keepAliveSocket!!.start()
            MyBroadcastReceiver.dataTransferSocket!!.start()

            Log.i(null, "WIFIDIRECT - Started sockets in foreground task")

            while (true) {
                if ((MyBroadcastReceiver.keepAliveSocket!!.connectionState == MySocket.ConnectionState.Disconnected ||
                            MyBroadcastReceiver.keepAliveSocket!!.connectionState == MySocket.ConnectionState.Error) &&
                    (MyBroadcastReceiver.dataTransferSocket!!.connectionState == MySocket.ConnectionState.Disconnected ||
                            MyBroadcastReceiver.dataTransferSocket!!.connectionState == MySocket.ConnectionState.Error)) {
                    break
                }
                Thread.sleep(1000)
            }

            Log.i(null, "WIFIDIRECT - Sockets died, so closing foreground task")

            // Stop the service using the startId, so that we don't stop
            // the service in the middle of handling another job
            stopSelf(msg.arg1)
        }
    }

    override fun onCreate() {
        super.onCreate()

        Log.i(null, "WIFIDIRECT - Service is being created")

        HandlerThread("KeepAliveStartArgs", Process.THREAD_PRIORITY_BACKGROUND).apply {
            start()
            serviceLooper = looper
            serviceHandler = ServiceHandler(looper)
        }

        var builder = NotificationCompat.Builder(this, CustomComponentsSampleActivity.CHANNEL_ID)
            .setSmallIcon(R.drawable.layout_bg)
            .setContentTitle("foregroundThingyTitle")
            .setContentText("foregroundThingyContent")
            .setPriority(NotificationCompat.PRIORITY_DEFAULT)

        startForeground(456, builder.build())
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        super.onStartCommand(intent, flags, startId)

        serviceHandler?.obtainMessage()?.also { msg ->
            msg.arg1 = startId
            serviceHandler?.sendMessage(msg)
        }

        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? {
        return null
    }

    override fun onDestroy() {
        Log.i(null, "WIFIDIRECT - Service is done")
    }
}