package com.displaylink.video

import android.graphics.SurfaceTexture
import java.nio.ByteBuffer

/**
 * Kotlin facade over the `dlvideo` JNI library, which drives DisplayLink Direct.
 *
 * The native side owns the DisplayLink Direct system, the GL compositor and the frame pipeline.
 * Every call that blocks (`openWall`, `showStillFrame`) must be made off the main thread.
 */
object NativeVideo {

    /** Geometry of the composite frame buffer that is split across the wall. */
    data class Layout(
        val displayCount: Int,
        val columns: Int,
        val rows: Int,
        val compositeWidth: Int,
        val compositeHeight: Int,
        val strideBytes: Int,
        /** Composite height plus the spare rows DisplayLink Direct needs for its last region. */
        val bufferHeight: Int,
    )

    /** Frame counters reported by the native pipeline. */
    data class Stats(val rendered: Long, val presented: Long, val dropped: Long)

    @Volatile
    private var libraryError: String? = null

    val available: Boolean
        get() = libraryError == null

    val loadError: String?
        get() = libraryError

    init {
        try {
            System.loadLibrary("dlvideo")
        } catch (error: UnsatisfiedLinkError) {
            libraryError = error.message ?: "libdlvideo.so could not be loaded"
        }
    }

    /** Initialises DisplayLink Direct for externally supplied USB devices. */
    fun initialise(): DlStatus = DlStatus.of(nativeInitialise())

    fun teardown() = nativeTeardown()

    /**
     * Hands an open USB device over to DisplayLink Direct. The file descriptor must stay open and
     * unused by the application until [detachUsbDevice] returns.
     */
    fun attachUsbDevice(fileDescriptor: Int, busNumber: Int, deviceAddress: Int): DlStatus =
        DlStatus.of(nativeAttachUsbDevice(fileDescriptor, busNumber, deviceAddress))

    fun detachUsbDevice(busNumber: Int, deviceAddress: Int): DlStatus =
        DlStatus.of(nativeDetachUsbDevice(busNumber, deviceAddress))

    /**
     * Waits for the attached devices to enumerate, powers on every display and computes the wall
     * layout. Blocking.
     */
    fun openWall(timeoutMs: Int): DlStatus = DlStatus.of(nativeOpenWall(timeoutMs))

    fun closeWall() = nativeCloseWall()

    /** Makes an in-flight [openWall] give up at its next poll. Non-blocking. */
    fun cancelOpenWall() = nativeCancelOpenWall()

    fun layout(): Layout? = nativeWallLayout()?.let {
        Layout(
            displayCount = it[0],
            columns = it[1],
            rows = it[2],
            compositeWidth = it[3],
            compositeHeight = it[4],
            strideBytes = it[5],
            bufferHeight = it[6],
        )
    }

    fun displayIds(): List<String> = nativeDisplayIds()?.toList().orEmpty()

    /** Starts the render and present threads that feed the wall from [surfaceTexture]. */
    fun startPlayback(surfaceTexture: SurfaceTexture, sourceWidth: Int, sourceHeight: Int): Boolean =
        nativeStartPlayback(surfaceTexture, sourceWidth, sourceHeight)

    fun stopPlayback() = nativeStopPlayback()

    fun onFrameAvailable() = nativeOnFrameAvailable()

    fun setSourceSize(width: Int, height: Int) = nativeSetSourceSize(width, height)

    /**
     * Shows a single pre-composed still frame. [pixels] must be a direct buffer holding
     * `ARGB_8888` pixels for the whole composite, including the spare rows.
     */
    fun showStillFrame(pixels: ByteBuffer, strideBytes: Int): Boolean =
        nativeShowStillFrame(pixels, strideBytes)

    fun stats(): Stats = nativeStats()?.let { Stats(it[0], it[1], it[2]) } ?: Stats(0, 0, 0)

    private external fun nativeInitialise(): Int
    private external fun nativeTeardown()
    private external fun nativeAttachUsbDevice(
        fileDescriptor: Int,
        busNumber: Int,
        deviceAddress: Int,
    ): Int

    private external fun nativeDetachUsbDevice(busNumber: Int, deviceAddress: Int): Int
    private external fun nativeOpenWall(timeoutMs: Int): Int
    private external fun nativeCloseWall()
    private external fun nativeCancelOpenWall()
    private external fun nativeWallLayout(): IntArray?
    private external fun nativeDisplayIds(): Array<String>?
    private external fun nativeStartPlayback(
        surfaceTexture: SurfaceTexture,
        sourceWidth: Int,
        sourceHeight: Int,
    ): Boolean

    private external fun nativeStopPlayback()
    private external fun nativeOnFrameAvailable()
    private external fun nativeSetSourceSize(width: Int, height: Int)
    private external fun nativeShowStillFrame(pixels: ByteBuffer, strideBytes: Int): Boolean
    private external fun nativeStats(): LongArray?
}

/** Mirrors `dlsdk_status` from `dlsdk.h`. */
enum class DlStatus(val code: Int, val message: String) {
    SUCCESS(0, "Success"),
    NOT_ENOUGH_SPACE(1, "Not enough space"),
    UNSUCCESSFUL(2, "Unsuccessful"),
    UNSUCCESSFUL_NO_DEVICE(3, "No DisplayLink device"),
    UNSUCCESSFUL_NO_MONITOR(4, "No monitor connected"),
    UNSUCCESSFUL_MONITOR_OFF(5, "Monitor is powered off"),
    INVALID_HANDLE(6, "Invalid handle"),
    INVALID_ARGS(7, "Invalid arguments"),
    TIMEOUT(8, "Timed out"),
    NOT_IMPLEMENTED(9, "Not implemented on this platform"),
    BUSY(10, "Device is busy"),
    UNKNOWN(-1, "Unknown error");

    val isSuccess: Boolean get() = this == SUCCESS

    companion object {
        fun of(code: Int): DlStatus = entries.firstOrNull { it.code == code } ?: UNKNOWN
    }
}
