import argparse
import ctypes
import datetime
import os
import random
import pathlib
import time
import sdl2
import sdl2.ext
import sdl2.sdlimage
import sdl2.sdlttf
import numpy
import dlsdk

class Clock:
    def __init__(self) -> None:
        self._last_tick: float | None = None
        self._fps = 0.0
        self._delta_time = 0.0

    def tick(self) -> None:
        now = time.perf_counter()
        if self._last_tick is not None:
            dt = now - self._last_tick
            if dt > 0:
                self._delta_time = dt
                self._fps = 1.0 / dt
        self._last_tick = now

    def get_fps(self) -> float:
        return self._fps

    def get_delta_time(self) -> float:
        return self._delta_time


class Surface:
    def __init__(self, width=None, height=None, masks=None):
        if width is None or height is None or masks is None:
            raise ValueError("Either ptr or width/height/masks must be provided")

        surface_ptr = sdl2.SDL_CreateRGBSurface(0, width, height, 32, *masks)
        if not surface_ptr:
            self.ptr = None
            raise RuntimeError(f"Failed to create SDL surface: {sdl2.SDL_GetError().decode()}")
        self.ptr = surface_ptr

    @property
    def width(self) -> int:
        return int(self.ptr.contents.w)

    @property
    def height(self) -> int:
        return int(self.ptr.contents.h)

    def __del__(self) -> None:
        if self.ptr:
            sdl2.SDL_FreeSurface(self.ptr)
            self.ptr = None


class DisplayLinkDisplay:
    def __init__(self, prefer_1080p: bool = False, embedded_mode: bool = False) -> None:
        config = dlsdk.Config()
        config.embedded_mode = embedded_mode
        dlsdk.create_system(config)
        print(f"[DisplayLink Direct] initialized ({'embedded' if embedded_mode else 'default'} mode)")

        devices = self.wait_for_devices()
        print(f"[DisplayLink Direct] detected {len(devices)} device(s)")
        assert len(devices) == 1

        displays = devices[0].get_displays()
        print(f"[DisplayLink Direct] detected {len(displays)} display(s)")
        assert len(displays) == 1
        self.display = displays[0]

        self.power_on_display(prefer_1080p)

        print("[DisplayLink Direct] display powered on")
        size = self.display.size()
        print(f"[DisplayLink Direct] display mode: {size.width}x{size.height}")

        self.width = size.width
        self.height = size.height
        assert self.width > 0 and self.height > 0
        assert self.mode.refresh_rate_hz > 0
        self._frame_interval = 1.0 / self.mode.refresh_rate_hz
        self._next_show_time = time.perf_counter()

        XRGB8888_MASKS = (0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000)
        self.surfaces = [
            Surface(width=self.width, height=self.height, masks=XRGB8888_MASKS),
            Surface(width=self.width, height=self.height, masks=XRGB8888_MASKS),
        ]
        self.surfaceViews = [self._surface_view(surface) for surface in self.surfaces]
        self.surfaceIndex = 0

    def power_on_display(self, prefer_1080p):
        self.mode = self._find_mode(1920, 1080) if prefer_1080p else None
        if self.mode is not None and prefer_1080p:
            print(f"[DisplayLink Direct] powering on display at {self.mode_string()}")
            assert self.display.power_on(mode=self.mode) == dlsdk.DLSDK_SUCCESS
        else:
            assert self.display.power_on() == dlsdk.DLSDK_SUCCESS
            self.mode = self.display.preferred_mode()[1]
            print(f"[DisplayLink Direct] powering on display at default mode {self.mode_string()}")

    def wait_for_devices(self):
        for i in range(30, 0, -1):
            devices = dlsdk.get_devices()
            if len(devices) > 0:
                break
            print(f"[DisplayLink Direct] no devices detected, retrying... ({i} attempts left)")
            time.sleep(1)
        return devices

    def _find_mode(self, width: int, height: int) -> dlsdk.DisplayMode | None:
        status, modes = self.display.modes()
        assert status == dlsdk.DLSDK_SUCCESS, f"[DisplayLink Direct] failed to query modes: {status}"
        matches = [m for m in modes if m.resolution.width == width and m.resolution.height == height]
        if not matches:
            return None
        return max(matches, key=lambda m: m.refresh_rate_hz)

    def _surface_view(self, surface: Surface):
        pitch = int(surface.ptr.contents.pitch)
        data_len = pitch * self.height
        buf_type = ctypes.c_uint8 * data_len
        buf = buf_type.from_address(surface.ptr.contents.pixels)
        return numpy.ndarray(
            (self.height, self.width, 4),
            dtype=numpy.uint8,
            buffer=buf,
            strides=(pitch, 4, 1),
        )

    def surface(self) -> Surface:
        return self.surfaces[self.surfaceIndex]

    def show(self, dirty_rects: list[dlsdk.DirtyRect]) -> None:
        # Pace the updates to match the desired frame interval
        delay = self._next_show_time - time.perf_counter()
        if delay > 0:
            time.sleep(delay)

        status = self.display.show(
            dlsdk.DLSDK_PIXEL_FORMAT_XRGB,
            self.surfaceViews[self.surfaceIndex],
            dirty_rects,
        )
        if status != dlsdk.DLSDK_SUCCESS:
            raise RuntimeError(f"[DisplayLink Direct] show failed: {status}")
        self._next_show_time += self._frame_interval
        if self._next_show_time < time.perf_counter():
            self._next_show_time = time.perf_counter() + self._frame_interval
        self.surfaceIndex ^= 1

    def close(self) -> None:
        dlsdk.delete_system()

    def mode_string(self) -> str:
        return f"{self.mode.resolution.width}x{self.mode.resolution.height}@{self.mode.refresh_rate_hz}Hz"

class LogoGame:
    def __init__(self) -> None:
        self.background_color = (0xFF, 0xFF, 0xFF)
        self.fps_text_color = (0x00, 0x81, 0xC6)
        self.default_logo_width = 220
        self.min_speed = 2
        self.max_speed = 20

        self.args: argparse.Namespace | None = None
        self.display: DisplayLinkDisplay | None = None
        self.clock: Clock | None = None
        self.font: ctypes.c_void_p | None = None
        self.fps_surface_ptr = None
        self.fps_surface_text = ""
        self.logo: Surface | None = None
        self.logo_rect: sdl2.SDL_Rect | None = None
        self.logo_velocity: list[int] | None = None

    def parse_args(self) -> argparse.Namespace:
        parser = argparse.ArgumentParser(description="Bounce the DisplayLink logo on a DisplayLink Direct display.")
        parser.add_argument(
            "--1080p",
            dest="prefer_1080p",
            action="store_true",
            help="Set the display to 1080p (1920x1080) mode if it supports such a mode.",
        )
        parser.add_argument(
            "--embedded",
            dest="embedded_mode",
            action="store_true",
            help="Run the DisplayLink Direct system in embedded mode.",
        )
        return parser.parse_args()

    def _open_font(self, size: int) -> ctypes.c_void_p:
        font_candidates = [
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        ]
        for font_path in font_candidates:
            if os.path.exists(font_path):
                font = sdl2.sdlttf.TTF_OpenFont(font_path.encode("utf-8"), size)
                if font:
                    return font
        raise RuntimeError("Unable to open a TTF font from known system font locations")

    def load_logo_surface(self) -> Surface:
        logo_path = pathlib.Path(__file__).resolve().parent / "dl_logo.png"
        assert logo_path.exists(), f"Logo file not found: {logo_path}"
        logo_ptr = sdl2.sdlimage.IMG_Load(str(logo_path).encode("utf-8"))
        if not logo_ptr:
            raise RuntimeError(f"Failed to load logo image: {sdl2.SDL_GetError().decode()}")

        src_w = int(logo_ptr.contents.w)
        src_h = int(logo_ptr.contents.h)
        scale = self.default_logo_width / src_w
        dst_w = self.default_logo_width
        dst_h = int(src_h * scale)

        dst_surface = Surface(width=dst_w, height=dst_h, masks=(0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000))

        src_rect = sdl2.SDL_Rect(0, 0, src_w, src_h)
        dst_rect = sdl2.SDL_Rect(0, 0, dst_w, dst_h)
        if sdl2.SDL_BlitScaled(logo_ptr, ctypes.byref(src_rect), dst_surface.ptr, ctypes.byref(dst_rect)) != 0:
            sdl2.SDL_FreeSurface(logo_ptr)
            raise RuntimeError(f"Failed to scale logo image: {sdl2.SDL_GetError().decode()}")

        sdl2.SDL_FreeSurface(logo_ptr)
        return dst_surface

    def get_random_velocity(self) -> list[int]:
        vx = random.choice([-1, 1]) * random.randint(self.min_speed, self.max_speed)
        vy = random.choice([-1, 1]) * random.randint(self.min_speed, self.max_speed)
        return [vx, vy]

    def advance_logo(
        self,
        rect: sdl2.SDL_Rect,
        velocity: list[int],
        width: int,
        height: int,
        delta_time: float,
    ) -> None:
        rect.x += int(velocity[0] * delta_time * 60)
        rect.y += int(velocity[1] * delta_time * 60)

        if rect.x <= 0 or rect.x + rect.w >= width:
            velocity[0] *= -1
            rect.x = max(0, min(rect.x, width - rect.w))

        if rect.y <= 0 or rect.y + rect.h >= height:
            velocity[1] *= -1
            rect.y = max(0, min(rect.y, height - rect.h))

    def _dirty_rect(self, rect: sdl2.SDL_Rect) -> dlsdk.DirtyRect:
        return dlsdk.DirtyRect(rect.x, rect.y, rect.w, rect.h)

    def _copy_rect(self, rect: sdl2.SDL_Rect) -> sdl2.SDL_Rect:
        return sdl2.SDL_Rect(rect.x, rect.y, rect.w, rect.h)

    def _draw_fps_text(self, dst_surface: Surface, text: str, fps_text_color: tuple[int, int, int], pos_x: int, pos_y: int) -> sdl2.SDL_Rect:
        assert self.font is not None
        if text != self.fps_surface_text:
            if self.fps_surface_ptr is not None:
                sdl2.SDL_FreeSurface(self.fps_surface_ptr)
            color = sdl2.SDL_Color(fps_text_color[0], fps_text_color[1], fps_text_color[2], 255)
            self.fps_surface_ptr = sdl2.sdlttf.TTF_RenderUTF8_Blended(self.font, text.encode("utf-8"), color)
            if not self.fps_surface_ptr:
                raise RuntimeError(f"Failed to render FPS text: {sdl2.SDL_GetError().decode()}")
            self.fps_surface_text = text

        dst_rect = sdl2.SDL_Rect(pos_x, pos_y, self.fps_surface_ptr.contents.w, self.fps_surface_ptr.contents.h)
        if sdl2.SDL_BlitSurface(self.fps_surface_ptr, None, dst_surface.ptr, ctypes.byref(dst_rect)) != 0:
            raise RuntimeError(f"Failed to blit FPS text: {sdl2.SDL_GetError().decode()}")
        return dst_rect

    def should_quit(self) -> bool:
        event = sdl2.SDL_Event()
        while sdl2.SDL_PollEvent(ctypes.byref(event)) != 0:
            if event.type == sdl2.SDL_QUIT:
                return True
            if event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym in (sdl2.SDLK_ESCAPE, sdl2.SDLK_q):
                return True
        return False

    def run(self) -> int:
        self.args = self.parse_args()
        self.sdl2_init()
        self.display = DisplayLinkDisplay(prefer_1080p=self.args.prefer_1080p, embedded_mode=self.args.embedded_mode)
        self.clock = Clock()

        try:
            self.font = self._open_font(28)
            self.logo = self.load_logo_surface()
            self.logo_rect = sdl2.SDL_Rect(
                (self.display.width - self.logo.width) // 2,
                (self.display.height - self.logo.height) // 2,
                self.logo.width,
                self.logo.height,
            )
            self.logo_velocity = self.get_random_velocity()
            fps_text_time = 0.0
            fps_text = ""
            previous_logo_rect = self._copy_rect(self.logo_rect)
            previous_fps_rect = None

            while not self.should_quit():
                self.clock.tick()
                self.advance_logo(
                    self.logo_rect,
                    self.logo_velocity,
                    self.display.width,
                    self.display.height,
                    self.clock.get_delta_time(),
                )
                surface = self.display.surface()
                self._draw_background(surface)
                self._draw_logo(surface)
                fps_text_time += self.clock.get_delta_time()
                if fps_text_time >= 0.25 or not fps_text: # Update FPS text every 0.25 seconds or if it's not set
                    fps_text = f"Mode: {self.display.mode_string()} FPS: {self.clock.get_fps():5.1f}"
                    fps_text_time = 0.0
                current_fps_rect = self._draw_fps_text(surface, fps_text, self.fps_text_color, 12, 12)

                dirty_rects = [
                    self._dirty_rect(previous_logo_rect),
                    self._dirty_rect(self.logo_rect),
                    self._dirty_rect(current_fps_rect),
                ]
                if previous_fps_rect is not None:
                    dirty_rects.append(self._dirty_rect(previous_fps_rect))
                self.display.show(dirty_rects)

                previous_logo_rect = self._copy_rect(self.logo_rect)
                previous_fps_rect = current_fps_rect
        except KeyboardInterrupt:
            pass
        finally:
            self.sdl2_deinit()
        return 0


    def _draw_logo(self, surface):
        if sdl2.SDL_BlitSurface(self.logo.ptr, None, surface.ptr, ctypes.byref(self.logo_rect)) != 0:
            raise RuntimeError(f"Failed to blit logo: {sdl2.SDL_GetError().decode()}")

    def _draw_background(self, surface):
        fill_color = sdl2.SDL_MapRGB(surface.ptr.contents.format, *self.background_color)
        if sdl2.SDL_FillRect(surface.ptr, None, fill_color) != 0:
            raise RuntimeError(f"Failed to fill background: {sdl2.SDL_GetError().decode()}")

    def sdl2_init(self):
        sdl2.ext.init()
        image_flags = sdl2.sdlimage.IMG_INIT_PNG
        if (sdl2.sdlimage.IMG_Init(image_flags) & image_flags) != image_flags:
            raise RuntimeError(f"Failed to initialize SDL_image: {sdl2.SDL_GetError().decode()}")
        if sdl2.sdlttf.TTF_Init() != 0:
            raise RuntimeError(f"Failed to initialize SDL_ttf: {sdl2.SDL_GetError().decode()}")

    def sdl2_deinit(self):
        if self.fps_surface_ptr is not None:
            sdl2.SDL_FreeSurface(self.fps_surface_ptr)
            self.fps_surface_ptr = None
        if self.font is not None:
            sdl2.sdlttf.TTF_CloseFont(self.font)
        sdl2.sdlttf.TTF_Quit()
        sdl2.sdlimage.IMG_Quit()
        sdl2.ext.quit()

if __name__ == "__main__":
    raise SystemExit(LogoGame().run())
