#include <vlc/vlc.h>


libvlc_media_player_t* RunPlayer(const char* path, unsigned char* fb, unsigned width, unsigned height, unsigned bpp, bool rotate, libvlc_video_display_cb);
void StopPlayer(libvlc_media_player_t* player);
