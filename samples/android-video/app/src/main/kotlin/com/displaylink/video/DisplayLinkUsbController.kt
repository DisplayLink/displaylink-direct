package com.displaylink.video

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbManager
import android.util.Log

/**
 * Finds DisplayLink adapters through [UsbManager], asks the user for permission and hands the open
 * file descriptors to DisplayLink Direct.
 *
 * This is what makes the sample work on a non-rooted device: the application never scans the USB
 * bus itself, it passes devices opened by the Android USB host stack to
 * `dlsdk_usb_attach_device()`.
 */
class DisplayLinkUsbController(
    private val context: Context,
    private val callback: Callback,
) {

    interface Callback {
        /** At least one adapter is now owned by DisplayLink Direct. */
        fun onDevicesAttached(count: Int)

        /** An adapter was unplugged or handed back. */
        fun onDeviceDetached(deviceName: String)

        fun onPermissionDenied(deviceName: String)

        fun onError(message: String)
    }

    private data class UsbAddress(val busNumber: Int, val deviceAddress: Int)

    private val usbManager = context.getSystemService(UsbManager::class.java)
    private val connections = linkedMapOf<String, UsbDeviceConnection>()
    private val addresses = linkedMapOf<String, UsbAddress>()
    private val permissionRequested = mutableSetOf<String>()
    private var registered = false
    @Volatile
    private var isSdkReady = false

    val attachedCount: Int get() = connections.size

    private val permissionAction = "${context.packageName}.USB_PERMISSION"

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(receivedContext: Context, intent: Intent) {
            val device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
            when (intent.action) {
                permissionAction -> {
                    val granted = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)
                    if (device == null) return
                    permissionRequested.remove(device.deviceName)
                    if (granted) {
                        if (!isSdkReady) {
                            Log.d(TAG, "Permission granted for ${device.deviceName}, deferring attach until SDK is ready")
                            return
                        }
                        if (attach(device)) {
                            callback.onDevicesAttached(connections.size)
                        }
                    } else {
                        callback.onPermissionDenied(device.deviceName)
                    }
                }

                UsbManager.ACTION_USB_DEVICE_ATTACHED -> {
                    if (device != null && isDisplayLink(device)) {
                        requestOrAttach(device)
                    }
                }

                UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                    if (device != null) {
                        permissionRequested.remove(device.deviceName)
                        if (connections.containsKey(device.deviceName)) {
                            detach(device.deviceName)
                            callback.onDeviceDetached(device.deviceName)
                        }
                    }
                }
            }
        }
    }

    fun register() {
        if (registered) return
        // The permission result is broadcast through our own PendingIntent, so it must not be
        // exported. Plug and unplug notifications come from the system instead.
        context.registerReceiver(
            receiver,
            IntentFilter(permissionAction),
            Context.RECEIVER_NOT_EXPORTED,
        )
        context.registerReceiver(
            receiver,
            IntentFilter().apply {
                addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
                addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
            },
            Context.RECEIVER_EXPORTED,
        )
        registered = true
    }

    fun unregister() {
        if (!registered) return
        runCatching { context.unregisterReceiver(receiver) }
        registered = false
    }

    fun findDevices(): List<UsbDevice> =
        usbManager.deviceList.values.filter(::isDisplayLink)

    /**
     * Attaches every DisplayLink adapter the application already has permission for, and requests
     * permission for the rest.
     *
     * @return the number of adapters currently owned by DisplayLink Direct.
     */
    fun attachAvailableDevices(): Int {
        val devices = findDevices()
        if (devices.isEmpty()) {
            callback.onError(context.getString(R.string.error_no_usb_device))
            return 0
        }
        devices.forEach(::requestOrAttach)
        return connections.size
    }

    /** Hands every device back to the application and closes its connection. */
    fun detachAll() {
        connections.keys.toList().forEach(::detach)
        isSdkReady = false
    }

    /**
     * Notifies the controller that DisplayLink Direct has initialized.
     * Attaches any permitted DisplayLink adapters and requests permission for the rest.
     */
    fun onSdkReady() {
        isSdkReady = true
        val devices = findDevices()
        var attachedCount = 0
        for (device in devices) {
            if (connections.containsKey(device.deviceName)) continue
            if (usbManager.hasPermission(device)) {
                if (attach(device)) {
                    attachedCount++
                }
            } else {
                requestOrAttach(device)
            }
        }
        if (attachedCount > 0) {
            callback.onDevicesAttached(connections.size)
        }
    }

    private fun requestOrAttach(device: UsbDevice) {
        if (connections.containsKey(device.deviceName)) return

        if (usbManager.hasPermission(device)) {
            if (!isSdkReady) {
                Log.d(TAG, "Device ${device.deviceName} has permission, deferring attach until SDK is ready")
                return
            }
            if (attach(device)) {
                callback.onDevicesAttached(connections.size)
            }
            return
        }
        if (!permissionRequested.add(device.deviceName)) return

        val intent = Intent(permissionAction).setPackage(context.packageName)
        val pendingIntent = PendingIntent.getBroadcast(
            context,
            0,
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        usbManager.requestPermission(device, pendingIntent)
    }

    private fun attach(device: UsbDevice): Boolean {
        val address = parseAddress(device.deviceName)
        if (address == null) {
            callback.onError("Unexpected USB device name: ${device.deviceName}")
            return false
        }

        val connection = usbManager.openDevice(device)
        if (connection == null) {
            callback.onError(context.getString(R.string.error_usb_open_failed))
            return false
        }

        val status = NativeVideo.attachUsbDevice(
            connection.fileDescriptor,
            address.busNumber,
            address.deviceAddress,
        )
        if (!status.isSuccess) {
            connection.close()
            callback.onError(
                context.getString(R.string.error_usb_attach_failed, status.message)
            )
            return false
        }

        connections[device.deviceName] = connection
        addresses[device.deviceName] = address
        Log.i(TAG, "Attached ${device.deviceName} (bus ${address.busNumber}, address ${address.deviceAddress})")
        return true
    }

    private fun detach(deviceName: String) {
        val connection = connections.remove(deviceName)
        val address = addresses.remove(deviceName)
        permissionRequested.remove(deviceName)
        if (address != null) {
            NativeVideo.detachUsbDevice(address.busNumber, address.deviceAddress)
        }
        // The descriptor must stay open until DisplayLink Direct has handed the device back.
        connection?.close()
    }

    private fun isDisplayLink(device: UsbDevice): Boolean = device.vendorId == DISPLAYLINK_VENDOR_ID

    /** `UsbDevice.getDeviceName()` has the form `/dev/bus/usb/<bus>/<address>`. */
    private fun parseAddress(deviceName: String): UsbAddress? {
        val parts = deviceName.split('/').filter { it.isNotEmpty() }
        if (parts.size < 2) return null
        val busNumber = parts[parts.size - 2].toIntOrNull() ?: return null
        val deviceAddress = parts[parts.size - 1].toIntOrNull() ?: return null
        return UsbAddress(busNumber, deviceAddress)
    }

    private companion object {
        const val TAG = "DisplayLinkUsb"
        const val DISPLAYLINK_VENDOR_ID = 0x17E9
    }
}
