package com.example.wifidirecttests2

import android.net.wifi.p2p.WifiP2pManager.WifiP2pGroupList

class MyPersistentGroupInfoListener(val activity: MainActivity)  {
    fun onPersistentGroupInfoAvailable(groups: WifiP2pGroupList) {
        activity.showInfo("onPersistentGroupInfoAvailable", "Listener was called")
    }
}