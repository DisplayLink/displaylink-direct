package com.displaylink.video

import android.content.Intent
import android.graphics.BitmapFactory
import android.media.MediaMetadataRetriever
import android.net.Uri
import android.os.Bundle
import android.os.CountDownTimer
import android.provider.OpenableColumns
import android.view.View
import android.view.WindowManager
import android.widget.ArrayAdapter
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.displaylink.video.databinding.ActivityMainBinding
import com.google.android.material.snackbar.Snackbar
import java.util.concurrent.Executors

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    private var selectedMedia: MediaItem? = null
    private var selectedMode: DisplayMode = DisplayMode.defaultMode()
    private var selectedTimeout: TimeoutOption = TimeoutOption.defaultOption()
    private var countDownTimer: CountDownTimer? = null
    private var isSessionActive: Boolean = false

    private val setupExecutor = Executors.newSingleThreadExecutor()
    private var sdkReady = false

    /** Set while waiting for the user to answer the USB permission dialog. */
    private var startPendingUsbPermission = false

    private lateinit var usbController: DisplayLinkUsbController
    private lateinit var wallSession: VideoWallSession

    // File picker supporting both images and videos
    private val openDocumentLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        uri?.let { onMediaSelected(it) }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Ensure HOME points to app-writable storage so libdlsdk can create its internal hive file (.dl.xml).
        runCatching {
            android.system.Os.setenv("HOME", filesDir.absolutePath, true)
        }

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        usbController = DisplayLinkUsbController(this, usbCallback)
        wallSession = VideoWallSession(this, wallCallback)

        setupWindowInsets()
        setupDisplayModeToggle()
        setupTimeoutDropdown()
        setupListeners()
        updateSessionState(SessionState.IDLE)
        initialiseSdk()
    }

    private fun initialiseSdk() {
        if (!NativeVideo.available) {
            sdkReady = false
            binding.btnStartSession.isEnabled = false
            binding.tvStatusMessage.text =
                getString(R.string.error_native_library, NativeVideo.loadError.orEmpty())
            return
        }

        setupExecutor.execute {
            val status = NativeVideo.initialise()
            runOnUiThread {
                sdkReady = status.isSuccess
                if (sdkReady) {
                    usbController.onSdkReady()
                    binding.btnStartSession.isEnabled = !isSessionActive
                } else {
                    binding.btnStartSession.isEnabled = false
                    binding.tvStatusMessage.text =
                        getString(R.string.error_init_sdk, status.message)
                }
            }
        }
    }

    private fun setupWindowInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { view, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }
    }

    private fun setupDisplayModeToggle() {
        // Set default checked mode (Wall)
        binding.toggleDisplayMode.check(R.id.btnModeWall)
        binding.tvModeDescription.text = selectedMode.description

        binding.toggleDisplayMode.addOnButtonCheckedListener { _, checkedId, isChecked ->
            if (isChecked) {
                selectedMode = when (checkedId) {
                    R.id.btnModeWall -> DisplayMode.WALL
                    R.id.btnModeClone -> DisplayMode.CLONE
                    else -> DisplayMode.defaultMode()
                }
                binding.tvModeDescription.text = selectedMode.description
            }
        }
    }

    private fun setupTimeoutDropdown() {
        val labels = TimeoutOption.entries.map { it.label }
        val adapter = ArrayAdapter(this, android.R.layout.simple_dropdown_item_1line, labels)
        binding.actvTimeout.setAdapter(adapter)

        // Set default value: 1 minute
        binding.actvTimeout.setText(selectedTimeout.label, false)
        binding.tvRemainingTimer.text = TimeoutOption.formatSeconds(selectedTimeout.durationSeconds)

        binding.actvTimeout.setOnItemClickListener { _, _, position, _ ->
            selectedTimeout = TimeoutOption.fromPosition(position)
            if (!isSessionActive) {
                binding.tvRemainingTimer.text =
                    TimeoutOption.formatSeconds(selectedTimeout.durationSeconds)
                binding.progressTimeout.progress = 100
            }
        }
    }

    private fun setupListeners() {
        binding.btnSelectMedia.setOnClickListener {
            // Pick image or video files
            openDocumentLauncher.launch(arrayOf("image/*", "video/*"))
        }

        binding.btnStartSession.setOnClickListener {
            startVideoSession()
        }

        binding.btnStopSession.setOnClickListener {
            stopVideoSession(byUser = true)
        }
    }

    private fun onMediaSelected(uri: Uri) {
        try {
            contentResolver.takePersistableUriPermission(
                uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
        } catch (_: SecurityException) {
            // Some content providers may not support persistable permissions
        }

        val item = extractMediaMetadata(uri)
        selectedMedia = item

        displayMediaInfo(item)
        Snackbar.make(binding.root, "Loaded: ${item.name}", Snackbar.LENGTH_SHORT).show()
    }

    private fun extractMediaMetadata(uri: Uri): MediaItem {
        var name = "Unknown media"
        var size: Long = 0
        val mimeType = contentResolver.getType(uri) ?: "application/octet-stream"
        val isVideo = mimeType.startsWith("video/")

        contentResolver.query(uri, null, null, null, null)?.use { cursor ->
            val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            val sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE)
            if (cursor.moveToFirst()) {
                if (nameIndex != -1 && !cursor.isNull(nameIndex)) {
                    name = cursor.getString(nameIndex)
                }
                if (sizeIndex != -1 && !cursor.isNull(sizeIndex)) {
                    size = cursor.getLong(sizeIndex)
                }
            }
        }

        var width = 0
        var height = 0
        var durationMs: Long = 0

        if (isVideo) {
            try {
                MediaMetadataRetriever().use { retriever ->
                    retriever.setDataSource(this, uri)
                    val wStr = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH)
                    val hStr = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT)
                    val dStr = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)
                    width = wStr?.toIntOrNull() ?: 0
                    height = hStr?.toIntOrNull() ?: 0
                    durationMs = dStr?.toLongOrNull() ?: 0
                }
            } catch (_: Exception) {
                // Metadata retrieval failed, continue with defaults
            }
        } else {
            try {
                contentResolver.openInputStream(uri)?.use { stream ->
                    val options = BitmapFactory.Options().apply { inJustDecodeBounds = true }
                    BitmapFactory.decodeStream(stream, null, options)
                    width = options.outWidth
                    height = options.outHeight
                }
            } catch (_: Exception) {
                // Bitmap decode failed, continue with defaults
            }
        }

        return MediaItem(
            uri = uri,
            name = name,
            mimeType = mimeType,
            sizeBytes = size,
            isVideo = isVideo,
            width = width,
            height = height,
            durationMs = durationMs
        )
    }

    private fun displayMediaInfo(item: MediaItem) {
        binding.tvEmptyMediaNotice.visibility = View.GONE
        binding.layoutMediaInfo.visibility = View.VISIBLE

        binding.tvFileName.text = item.name

        val typeText = if (item.isVideo) "Video (${item.mimeType})" else "Image (${item.mimeType})"
        binding.tvFileTypeSize.text = getString(R.string.file_type_format, typeText, item.formattedSize)

        val resolutionText = if (item.durationMs > 0) {
            "${item.formattedResolution} • Duration: ${item.formattedDuration}"
        } else {
            item.formattedResolution
        }
        binding.tvFileDimensions.text = getString(R.string.file_resolution_format, resolutionText)
    }

    private fun startVideoSession() {
        val media = selectedMedia
        if (media == null) {
            showMessage(getString(R.string.error_no_media))
            return
        }
        if (!selectedMode.isImplemented) {
            showMessage(getString(R.string.error_mode_not_supported, selectedMode.label))
            return
        }
        if (!sdkReady) {
            showMessage(getString(R.string.error_sdk_not_ready))
            return
        }

        // DisplayLink Direct only sees the adapters this application hands to it, so make sure the
        // user has granted USB permission before opening the wall.
        if (usbController.attachedCount == 0) {
            startPendingUsbPermission = true
            if (usbController.attachAvailableDevices() == 0) {
                showMessage(getString(R.string.status_waiting_for_usb))
                return
            }
            startPendingUsbPermission = false
        }

        isSessionActive = true
        updateSessionState(SessionState.RUNNING)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        wallSession.start(media, binding.switchLoop.isChecked)
        startCountdown()
    }

    private fun startCountdown() {
        val totalDurationMillis = selectedTimeout.durationMillis
        countDownTimer?.cancel()

        countDownTimer = object : CountDownTimer(totalDurationMillis, 500L) {
            override fun onTick(millisUntilFinished: Long) {
                val secondsRemaining = (millisUntilFinished + 999) / 1000
                binding.tvRemainingTimer.text = TimeoutOption.formatSeconds(secondsRemaining)

                val progress = ((millisUntilFinished.toDouble() / totalDurationMillis.toDouble()) * 100).toInt()
                binding.progressTimeout.progress = progress.coerceIn(0, 100)
            }

            override fun onFinish() {
                binding.tvRemainingTimer.text = TimeoutOption.formatSeconds(0L)
                binding.progressTimeout.progress = 0
                stopVideoSession(byUser = false)
            }
        }.start()
    }

    private fun stopVideoSession(byUser: Boolean) {
        countDownTimer?.cancel()
        countDownTimer = null
        startPendingUsbPermission = false

        if (isSessionActive) {
            wallSession.stop()
        }
        isSessionActive = false
        window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        if (byUser) {
            updateSessionState(SessionState.IDLE)
            binding.tvRemainingTimer.text =
                TimeoutOption.formatSeconds(selectedTimeout.durationSeconds)
            binding.progressTimeout.progress = 100
        } else {
            updateSessionState(SessionState.EXPIRED)
        }
    }

    private fun showMessage(message: String) {
        Snackbar.make(binding.root, message, Snackbar.LENGTH_LONG).show()
    }

    private val usbCallback = object : DisplayLinkUsbController.Callback {
        override fun onDevicesAttached(count: Int) {
            binding.tvStatusMessage.text = getString(R.string.status_usb_attached, count)
            if (startPendingUsbPermission) {
                startPendingUsbPermission = false
                startVideoSession()
            }
        }

        override fun onDeviceDetached(deviceName: String) {
            showMessage(getString(R.string.error_device_removed))
            if (isSessionActive) {
                stopVideoSession(byUser = true)
            }
        }

        override fun onPermissionDenied(deviceName: String) {
            startPendingUsbPermission = false
            showMessage(getString(R.string.error_usb_permission_denied))
            if (isSessionActive) {
                stopVideoSession(byUser = true)
            }
        }

        override fun onError(message: String) {
            startPendingUsbPermission = false
            showMessage(message)
        }
    }

    private val wallCallback = object : VideoWallSession.Callback {
        override fun onWallReady(layout: NativeVideo.Layout, displayIds: List<String>) {
            binding.tvStatusMessage.text = getString(
                R.string.status_wall_ready,
                layout.displayCount,
                layout.columns,
                layout.rows,
                layout.compositeWidth,
                layout.compositeHeight,
            )
        }

        override fun onStillShown(layout: NativeVideo.Layout) {
            binding.tvStatusMessage.text = getString(R.string.status_still_shown)
        }

        override fun onPlaybackEnded() {
            if (isSessionActive && !binding.switchLoop.isChecked) {
                stopVideoSession(byUser = false)
            }
        }

        override fun onFailed(message: String) {
            showMessage(message)
            if (isSessionActive) {
                stopVideoSession(byUser = true)
            }
        }
    }

    private enum class SessionState {
        IDLE, RUNNING, EXPIRED
    }

    private fun updateSessionState(state: SessionState) {
        val idle = state != SessionState.RUNNING
        when (state) {
            SessionState.IDLE -> {
                binding.tvSessionBadge.text = "IDLE"
                binding.tvSessionBadge.setTextColor(ContextCompat.getColor(this, R.color.status_idle))
                binding.tvStatusMessage.text = getString(R.string.status_idle)
            }
            SessionState.RUNNING -> {
                binding.tvSessionBadge.text = "ACTIVE"
                binding.tvSessionBadge.setTextColor(ContextCompat.getColor(this, R.color.status_active))
                binding.tvStatusMessage.text = getString(R.string.status_active, selectedMode.label)
            }
            SessionState.EXPIRED -> {
                binding.tvSessionBadge.text = "EXPIRED"
                binding.tvSessionBadge.setTextColor(ContextCompat.getColor(this, R.color.status_timeout))
                binding.tvStatusMessage.text = getString(R.string.status_timeout_expired)
            }
        }

        binding.btnStartSession.isEnabled = idle && sdkReady
        binding.btnStopSession.isEnabled = !idle
        binding.tilTimeout.isEnabled = idle
        binding.btnSelectMedia.isEnabled = idle
        binding.toggleDisplayMode.isEnabled = idle
        binding.btnModeWall.isEnabled = idle
        // Clone is not implemented yet, so it stays unavailable.
        binding.btnModeClone.isEnabled = false
        binding.switchLoop.isEnabled = idle
    }

    override fun onStart() {
        super.onStart()
        usbController.register()
    }

    override fun onStop() {
        usbController.unregister()
        super.onStop()
    }

    override fun onDestroy() {
        countDownTimer?.cancel()
        countDownTimer = null
        wallSession.release()
        usbController.detachAll()
        setupExecutor.shutdown()
        if (NativeVideo.available) {
            NativeVideo.teardown()
        }
        super.onDestroy()
    }
}
