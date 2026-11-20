package com.displaylink.video

import java.util.Locale
import java.util.concurrent.TimeUnit

/**
 * Timeout options for the 2x2 Video-Wall session.
 */
enum class TimeoutOption(
    val label: String,
    val durationSeconds: Long
) {
    ONE_MINUTE("1 minute (Default)", 60L),
    TEN_MINUTES("10 minutes", 10L * 60L),
    SIXTY_MINUTES("60 minutes", 60L * 60L),
    TEN_HOURS("10 hours", 10L * 3600L);

    val durationMillis: Long
        get() = durationSeconds * 1000L

    companion object {
        fun defaultOption(): TimeoutOption = ONE_MINUTE

        fun fromPosition(position: Int): TimeoutOption =
            entries.getOrNull(position) ?: defaultOption()

        fun formatSeconds(seconds: Long): String {
            val safeSeconds = seconds.coerceAtLeast(0L)
            val hours = TimeUnit.SECONDS.toHours(safeSeconds)
            val minutes = TimeUnit.SECONDS.toMinutes(safeSeconds) % 60
            val secs = safeSeconds % 60
            return String.format(Locale.US, "%02d:%02d:%02d", hours, minutes, secs)
        }
    }
}
