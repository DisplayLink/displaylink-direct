package com.displaylink.video

import org.junit.Assert.assertEquals
import org.junit.Test

class TimeoutOptionTest {

    @Test
    fun testTimeoutDurations() {
        assertEquals(60L, TimeoutOption.ONE_MINUTE.durationSeconds)
        assertEquals(60_000L, TimeoutOption.ONE_MINUTE.durationMillis)

        assertEquals(600L, TimeoutOption.TEN_MINUTES.durationSeconds)
        assertEquals(600_000L, TimeoutOption.TEN_MINUTES.durationMillis)

        assertEquals(3600L, TimeoutOption.SIXTY_MINUTES.durationSeconds)
        assertEquals(3_600_000L, TimeoutOption.SIXTY_MINUTES.durationMillis)

        assertEquals(36_000L, TimeoutOption.TEN_HOURS.durationSeconds)
        assertEquals(36_000_000L, TimeoutOption.TEN_HOURS.durationMillis)
    }

    @Test
    fun testDefaultOption() {
        assertEquals(TimeoutOption.ONE_MINUTE, TimeoutOption.defaultOption())
    }

    @Test
    fun testFormatSeconds() {
        assertEquals("00:01:00", TimeoutOption.formatSeconds(60L))
        assertEquals("00:10:00", TimeoutOption.formatSeconds(600L))
        assertEquals("01:00:00", TimeoutOption.formatSeconds(3600L))
        assertEquals("10:00:00", TimeoutOption.formatSeconds(36000L))
        assertEquals("00:00:00", TimeoutOption.formatSeconds(0L))
        assertEquals("00:00:00", TimeoutOption.formatSeconds(-5L))
    }
}
