package com.displaylink.video

import android.net.Uri
import java.util.Locale

/**
 * Encapsulates metadata for a selected image or video file.
 */
data class MediaItem(
    val uri: Uri,
    val name: String,
    val mimeType: String,
    val sizeBytes: Long,
    val isVideo: Boolean,
    val width: Int = 0,
    val height: Int = 0,
    val durationMs: Long = 0
) {
    val formattedSize: String
        get() {
            if (sizeBytes <= 0) return "Unknown size"
            val kb = sizeBytes / 1024.0
            val mb = kb / 1024.0
            val gb = mb / 1024.0
            return when {
                gb >= 1.0 -> String.format(Locale.US, "%.2f GB", gb)
                mb >= 1.0 -> String.format(Locale.US, "%.1f MB", mb)
                else -> String.format(Locale.US, "%.1f KB", kb)
            }
        }

    val formattedResolution: String
        get() = if (width > 0 && height > 0) "${width}x${height}" else "Unknown"

    val formattedDuration: String
        get() {
            if (durationMs <= 0) return ""
            val totalSeconds = durationMs / 1000
            val minutes = totalSeconds / 60
            val seconds = totalSeconds % 60
            return String.format(Locale.US, "%02d:%02d", minutes, seconds)
        }
}
