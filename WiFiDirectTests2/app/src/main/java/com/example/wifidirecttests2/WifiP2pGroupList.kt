package android.net.wifi.p2p.WifiP2pManager

import android.net.wifi.p2p.WifiP2pGroup
import android.os.Parcel
import android.os.Parcelable
import android.util.LruCache


/**
 * A class representing a Wi-Fi P2p group list
 *
 * {@see WifiP2pManager}
 * @hide
 */
class WifiP2pGroupList @JvmOverloads internal constructor(
    source: WifiP2pGroupList? = null,
    private val mListener: GroupDeleteListener? = null
) : Parcelable {
    private val mGroups: LruCache<Int?, WifiP2pGroup?>
    private var isClearCalled = false

    interface GroupDeleteListener {
        fun onDeleteGroup(netId: Int)
    }

    val groupList: MutableCollection<WifiP2pGroup?>
        /**
         * Return the list of p2p group.
         *
         * @return the list of p2p group.
         */
        get() = mGroups.snapshot().values

    /**
     * Add the specified group to this group list.
     *
     * @param group
     */
    fun add(group: WifiP2pGroup) {
        mGroups.put(group.networkId, group)
    }

    /**
     * Remove the group with the specified network id from this group list.
     *
     * @param netId
     */
    fun remove(netId: Int) {
        mGroups.remove(netId)
    }

    /**
     * Remove the group with the specified device address from this group list.
     *
     * @param deviceAddress
     */
    fun remove(deviceAddress: String?) {
        remove(getNetworkId(deviceAddress))
    }

    /**
     * Clear the group.
     */
    fun clear(): Boolean {
        if (mGroups.size() == 0) return false
        isClearCalled = true
        mGroups.evictAll()
        isClearCalled = false
        return true
    }

    /**
     * Return the network id of the group owner profile with the specified p2p device
     * address.
     * If more than one persistent group of the same address is present in the list,
     * return the first one.
     *
     * @param deviceAddress p2p device address.
     * @return the network id. if not found, return -1.
     */
    fun getNetworkId(deviceAddress: String?): Int {
        if (deviceAddress == null) return -1
        val groups: MutableCollection<WifiP2pGroup?> = mGroups.snapshot().values
        for (grp in groups) {
            if (deviceAddress.equals(grp?.owner?.deviceAddress, ignoreCase = true)) {
                // update cache ordered.
                mGroups.get(grp?.networkId)
                if (grp?.networkId != null)
                {
                    return grp.networkId
                }
                return -1
            }
        }
        return -1
    }

    /**
     * Return the network id of the group with the specified p2p device address
     * and the ssid.
     *
     * @param deviceAddress p2p device address.
     * @param ssid ssid.
     * @return the network id. if not found, return -1.
     */
    fun getNetworkId(deviceAddress: String?, ssid: String?): Int {
        if (deviceAddress == null || ssid == null) {
            return -1
        }
        val groups: MutableCollection<WifiP2pGroup?> = mGroups.snapshot().values
        for (grp in groups) {
            if (deviceAddress.equals(grp?.owner?.deviceAddress, ignoreCase = true) &&
                ssid == grp?.networkName
            ) {
                // update cache ordered.
                mGroups.get(grp.networkId)
                return grp.networkId
            }
        }
        return -1
    }

    /**
     * Return the group owner address of the group with the specified network id
     *
     * @param netId network id.
     * @return the address. if not found, return null.
     */
    fun getOwnerAddr(netId: Int): String? {
        val grp = mGroups.get(netId)
        if (grp != null) {
            return grp.owner.deviceAddress
        }
        return null
    }

    /**
     * Return true if this group list contains the specified network id.
     * This function does NOT update LRU information.
     * It means the internal queue is NOT reordered.
     *
     * @param netId network id.
     * @return true if the specified network id is present in this group list.
     */
    fun contains(netId: Int): Boolean {
        val groups: MutableCollection<WifiP2pGroup?> = mGroups.snapshot().values
        for (grp in groups) {
            if (netId == grp?.networkId) {
                return true
            }
        }
        return false
    }

    override fun toString(): String {
        val sbuf = StringBuffer()
        val groups = mGroups.snapshot().values
        for (grp in groups) {
            sbuf.append(grp).append("\n")
        }
        return sbuf.toString()
    }

    /** Implement the Parcelable interface  */
    override fun describeContents(): Int {
        return 0
    }

    /** Implement the Parcelable interface  */
    override fun writeToParcel(dest: Parcel, flags: Int) {
        val groups = mGroups.snapshot().values
        dest.writeInt(groups.size)
        for (group in groups) {
            dest.writeParcelable(group, flags)
        }
    }

    init {
        mGroups = object : LruCache<Int?, WifiP2pGroup?>(CREDENTIAL_MAX_NUM) {
            override fun entryRemoved(
                evicted: Boolean, netId: Int?,
                oldValue: WifiP2pGroup?, newValue: WifiP2pGroup?
            ) {
                if (mListener != null && !isClearCalled) {
                    if (oldValue?.networkId != null) {
                        mListener.onDeleteGroup(oldValue.networkId)
                    }
                }
            }
        }
        if (source != null) {
            for (item in source.mGroups.snapshot().entries) {
                mGroups.put(item.key, item.value)
            }
        }
    }

    companion object {
        private const val CREDENTIAL_MAX_NUM = 32

        /** Implement the Parcelable interface  */
        @JvmField
        val CREATOR: Parcelable.Creator<WifiP2pGroupList?> =
            object : Parcelable.Creator<WifiP2pGroupList?> {
                override fun createFromParcel(`in`: Parcel): WifiP2pGroupList {
                    val grpList = WifiP2pGroupList()
                    val deviceCount = `in`.readInt()
                    for (i in 0..<deviceCount) {
                        grpList.add((`in`.readParcelable<Parcelable?>(null) as WifiP2pGroup?)!!)
                    }
                    return grpList
                }

                override fun newArray(size: Int): Array<WifiP2pGroupList?> {
                    return arrayOfNulls<WifiP2pGroupList>(size)
                }
            }
    }
}