// GStreamer variant of Player.

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <string.h>
#include "common_gst.hpp"


static gpointer mainloop_thread(gpointer data) {
    PlayerCtx* ctx = (PlayerCtx*)data;
    g_main_loop_run(ctx->loop);
    return nullptr;
}

static GstFlowReturn on_new_sample(GstAppSink* sink, gpointer user_data) {
    PlayerCtx* ctx = (PlayerCtx*)user_data;

    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;

    GstCaps* caps = gst_sample_get_caps(sample);

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        if (ctx->on_display) {
            ctx->on_display(ctx->fb, map.data);
        }
        gst_buffer_unmap(buffer, &map);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

static gboolean on_bus_msg(GstBus* bus, GstMessage* msg, gpointer user_data) {
    PlayerCtx* ctx = (PlayerCtx*)user_data;

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError* err = nullptr; gchar* dbg = nullptr;
        gst_message_parse_error(msg, &err, &dbg);
        g_printerr("GStreamer error: %s\n", err->message);
        g_error_free(err); g_free(dbg);
        if (ctx->loop) g_main_loop_quit(ctx->loop);
        break;
    }
    case GST_MESSAGE_EOS:
        if (ctx->loop_playback) {
            // Loop: seek to beginning
            gst_element_seek_simple(ctx->pipeline, GST_FORMAT_TIME,
                                    static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), 0);
            gst_element_set_state(ctx->pipeline, GST_STATE_PLAYING);
        } else {
            if (ctx->loop) g_main_loop_quit(ctx->loop);
        }
        break;
    default: break;
    }
    return TRUE;
}

GstElement* RunPlayer(const char* path,
                                unsigned char* fb,
                                unsigned width,
                                unsigned height,
                                unsigned bpp,
                                gboolean rotate90,
                                display_cb onDisplay)
{
    gst_init(nullptr, nullptr);

    PlayerCtx* ctx = g_new0(PlayerCtx, 1);
    ctx->fb = fb;
    ctx->width = width;
    ctx->height = height;
    ctx->bpp = bpp;
    ctx->on_display = onDisplay;
    ctx->loop_playback = TRUE;

    // Choose caps format from bpp
    const char* fmt = (bpp == 4) ? "BGRx" : (bpp == 3 ? "BGR" : "BGRx"); // default to BGRx
    gchar* canonical_path = g_canonicalize_filename(path, nullptr);
    GError* uri_error = nullptr;
    gchar* uri = g_filename_to_uri(canonical_path, nullptr, &uri_error);
    g_free(canonical_path);
    if (!uri) {
        g_error("Failed to convert path to URI: %s", uri_error ? uri_error->message : "unknown");
    }

    // Build the video-sink chain ending in appsink
    // Note: videoflip is conditional

    // TODO: this works well for Chimei Inn on Astra SL1680 but get the wrong framerate on other videos
    gchar* vsink_desc = g_strdup_printf(
        "videoconvert ! videoscale ! "
        "video/x-raw,format=%s,width=%u,height=%u ! %s "
        "queue max-size-buffers=1 leaky=downstream ! "
        "appsink name=sink emit-signals=true sync=false max-buffers=1 drop=true ",
        fmt, width, height,
        rotate90 ? "videoflip method=clockwise ! " : ""
    );

    // playbin drives demux/decode; we inject our custom video-sink
    ctx->pipeline = gst_element_factory_make("playbin", "player");
    if (!ctx->pipeline) {
        g_error("Failed to create playbin");
    }

    // Parse the video-sink string into an element
    GError* err = nullptr;
    GstElement* video_sink = gst_parse_bin_from_description(vsink_desc, TRUE, &err);
    if (!video_sink) {
        g_error("Failed to parse video-sink: %s", err ? err->message : "unknown");
    }

    g_object_set(ctx->pipeline,
                 "uri", uri,
                 "video-sink", video_sink,
                 // discard audio
                 "audio-sink", gst_element_factory_make("fakesink", nullptr),
                 nullptr);

    // Get the appsink handle to connect the callback
    ctx->appsink = gst_bin_get_by_name(GST_BIN(video_sink), "sink");
    g_signal_connect(ctx->appsink, "new-sample", G_CALLBACK(on_new_sample), ctx);

    // Setup bus to handle EOS/ERROR (for looping)
    GstBus* bus = gst_element_get_bus(ctx->pipeline);
    gst_bus_add_watch(bus, on_bus_msg, ctx);
    gst_object_unref(bus);

    // Main loop in a helper thread (so your app stays responsive)
    ctx->loop = g_main_loop_new(nullptr, FALSE);
    pthread_create(&ctx->loop_thread, nullptr, (void*(*)(void*))mainloop_thread, ctx);

    // Start playback
    gst_element_set_state(ctx->pipeline, GST_STATE_PLAYING);

    // We return the pipeline so caller can stop it later
    // (and keep ctx alive by associating it to pipeline)
    g_object_set_data_full(G_OBJECT(ctx->pipeline), "player-ctx", ctx, (GDestroyNotify)g_free);

    g_free(uri);
    g_free(vsink_desc);
    return ctx->pipeline;
}

// Call to stop/cleanup (similar to releasing libVLC objects)
void StopPlayer(GstElement* pipeline) {
    if (!pipeline) {
        g_printerr("StopPlayer: pipeline is NULL, nothing to clean up.\n");
        return;
    }
    PlayerCtx* ctx = (PlayerCtx*)g_object_get_data(G_OBJECT(pipeline), "player-ctx");

    g_printerr("StopPlayer: setting pipeline to NULL state.\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_element_get_state(pipeline, nullptr, nullptr, GST_CLOCK_TIME_NONE); // wait until NULL

    if (ctx) {
        if (ctx->loop) {
            g_printerr("StopPlayer: quitting main loop.\n");
            g_main_loop_quit(ctx->loop);
            g_printerr("StopPlayer: joining main loop thread.\n");
            pthread_join(ctx->loop_thread, nullptr);
            g_printerr("StopPlayer: unref main loop.\n");
            g_main_loop_unref(ctx->loop);
            ctx->loop = nullptr;
        } else {
            g_printerr("StopPlayer: main loop already NULL.\n");
        }
    } else {
        g_printerr("StopPlayer: PlayerCtx is NULL.\n");
    }

    g_printerr("StopPlayer: unref pipeline.\n");
    gst_object_unref(pipeline);
}