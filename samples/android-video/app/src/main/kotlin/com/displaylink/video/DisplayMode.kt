package com.displaylink.video

/**
 * Display modes supported by the DisplayLink Android sample.
 */
enum class DisplayMode(
    val label: String,
    val description: String,
    /** Whether the mode is wired up to DisplayLink Direct yet. */
    val isImplemented: Boolean
) {
    WALL(
        "Wall",
        "Splits media across connected monitors in a video wall layout",
        true
    ),
    CLONE(
        "Clone",
        "Clones media identically across all connected displays (not implemented yet)",
        false
    );

    companion object {
        fun defaultMode(): DisplayMode = WALL

        fun fromPosition(position: Int): DisplayMode =
            entries.getOrNull(position) ?: defaultMode()
    }
}
