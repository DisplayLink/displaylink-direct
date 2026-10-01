#include "common.hpp"
#include "common_vlc.hpp"

static void* OnLock(void* object, void **planes)
{
  planes[0] = object;
  return object;
}

libvlc_media_player_t* RunPlayer(const char* path, unsigned char* fb, unsigned width, unsigned height, unsigned bpp, bool rotate, libvlc_video_display_cb onDisplay)
{
  const char* argv[] = {"-q", "--video-filter", "rotate", "--rotate-angle", "90"};
  const int argc = (rotate) ? 5 : 1;

  libvlc_instance_t* const library = libvlc_new(argc, argv);
  if (!library) {
    ExitFail("Unable to create library");
  }

  libvlc_media_t* const media = libvlc_media_new_path(library, path);
  if (!media) {
    libvlc_release(library);
    ExitFail("Unable to load media");
  }

  libvlc_media_add_option(media, "input-repeat=65535");
  libvlc_media_player_t* player = libvlc_media_player_new_from_media(media);
  libvlc_media_release(media);
  if (!player) {
    libvlc_release(library);
    ExitFail("Unable to start player");
  }

  libvlc_video_set_callbacks(player, OnLock, nullptr, onDisplay, fb);
  libvlc_video_set_format(player, "RV32", width, height, width * bpp);
  libvlc_media_player_play(player);

  return player;
}

void StopPlayer(libvlc_media_player_t* player)
{
  if (player) {
    libvlc_media_player_stop(player);
    libvlc_media_player_release(player);
  }
}

