package com.displaylink.video

import org.junit.Assert.assertEquals
import org.junit.Test

class VideoWallSessionTest {

    @Test
    fun letterboxFillsTargetWhenAspectRatiosMatch() {
        val rect = VideoWallSession.letterbox(1920, 1080, 3840, 2160)
        assertEquals(0, rect.left)
        assertEquals(0, rect.top)
        assertEquals(3840, rect.width)
        assertEquals(2160, rect.height)
    }

    @Test
    fun letterboxAddsHorizontalBarsForNarrowSource() {
        // A 1:1 source inside a 16:9 wall is limited by the wall height.
        val rect = VideoWallSession.letterbox(1000, 1000, 3840, 2160)
        assertEquals(2160, rect.height)
        assertEquals(2160, rect.width)
        assertEquals((3840 - 2160) / 2, rect.left)
        assertEquals(0, rect.top)
    }

    @Test
    fun letterboxAddsVerticalBarsForWideSource() {
        // A 32:9 source inside a 16:9 wall is limited by the wall width.
        val rect = VideoWallSession.letterbox(3840, 1080, 3840, 2160)
        assertEquals(3840, rect.width)
        assertEquals(1080, rect.height)
        assertEquals(0, rect.left)
        assertEquals((2160 - 1080) / 2, rect.top)
    }

    @Test
    fun letterboxFallsBackToTargetForEmptySource() {
        val rect = VideoWallSession.letterbox(0, 0, 1920, 1080)
        assertEquals(1920, rect.width)
        assertEquals(1080, rect.height)
    }

    @Test
    fun sampleSizeKeepsSmallImagesIntact() {
        assertEquals(1, VideoWallSession.sampleSizeFor(1920, 1080, 3840, 2160))
        assertEquals(1, VideoWallSession.sampleSizeFor(3840, 2160, 3840, 2160))
    }

    @Test
    fun sampleSizeSubsamplesOversizedImages() {
        assertEquals(2, VideoWallSession.sampleSizeFor(7680, 4320, 3840, 2160))
        assertEquals(4, VideoWallSession.sampleSizeFor(15360, 8640, 3840, 2160))
    }

    @Test
    fun sampleSizeNeverDropsBelowTargetSize() {
        // Subsampling by two would land under the target height, so the full image is decoded.
        assertEquals(1, VideoWallSession.sampleSizeFor(7680, 3000, 3840, 2160))
    }
}
