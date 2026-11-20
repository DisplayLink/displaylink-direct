package com.displaylink.video

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.SurfaceTexture
import android.net.Uri
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.util.Log
import android.view.Surface
import androidx.annotation.OptIn
import androidx.media3.common.PlaybackException
import androidx.media3.common.Player
import androidx.media3.common.VideoSize
import androidx.media3.common.util.UnstableApi
import androidx.media3.exoplayer.ExoPlayer
import java.nio.ByteBuffer
import java.nio.ByteOrder
import androidx.media3.common.MediaItem as Media3MediaItem

/**
 * Drives a video wall session end to end.
 *
 * Videos are decoded by ExoPlayer into a [SurfaceTexture], scaled on the GPU into a single
 * composite frame buffer and split across the DisplayLink displays by the native pipeline. Still
 * images are composed once with a [Canvas] and shown as a single frame.
 *
 * Every player and DisplayLink Direct call runs on a dedicated control thread so the UI thread
 * never blocks.
 */
@OptIn(UnstableApi::class)
class VideoWallSession(
    private val context: Context,
    private val callback: Callback,
) {

    interface Callback {
        fun onWallReady(layout: NativeVideo.Layout, displayIds: List<String>)
        fun onStillShown(layout: NativeVideo.Layout)
        fun onPlaybackEnded()
        fun onFailed(message: String)
    }

    private val mainHandler = Handler(Looper.getMainLooper())

    private val controlThread = HandlerThread("dlvideo-control").apply { start() }
    private val controlHandler = Handler(controlThread.looper)

    // SurfaceTexture callbacks arrive at video frame rate and only signal the native render
    // thread, so they get their own thread rather than competing with the control thread.
    private val frameThread = HandlerThread("dlvideo-frames").apply { start() }
    private val frameHandler = Handler(frameThread.looper)

    private var player: ExoPlayer? = null
    private var surface: Surface? = null
    private var surfaceTexture: SurfaceTexture? = null

    @Volatile
    private var active = false

    @Volatile
    private var released = false

    val isActive: Boolean get() = active

    /** Opens the wall and starts showing [media]. Returns immediately. */
    fun start(media: MediaItem, loop: Boolean) {
        if (active || released) return
        active = true

        controlHandler.post {
            val status = NativeVideo.openWall(WALL_OPEN_TIMEOUT_MS)
            if (!status.isSuccess) {
                fail(
                    when (status) {
                        DlStatus.INVALID_ARGS ->
                            context.getString(R.string.error_mixed_resolutions)

                        else -> context.getString(R.string.error_open_wall, status.message)
                    }
                )
                return@post
            }

            val layout = NativeVideo.layout()
            if (layout == null) {
                fail(context.getString(R.string.error_open_wall, DlStatus.UNKNOWN.message))
                return@post
            }

            val displayIds = NativeVideo.displayIds()
            Log.i(TAG, "Wall open: $layout displays=$displayIds")
            notifyUi { callback.onWallReady(layout, displayIds) }

            if (media.isVideo) {
                startVideo(media, loop, layout)
            } else {
                showStillImage(media.uri, layout)
            }
        }
    }

    /** Stops playback, powers the displays down and releases every resource. */
    fun stop() {
        if (released) return
        active = false
        // Unblocks the control thread if it is still waiting for displays to enumerate.
        NativeVideo.cancelOpenWall()
        controlHandler.post(::releaseResources)
    }

    /**
     * Releases the worker threads and every DisplayLink Direct resource. Blocks until the control
     * thread has finished, so the caller can safely tear the SDK down afterwards. The session
     * cannot be reused.
     */
    fun release() {
        if (released) return
        active = false
        released = true

        NativeVideo.cancelOpenWall()
        controlHandler.post(::releaseResources)
        // quitSafely() lets the pending teardown run before the loopers exit.
        controlThread.quitSafely()
        frameThread.quitSafely()
        runCatching { controlThread.join(RELEASE_TIMEOUT_MS) }
        runCatching { frameThread.join(RELEASE_TIMEOUT_MS) }
    }

    /** Tears the pipeline down. Idempotent, and only ever runs on the control thread. */
    private fun releaseResources() {
        player?.let {
            it.stop()
            it.release()
        }
        player = null

        // The native pipeline must let go of the SurfaceTexture before it is released.
        NativeVideo.stopPlayback()

        surface?.release()
        surface = null
        surfaceTexture?.release()
        surfaceTexture = null

        NativeVideo.closeWall()
    }

    private fun startVideo(media: MediaItem, loop: Boolean, layout: NativeVideo.Layout) {
        val sourceWidth = if (media.width > 0) media.width else layout.compositeWidth
        val sourceHeight = if (media.height > 0) media.height else layout.compositeHeight

        // Created detached from any GL context so the native render thread can attach it to its
        // own context.
        val texture = SurfaceTexture(false).apply {
            setDefaultBufferSize(sourceWidth, sourceHeight)
            setOnFrameAvailableListener({ NativeVideo.onFrameAvailable() }, frameHandler)
        }
        surfaceTexture = texture

        if (!NativeVideo.startPlayback(texture, sourceWidth, sourceHeight)) {
            texture.release()
            surfaceTexture = null
            fail(context.getString(R.string.error_start_pipeline))
            return
        }

        val videoSurface = Surface(texture)
        surface = videoSurface

        val exoPlayer = ExoPlayer.Builder(context)
            .setLooper(controlThread.looper)
            .build()
        player = exoPlayer

        exoPlayer.setVideoSurface(videoSurface)
        exoPlayer.repeatMode = if (loop) Player.REPEAT_MODE_ONE else Player.REPEAT_MODE_OFF
        exoPlayer.volume = 0f
        exoPlayer.addListener(object : Player.Listener {
            override fun onVideoSizeChanged(videoSize: VideoSize) {
                if (videoSize.width > 0 && videoSize.height > 0) {
                    texture.setDefaultBufferSize(videoSize.width, videoSize.height)
                    NativeVideo.setSourceSize(videoSize.width, videoSize.height)
                }
            }

            override fun onPlayerError(error: PlaybackException) {
                fail(context.getString(R.string.error_playback, error.errorCodeName))
            }

            override fun onPlaybackStateChanged(playbackState: Int) {
                if (playbackState == Player.STATE_ENDED) {
                    notifyUi { callback.onPlaybackEnded() }
                }
            }
        })

        exoPlayer.setMediaItem(Media3MediaItem.fromUri(media.uri))
        exoPlayer.prepare()
        exoPlayer.play()
    }

    private fun showStillImage(uri: Uri, layout: NativeVideo.Layout) {
        val composite = composeStillImage(uri, layout)
        if (composite == null) {
            fail(context.getString(R.string.error_decode_image))
            return
        }

        try {
            val pixels = ByteBuffer
                .allocateDirect(composite.byteCount)
                .order(ByteOrder.nativeOrder())
            composite.copyPixelsToBuffer(pixels)
            pixels.rewind()

            if (NativeVideo.showStillFrame(pixels, composite.rowBytes)) {
                notifyUi { callback.onStillShown(layout) }
            } else {
                fail(context.getString(R.string.error_show_frame))
            }
        } catch (error: OutOfMemoryError) {
            fail(context.getString(R.string.error_decode_image))
            Log.e(TAG, "Out of memory composing the still frame", error)
        } finally {
            composite.recycle()
        }
    }

    /**
     * Decodes [uri] and letterboxes it into a composite-sized `ARGB_8888` bitmap, including the
     * spare rows DisplayLink Direct reads past the last display region.
     */
    private fun composeStillImage(uri: Uri, layout: NativeVideo.Layout): Bitmap? {
        val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        context.contentResolver.openInputStream(uri)?.use {
            BitmapFactory.decodeStream(it, null, bounds)
        }
        if (bounds.outWidth <= 0 || bounds.outHeight <= 0) return null

        val options = BitmapFactory.Options().apply {
            inPreferredConfig = Bitmap.Config.ARGB_8888
            inSampleSize = sampleSizeFor(
                bounds.outWidth,
                bounds.outHeight,
                layout.compositeWidth,
                layout.compositeHeight,
            )
        }
        val source = context.contentResolver.openInputStream(uri)?.use {
            BitmapFactory.decodeStream(it, null, options)
        } ?: return null

        return try {
            val composite = Bitmap.createBitmap(
                layout.compositeWidth,
                layout.bufferHeight,
                Bitmap.Config.ARGB_8888,
            )
            val region = letterbox(
                source.width,
                source.height,
                layout.compositeWidth,
                layout.compositeHeight,
            )
            val canvas = Canvas(composite)
            canvas.drawColor(Color.BLACK)
            canvas.drawBitmap(
                source,
                null,
                Rect(
                    region.left,
                    region.top,
                    region.left + region.width,
                    region.top + region.height,
                ),
                Paint(Paint.FILTER_BITMAP_FLAG or Paint.ANTI_ALIAS_FLAG),
            )
            composite
        } catch (error: OutOfMemoryError) {
            Log.e(TAG, "Out of memory allocating the composite bitmap", error)
            null
        } finally {
            source.recycle()
        }
    }

    private fun fail(message: String) {
        active = false
        Log.e(TAG, message)
        // Posted rather than run inline: fail() can be reached from a player callback, and
        // ExoPlayer must not be released while it is dispatching one.
        controlHandler.post(::releaseResources)
        notifyUi { callback.onFailed(message) }
    }

    private fun notifyUi(action: () -> Unit) {
        if (released) return
        mainHandler.post {
            if (!released) action()
        }
    }

    companion object {
        private const val TAG = "VideoWallSession"

        /** Device and display enumeration completes asynchronously after the USB handover. */
        private const val WALL_OPEN_TIMEOUT_MS = 10_000

        /** Upper bound on how long [release] waits for the worker threads to finish. */
        private const val RELEASE_TIMEOUT_MS = 5_000L

        /** A rectangle in composite pixels, free of Android types so it can be unit tested. */
        data class Region(val left: Int, val top: Int, val width: Int, val height: Int)

        /** Largest rectangle with the source aspect ratio that fits the target, centred. */
        fun letterbox(
            sourceWidth: Int,
            sourceHeight: Int,
            targetWidth: Int,
            targetHeight: Int,
        ): Region {
            if (sourceWidth <= 0 || sourceHeight <= 0) {
                return Region(0, 0, targetWidth, targetHeight)
            }
            val scale = minOf(
                targetWidth.toDouble() / sourceWidth,
                targetHeight.toDouble() / sourceHeight,
            )
            val width = Math.round(sourceWidth * scale).toInt().coerceAtLeast(1)
            val height = Math.round(sourceHeight * scale).toInt().coerceAtLeast(1)
            return Region((targetWidth - width) / 2, (targetHeight - height) / 2, width, height)
        }

        /** Largest power-of-two subsampling that still covers the target size. */
        fun sampleSizeFor(
            sourceWidth: Int,
            sourceHeight: Int,
            targetWidth: Int,
            targetHeight: Int,
        ): Int {
            var sampleSize = 1
            while (sourceWidth / (sampleSize * 2) >= targetWidth &&
                sourceHeight / (sampleSize * 2) >= targetHeight
            ) {
                sampleSize *= 2
            }
            return sampleSize
        }
    }
}
