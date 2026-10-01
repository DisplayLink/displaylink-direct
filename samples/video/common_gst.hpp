#include <gst/gst.h>
#include <pthread.h>

typedef void (*display_cb)(void* fb, void* pixels);

typedef struct {
    GstElement* pipeline;
    GstElement* appsink;
    GMainLoop*  loop;
    pthread_t   loop_thread;
    void*       fb;
    unsigned    width, height, bpp;
    display_cb  on_display;
    gboolean    loop_playback;
} PlayerCtx;


void StopPlayer(GstElement* pipeline);
GstElement* RunPlayer(const char* path,
                                unsigned char* fb,
                                unsigned width,
                                unsigned height,
                                unsigned bpp,
                                gboolean rotate90,
                                display_cb onDisplay);
