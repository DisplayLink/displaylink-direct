package com.displaylink.video

import org.junit.Assert.assertEquals
import org.junit.Test

class DisplayModeTest {

    @Test
    fun testDisplayModeValues() {
        assertEquals("Wall", DisplayMode.WALL.label)
        assertEquals("Clone", DisplayMode.CLONE.label)
    }

    @Test
    fun testDefaultMode() {
        assertEquals(DisplayMode.WALL, DisplayMode.defaultMode())
    }

    @Test
    fun testFromPosition() {
        assertEquals(DisplayMode.WALL, DisplayMode.fromPosition(0))
        assertEquals(DisplayMode.CLONE, DisplayMode.fromPosition(1))
        // Out of bounds falls back to default
        assertEquals(DisplayMode.WALL, DisplayMode.fromPosition(99))
    }
}
