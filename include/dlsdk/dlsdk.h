// Copyright (C) 2024 DisplayLink (UK) Ltd.
// Copyright (C) 2025 - 2026 Synaptics Incorporated

/**
 * \mainpage DisplayLink Direct API Documentation
 *
 * \section Introduction
 * This file contains the C/C++ API used to display pixel buffers across multiple displays.
 *
 * \subsection tagContact Contact and Support
 * For support, contact your DisplayLink representative or email technical-enquiries@synaptics.com.
 *
 * \subsection tagThreadSafety Thread Safety
 * The API is thread-safe: it may be called concurrently from multiple threads, including on the
 * same handle (e.g. dlsdk_device_handle, dlsdk_display_handle, dlsdk_dpaux_handle), without
 * external locking.
 * Concurrent calls on the same handle are serialised internally, so a long-running call such as
 * dlsdk_device_update_firmware() will block other threads until it completes.
 *
 * The following remain the caller's responsibility:
 * \li dlsdk_initialise(), dlsdk_initialise_with_config() and dlsdk_teardown() are not thread-safe.
 *     Call them from a single thread, and ensure no other API call is in progress on any thread
 *     when calling dlsdk_teardown().
 * \li All handles are owned by the SDK; they are invalidated on dlsdk_teardown() and must not be
 *     used after that point.
 * \li Serialization of concurrent calls on the same handle is handled internally by the SDK, but
 *     the order is time-dependent. Callers must still sequence dependent operations.
 * \li A hotplug callback must not call dlsdk_register_hotplug_callback() or
 *     dlsdk_unregister_hotplug_callback(), as this will deadlock.
 */

/**
 * \file  dlsdk.h
 *
 * \brief This file contains the C-API and C++ API wrapper available to display a pixel buffer
 *        across multiple displays.
 */

#pragma once

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifdef _EXPORTING
#define DLSDK_EXPORT __declspec(dllexport)
#else
#define DLSDK_EXPORT __declspec(dllimport)
#endif
#else
#define DLSDK_EXPORT __attribute__((visibility("default")))
#endif

// API internals
struct dlsdk_device;
struct dlsdk_display;
struct dlsdk_dpaux;
struct dlsdk_display_hotplug_callback;

/** \defgroup dlSdkCApi DisplayLink Direct c-API
 *  @{
 */

/** \defgroup apiTypes Types
 * \brief Custom types used across the API
 *  @{
 */

/**
 * \brief Configuration flags used to initialise the DisplayLink Direct.
 */
typedef enum dlsdk_config_flags_t
{
  DLSDK_CONFIG_FLAG_EMBEDDED_MODE = 1U << 0,  /**< In embedded mode, DisplayLink Direct will not use
                                                   dynamic memory at runtime. */
} dlsdk_config_flags;

/**
 * \brief Configuration structure used to initialise the DisplayLink Direct with custom options.
 *        Initialise this structure with DLSDK_CONFIG_INIT(&config).
 */
typedef struct dlsdk_config
{
  uint32_t size;             /**< Size of the structure; is set by DLSDK_CONFIG_INIT(&config). */
  uint32_t flags;            /**< Combination of dlsdk_config_flags values. */
  const char* firmwarePath;  /**< Firmware directory path used to update the firmware automatically
                                  if required. Set to NULL or an empty string to disable automatic
                                  firmware updates. If automatic firmware updates are disabled, the
                                  user should ensure that the firmware on the device is compatible
                                  with the DisplayLink Direct version; otherwise, the device may not
                                  function properly or at all. */
} dlsdk_config;

/**
 * \brief Default initialiser for dlsdk_config.
 *
 * \details Zero-initialises the complete structure, and initialises the size field.
 *          After using this macro, callers may set firmwarePath and flags as needed.
 */
#define DLSDK_CONFIG_INIT(VALUE)           \
  do {                                     \
    memset((VALUE), 0, sizeof(*(VALUE)));  \
    (VALUE)->size = sizeof(*(VALUE));      \
  } while (0)

/**
 * \brief An abstract representation of a rectangular area.
 */
typedef struct dlsdk_rect
{
  unsigned int width;
  unsigned int height;
} dlsdk_rect;

/**
 * \brief Maximum number of dirty rectangles accepted by dlsdk_display_show()
 */
enum
{
  DLSDK_MAX_DIRTY_RECTS = 128
};

/**
 * \brief A rectangular area of a pixel buffer, relative to its top-left corner
 */
typedef struct dlsdk_dirty_rect
{
  unsigned int x;
  unsigned int y;
  unsigned int width;
  unsigned int height;
} dlsdk_dirty_rect;

/**
 * \brief Represents a specific resolution and refresh rate at which a display can operate.
 */
typedef struct dlsdk_display_mode
{
  dlsdk_rect resolution;
  unsigned int refreshRateHz;
} dlsdk_display_mode;

/**
 * \brief Detailed video timing parameters used to power on a display with a fully custom timing,
 *        rather than one of the modes reported by dlsdk_display_modes().
 */
typedef struct dlsdk_display_timing
{
  uint16_t horizontalResolution;  /**< Active horizontal resolution, in pixels */
  uint16_t verticalResolution;    /**< Active vertical resolution, in pixels */
  uint32_t refreshRateMilliHz;    /**< Refresh rate, in milli-Hertz (e.g. 60000 for 60Hz) */
  uint16_t hTotal;                /**< Total horizontal line time, in pixels */
  uint16_t vTotal;                /**< Total vertical frame time, in lines */
  uint16_t hStart;                /**< Offset from the start of the line to the start of active
                                       video, in pixels (hsync width + back porch) */
  uint16_t vStart;                /**< Offset from the start of the frame to the start of active
                                       video, in lines (vsync width + back porch) */
  uint16_t hSyncWidth;            /**< Horizontal sync pulse width, in pixels */
  uint16_t vSyncWidth;            /**< Vertical sync pulse width, in lines */
  uint8_t hSyncPolarityPositive;  /**< Non-zero if the horizontal sync pulse is active-high */
  uint8_t vSyncPolarityPositive;  /**< Non-zero if the vertical sync pulse is active-high */
} dlsdk_display_timing;

/**
 * \brief Display signal mode used by the display signal.
 */
typedef enum dlsdk_display_signal_mode_t
{
  DLSDK_DISPLAY_SIGNAL_MODE_SDR = 0,
  DLSDK_DISPLAY_SIGNAL_MODE_HDR10,
} dlsdk_display_signal_mode;

/**
 * \brief Options used when powering on a display.
 *
 * \details Initialise this structure with DLSDK_DISPLAY_POWER_ON_OPTIONS_INIT(&options).
 *          Optionally set the signalMode, mode, and timing fields to request a specific display
 *          configuration. The mode and timing pointers are mutually exclusive; when both are NULL,
 *          the preferred display mode is used.
 *          When using a signalMode of DLSDK_DISPLAY_SIGNAL_MODE_HDR10, you must use
 *          DLSDK_PIXEL_FORMAT_RGB10 or DLSDK_PIXEL_FORMAT_XBGR16F in dlsdk_display_show().
 */
typedef struct dlsdk_display_power_on_options
{
  uint32_t size;                         /**< Size of this structure; initialise with
                                              DLSDK_DISPLAY_POWER_ON_OPTIONS_INIT(&options). */
  dlsdk_display_signal_mode signalMode;  /**< Display signal mode used by the display. */
  const dlsdk_display_mode* mode;        /**< Optional requested display mode. */
  const dlsdk_display_timing* timing;    /**< Optional requested custom timing. */
} dlsdk_display_power_on_options;

/**
 * \brief Default initialiser for dlsdk_display_power_on_options.
 *
 * \details Zero-initialises the complete structure, and initialises a preferred-mode SDR power-on
 *          request.
 */
#define DLSDK_DISPLAY_POWER_ON_OPTIONS_INIT(VALUE)        \
  do {                                                    \
    memset((VALUE), 0, sizeof(*(VALUE)));                 \
    (VALUE)->size = sizeof(*(VALUE));                     \
    (VALUE)->signalMode = DLSDK_DISPLAY_SIGNAL_MODE_SDR;  \
    (VALUE)->mode = NULL;                                 \
    (VALUE)->timing = NULL;                               \
  } while (0)

/**
 * \brief API method status return codes.
 */
typedef enum dlsdk_status_t
{
  DLSDK_SUCCESS = 0,
  DLSDK_NOT_ENOUGH_SPACE,
  DLSDK_UNSUCCESSFUL,
  DLSDK_UNSUCCESSFUL_NO_DEVICE,
  DLSDK_UNSUCCESSFUL_NO_MONITOR,
  DLSDK_UNSUCCESSFUL_MONITOR_OFF,
  DLSDK_INVALID_HANDLE,
  DLSDK_INVALID_ARGS,
  DLSDK_TIMEOUT,
  DLSDK_NOT_IMPLEMENTED,
  DLSDK_BUSY,
} dlsdk_status;

/**
 * \brief API-supported pixel formats.
 *
 * \note XRGB and XBGR are recommended for maximum compatibility and performance.
 */
typedef enum dlsdk_pixel_format_t
{
  DLSDK_PIXEL_FORMAT_XRGB = 0, /**< 8 bits per channel. 4 bytes per pixel. */
  DLSDK_PIXEL_FORMAT_XBGR,     /**< 8 bits per channel. 4 bytes per pixel. */
  DLSDK_PIXEL_FORMAT_RGB565,   /**< 5/6/5 bits per channel. 2 bytes per pixel. */
  DLSDK_PIXEL_FORMAT_RGB,      /**< 8 bits per channel. 3 bytes per pixel. */
  DLSDK_PIXEL_FORMAT_RGB10,    /**< 10 bits per channel. 4 bytes per pixel. Top two bits unused. */
  DLSDK_PIXEL_FORMAT_XBGR16F,  /**< 16 bit half-floats per channel. 8 bytes per pixel. */
} dlsdk_pixel_format;

/**
 * \brief Flags identifying the static HDR metadata format associated with a frame.
 */
typedef enum dlsdk_hdr_metadata_flags_t
{
  DLSDK_HDR_METADATA_FLAG_NONE = 0,
  DLSDK_HDR_METADATA_HDR10 = 1U << 0,
} dlsdk_hdr_metadata_flags;

/**
 * \brief SMPTE ST 2086 mastering display color volume metadata.
 *
 * \details Chromaticity values are normalised CIE 1931 x/y coordinates in the range [0, 1].
 *          Luminance values are in cd/m^2 (nits).
 */
typedef struct dlsdk_hdr_smpte2086
{
  float redX;          /**< Red primary x coordinate. */
  float redY;          /**< Red primary y coordinate. */
  float greenX;        /**< Green primary x coordinate. */
  float greenY;        /**< Green primary y coordinate. */
  float blueX;         /**< Blue primary x coordinate. */
  float blueY;         /**< Blue primary y coordinate. */
  float whiteX;        /**< White point x coordinate. */
  float whiteY;        /**< White point y coordinate. */
  float maxLuminance;  /**< Maximum mastering luminance in cd/m^2. */
  float minLuminance;  /**< Minimum mastering luminance in cd/m^2. */
} dlsdk_hdr_smpte2086;

/**
 * \brief CTA-861.3 content light level metadata.
 *
 * \details Light-level values are in cd/m^2 (nits).
 */
typedef struct dlsdk_hdr_cta861_3
{
  float maxContentLightLevel;       /**< Maximum content light level. */
  float maxFrameAverageLightLevel;  /**< Maximum frame-average light level. */
} dlsdk_hdr_cta861_3;

typedef struct dlsdk_hdr10_metadata
{
  dlsdk_hdr_smpte2086 smpte2086;  /**< SMPTE 2086 mastering display color volume metadata. */
  dlsdk_hdr_cta861_3 cta861_3;    /**< CTA-861.3 content light level metadata. */
} dlsdk_hdr10_metadata;

/**
 * \brief Static HDR metadata associated with a frame.
 */
typedef struct dlsdk_hdr_static_metadata
{
  uint32_t size;                   /**< Size of this structure; initialise with
                                        DLSDK_HDR_STATIC_METADATA_INIT(&metadata). */
  uint32_t flags;                  /**< Combination of dlsdk_hdr_metadata_flags values. */
  dlsdk_hdr10_metadata hdr10;      /**< Complete HDR10 payload when its flag is set. */
} dlsdk_hdr_static_metadata;

/**
 * \brief Initialises dlsdk_hdr_smpte2086.
 */
#define DLSDK_HDR_SMPTE2086_INIT(VALUE)    \
  do {                                     \
    memset((VALUE), 0, sizeof(*(VALUE)));  \
    (VALUE)->redX = 0.0f;                  \
    (VALUE)->redY = 0.0f;                  \
    (VALUE)->greenX = 0.0f;                \
    (VALUE)->greenY = 0.0f;                \
    (VALUE)->blueX = 0.0f;                 \
    (VALUE)->blueY = 0.0f;                 \
    (VALUE)->whiteX = 0.0f;                \
    (VALUE)->whiteY = 0.0f;                \
    (VALUE)->maxLuminance = 0.0f;          \
    (VALUE)->minLuminance = 0.0f;          \
  } while (0)

/**
 * \brief Initialises dlsdk_hdr_cta861_3.
 */
#define DLSDK_HDR_CTA861_3_INIT(VALUE)          \
  do {                                          \
    memset((VALUE), 0, sizeof(*(VALUE)));       \
    (VALUE)->maxContentLightLevel = 0.0f;       \
    (VALUE)->maxFrameAverageLightLevel = 0.0f;  \
  } while (0)

/**
 * \brief Initialises dlsdk_hdr_static_metadata.
 */
#define DLSDK_HDR_STATIC_METADATA_INIT(VALUE)             \
  do {                                                    \
    memset((VALUE), 0, sizeof(*(VALUE)));                 \
    (VALUE)->size = sizeof(*(VALUE));                     \
    (VALUE)->flags = DLSDK_HDR_METADATA_FLAG_NONE;        \
    DLSDK_HDR_SMPTE2086_INIT(&(VALUE)->hdr10.smpte2086);  \
    DLSDK_HDR_CTA861_3_INIT(&(VALUE)->hdr10.cta861_3);    \
  } while (0)

/**
 * \brief Display capabilities relevant to signal modes and pixel formats.
 *
 * \details The signalModeMask uses the value of dlsdk_display_signal_mode as its bit index. The
 *          pixelFormatMask uses the value of dlsdk_pixel_format as its bit index.
 *          Use DLSDK_DISPLAY_CAPABILITIES_INIT(&capabilities) to initialise this structure before
 *          calling dlsdk_display_get_capabilities().
 */
typedef struct dlsdk_display_capabilities
{
  uint32_t size;                 /**< Size of this structure; initialise with
                                       DLSDK_DISPLAY_CAPABILITIES_INIT(&capabilities). */
  uint32_t pixelFormatMask;      /**< Supported dlsdk_pixel_format values. */
  uint32_t signalModeMask;       /**< Supported dlsdk_display_signal_mode values. */
} dlsdk_display_capabilities;

/**
 * \brief Default initialiser for dlsdk_display_capabilities.
 *
 * \details Zero-initialises the complete structure, and initialises the structure size.
 *          The display capability query fills the output fields.
 */
#define DLSDK_DISPLAY_CAPABILITIES_INIT(VALUE)  \
  do {                                          \
    memset((VALUE), 0, sizeof(*(VALUE)));       \
    (VALUE)->size = sizeof(*(VALUE));           \
    (VALUE)->pixelFormatMask = 0U;              \
    (VALUE)->signalModeMask = 0U;               \
  } while (0)

/**
 * \brief Frame description used by dlsdk_display_show().
 *        Initialise this structure with DLSDK_FRAME_INIT(&frame, FORMAT, PIXELS, PIXELS_SIZE).
 */
typedef struct dlsdk_frame
{
  uint32_t size;                      /**< Size of this structure, is set by
                                           DLSDK_FRAME_INIT(&frame, FORMAT, PIXELS, PIXELS_SIZE). */
  dlsdk_pixel_format format;          /**< The pixel format contained in the pixel buffer. */
  const uint8_t* pixels;              /**< The buffer containing the pixels to be displayed.
                                           For optimal performance, a 64-byte aligned pixel buffer
                                           address is preferred. */
  uint32_t pixelsSize;                /**< Size in bytes of the pixel buffer. Must be non-zero and
                                           large enough for display height and effective stride. */
  uint32_t stride;                    /**< Number of bytes between two consecutive rows. If 0, it
                                           is assumed to be bytesPerPixel * displayWidth.
                                           For optimal performance, a 64-byte aligned stride is
                                           preferred. */
  const dlsdk_dirty_rect* dirtyRects; /**< Array of changed regions. NULL or zero count means
                                           full-display update. */
  uint32_t dirtyRectCount;            /**< Number of rectangles in dirtyRects. Zero when dirtyRects
                                           is NULL, less than or equal to DLSDK_MAX_DIRTY_RECTS
                                           otherwise. */
  const dlsdk_hdr_static_metadata* staticMetadata; /**< Optional static HDR metadata. */
} dlsdk_frame;

/**
 * \brief Default initialiser for dlsdk_frame.
 *
 * \details Zero-initialises the complete structure, and initialises the required fields.
 *          After using this macro, callers may set the optional parameters: stride, dirtyRects,
 *          dirtyRectCount, and staticMetadata as needed.
 */
#define DLSDK_FRAME_INIT(VALUE, FORMAT, PIXELS, PIXELS_SIZE)  \
  do {                                                        \
    memset((VALUE), 0, sizeof(*(VALUE)));                     \
    (VALUE)->size = sizeof(*(VALUE));                         \
    (VALUE)->format = (FORMAT);                               \
    (VALUE)->pixels = (const uint8_t*)(PIXELS);               \
    (VALUE)->pixelsSize = (uint32_t)(PIXELS_SIZE);            \
    (VALUE)->stride = 0U;                                     \
    (VALUE)->dirtyRects = NULL;                               \
    (VALUE)->dirtyRectCount = 0U;                             \
    (VALUE)->staticMetadata = NULL;                           \
  } while (0)

/**
 * \brief DP AUX channel return codes.
 */
typedef enum dlsdk_aux_status_t
{
  // Standard DP response codes
  DLSDK_AUX_ACK = 0x0,
  DLSDK_AUX_NATIVE_NAK = 0x1,
  DLSDK_AUX_NATIVE_DEFER = 0x2,

  // DisplayLink Direct defined errors where response is missing or corrupted
  DLSDK_AUX_TIMEOUT = 0x10,  // Timeout, no response from downstream
  DLSDK_AUX_ERROR = 0x11,    // Error detected by DP hardware
  DLSDK_AUX_DETACHED = 0x12, // Sink connection no longer present
} dlsdk_aux_status;

/**
 * \brief Opaque handle to a hotplug callback registration, used for unregistering callbacks
 */
typedef struct dlsdk_hotplug_callback* dlsdk_hotplug_callback_handle;
/**
 *  \brief Opaque handle to a Display object
 */
typedef struct dlsdk_display* dlsdk_display_handle;
/**
 * \brief Opaque handle to a Device object
 */
typedef struct dlsdk_device* dlsdk_device_handle;
/**
 * \brief Opaque handle to a DP AUX channel object
 */
typedef struct dlsdk_dpaux* dlsdk_dpaux_handle;

/**
 * \brief Hotplug event types
 */
typedef enum dlsdk_hotplug_event_t
{
  DLSDK_HOTPLUG_EVENT_DEVICE_ARRIVED,
  DLSDK_HOTPLUG_EVENT_DEVICE_REMOVED,
  DLSDK_HOTPLUG_EVENT_DISPLAY_ARRIVED,
  DLSDK_HOTPLUG_EVENT_DISPLAY_REMOVED,
} dlsdk_hotplug_event;

/**
 * \brief Version constant for dlsdk_hotplug_event_data.
 */
enum
{
  DLSDK_HOTPLUG_EVENT_DATA_VERSION_1 = 1, /**< event, device, device_id, display, display_id */
  DLSDK_HOTPLUG_EVENT_DATA_VERSION_CURRENT = DLSDK_HOTPLUG_EVENT_DATA_VERSION_1
};

/**
 * \brief Data provided to hotplug callbacks when a hotplug event occurs
 *
 * Handle validity rules:
 * - On DLSDK_HOTPLUG_EVENT_DEVICE_ARRIVED, \p device is valid during the callback and will remain
 *   valid until a corresponding DLSDK_HOTPLUG_EVENT_DEVICE_REMOVED event is received for the
 *   same device.
 * - On DLSDK_HOTPLUG_EVENT_DEVICE_REMOVED, \p device is informational only and must not be used
 *   with device API calls.
 * - On DLSDK_HOTPLUG_EVENT_DISPLAY_ARRIVED, \p display is valid during the callback. The value is
 *   NULL if the event is not for a display (for example, a device hotplug event).
 * - On DLSDK_HOTPLUG_EVENT_DISPLAY_REMOVED, \p display is informational only and must not be used
 *   with display API calls.
 */
typedef struct dlsdk_hotplug_event_data
{
  uint32_t version;             /**< Struct version set by the DisplayLink Direct. */
  dlsdk_hotplug_event event;    /**< The type of hotplug event that occurred. */
  dlsdk_device_handle device;   /**< Handle to the device on which the event occurred. */
  const char* device_id;        /**< Unique identifier for the device involved in the event.
                                     Valid for the duration of the callback. */
  dlsdk_display_handle display; /**< Handle to the display involved in the event if applicable.*/
  const char* display_id;       /**< Unique identifier for the display involved in the event.
                                     Valid for the duration of the callback. */
} dlsdk_hotplug_event_data;

/**
 * \brief Hotplug callback function type.
 *
 * \details You must not register or unregister callbacks from within a callback.
 *          Callbacks are invoked on an internal DisplayLink Direct worker thread.
 *          Callbacks are serialised and delivered in event order.
 *
 * \param[in] data        Non-null pointer to a struct containing event data.
 * \param[in] user_data   User data pointer provided at registration.
 */
typedef void (*dlsdk_hotplug_callback_fn)(dlsdk_hotplug_event_data* data, void* user_data);

/** @}*/  // End group apiTypes

/** \defgroup apiMethods Methods
 *  @{
 */
/** \defgroup displayApi Display
 *  \brief Functions for manipulating/controlling displays attached to a device
 *  @{
 */

/**
 * \brief Function used to obtain the unique identifier of the display.
 *
 * \details Identifies a single output on a device; a device with multiple outputs exposes one
 *          identifier per output. For example, "USB_6015-10096418^0" and "USB_6015-10096418^1",
 *          where 6015 is the USB product ID (in hex), 10096418 is the serial number, and the suffix
 *          after ^ is the output index. The DisplayLink USB vendor ID (0x17E9) is not included.
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \return A string containing the device and head identifiers.
 */
DLSDK_EXPORT const char* dlsdk_display_id(dlsdk_display_handle display);

/**
 * \brief Function used to obtain the resolution of the display
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \return The resolution of the display
 */
DLSDK_EXPORT struct dlsdk_rect dlsdk_display_size(dlsdk_display_handle display);

/**
 * \brief Function used to obtain the EDID of the display
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \param[out] data Buffer to store the EDID data. If null, the function will return the required
 *                  buffer size in len.
 * \param[in, out] len Length of the data buffer.
 *                     On input, should be set to the size of the data buffer.
 *                     On output, will be set to the actual length of the EDID data written to the
 *                     buffer, or the required buffer size if buffer is null.
 *                     If the buffer is too small, the data will be truncated.
 * \return DLSDK_SUCCESS if the EDID was obtained successfully, or the reason for failure otherwise.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_edid(dlsdk_display_handle display,
                                             uint8_t* data,
                                             uint32_t* len);

/**
 * \brief Function used to retrieve a list of supported display modes for the display.
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \param[out] modes Buffer to store the display modes. If null, the function will return the
 *                   required buffer size in count.
 * \param[in, out] count Number of display modes that can be stored in the modes buffer. On input,
 *                       should be set to the size of the modes buffer. On output, will be set to
 *                       the actual number of display modes written to the buffer, or the required
 *                       buffer size if buffer is null.
 * \return DLSDK_SUCCESS if the display modes were obtained successfully, or the reason for failure
 *         otherwise.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_modes(dlsdk_display_handle display,
                                              dlsdk_display_mode* modes,
                                              unsigned int* count);

/**
 * \brief Function used to obtain signal mode and pixel format capabilities for the display.
 *
 * \details Initialise capabilities with DLSDK_DISPLAY_CAPABILITIES_INIT(&capabilities) before
 *          calling this function; the macro sets capabilities->size to
 *          sizeof(dlsdk_display_capabilities).
 *          The signalModeMask uses the value of dlsdk_display_signal_mode as its bit index.
 *          The pixelFormatMask uses the value of dlsdk_pixel_format as its bit index.
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \param[out] capabilities Buffer to store the display capabilities. Must be valid and non-NULL
 * \return DLSDK_SUCCESS if the capabilities were obtained successfully, or the reason for failure
 *         otherwise.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_get_capabilities(dlsdk_display_handle display,
                                                         dlsdk_display_capabilities* capabilities);

/**
 * \brief Function used to obtain the preferred display mode for the display
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \param[out] mode Buffer to store the preferred display mode
 * \return DLSDK_SUCCESS if the preferred display mode was obtained successfully, or the reason for
 *         failure otherwise.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_preferred_mode(dlsdk_display_handle display,
                                                       dlsdk_display_mode* mode);

/**
 * \brief Function used to power on the display in its preferred mode and SDR signal mode.
 *
 * \details This is equivalent to calling dlsdk_display_power_on_with_options() with an options
 *          structure initialised by DLSDK_DISPLAY_POWER_ON_OPTIONS_INIT(&options).
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \return DLSDK_SUCCESS or an appropriate error code on failure.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_power_on(dlsdk_display_handle display);

/**
 * \brief Function used to power on the display with custom options.
 *
 * \details The display is powered on using the preferred display mode when options->mode and
 *          options->timing are NULL.
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \param[in] options Power-on options. Must be valid and non-NULL. Initialise it with
 *                    DLSDK_DISPLAY_POWER_ON_OPTIONS_INIT(&options) before setting optional fields.
 *                    The mode and timing pointers are mutually exclusive.
 * \return DLSDK_SUCCESS or an appropriate error code on failure.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_power_on_with_options(
                                                     dlsdk_display_handle display,
                                                     const dlsdk_display_power_on_options* options);

/**
 * \brief Function used to power off the display
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \return DLSDK_SUCCESS or an appropriate error code on failure.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_power_off(dlsdk_display_handle display);

/**
 * \brief Function to present frame contents on the display.
 *
 * \details This is a non-blocking call and returns before the pixels are displayed. To receive
 *          notification when the pixels have been displayed, call dlsdk_display_wait_on_show().
 *          The frame can describe either a full-screen update or a partial update using dirty
 *          rectangles.
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \param[in] frame Frame description. Must be non-NULL and initialised with
 *                  DLSDK_FRAME_INIT(&frame, FORMAT, PIXELS, PIXELS_SIZE).
 *                  Frame buffer requirements:
 *                  - frame->pixels must be non-NULL.
 *                  - frame->pixelsSize must be non-zero and at least frame->stride * displayHeight.
 *                  - If frame->stride is zero, it is assumed to be bytesPerPixel * displayWidth;
 *                    otherwise is must be greater than or equal to this value.
 *                  - For optimal performance, 64-byte alignment of both frame->pixels address and
 *                    frame->stride is preferred.
 *
 *                  Dirty rectangle requirements:
 *                  - frame->dirtyRects is an optional hint to the display of which regions of the
 *                    pixel buffer have changed since the previous frame, allowing implementations
 *                    that support partial updates to only update those regions.
 *                  - The rectangles must cover everything that changed, as changes outside of them
 *                    may not be presented on the display.
 *                  - The frame->pixels buffer must still describe the entire display,
 *                    as the DisplayLink Direct may access regions outside of those given.
 *                  - Rectangles are clipped to the display area, and rectangles that fall entirely
 *                    outside it are ignored.
 *                  - If frame->dirtyRects is NULL, a full-screen update is assumed.
 *                  - frame->dirtyRectCount must be <= DLSDK_MAX_DIRTY_RECTS.
 *                  - If frame->dirtyRectCount is non-zero, frame->dirtyRects must be non-NULL.
 *
 *                  When the display is powered on in HDR10 mode, the frame format must provide at
 *                  least 10 bits per channel of source precision and be supported by the display
 *                  capabilities.
 *                  If frame->staticMetadata is non-NULL, initialise it with
 *                  DLSDK_HDR_STATIC_METADATA_INIT(&metadata), then its metadata applied
 *                  synchronously with the frame.
 *
 * \return DLSDK_SUCCESS if the frame was successfully submitted for rendering, or an error code
 *         on failure. The caller should check the render and frame transmission status with
 *         dlsdk_display_wait_on_show().
 *         DLSDK_INVALID_ARGS for invalid frame parameters.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_show(dlsdk_display_handle display,
                                             const dlsdk_frame* frame);

/**
 * \brief Function used to block for notification that pixels have been displayed after calling
 *        dlsdk_display_show().
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \return DLSDK_SUCCESS when the pixels have been displayed.
 * \return DLSDK_UNSUCCESSFUL_NO_MONITOR when display is not ready.
 * \return DLSDK_BUSY when the device aborted a previously submitted frame due to resource
 *         constraints. The caller should retry dlsdk_display_show() with the frame to be displayed,
 *         which will typically involve more aggressive compression, or proceed with a new frame if
 *         the previous frame is no longer relevant.
 * \return DLSDK_TIMEOUT if waiting for the display to be ready timed out.
 * \return DLSDK_UNSUCCESSFUL on unknown error.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_wait_on_show(dlsdk_display_handle display);

/**
 * \brief Function used to block until the pixels have been displayed after calling
 *        dlsdk_display_show(), with a timeout option.
 * \param[in] display Display handle. Must be valid and non-NULL
 * \param[in] timeoutMs Maximum time to wait in milliseconds
 * \return DLSDK_SUCCESS when the pixels have been displayed.
 * \return DLSDK_UNSUCCESSFUL_NO_MONITOR when display is not ready.
 * \return DLSDK_BUSY when the device aborted a previously submitted frame due to resource
 *         constraints. The caller should retry dlsdk_display_show() with the frame to be displayed,
 *         which will typically involve more aggressive compression, or proceed with a new frame if
 *         the previous frame is no longer relevant.
 * \return DLSDK_TIMEOUT if waiting for the display to be ready timed out.
 * \return DLSDK_UNSUCCESSFUL on unknown error.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_wait_on_show_for(dlsdk_display_handle display,
                                                         uint32_t timeoutMs);

/**
 * \brief Function used to clear the display.
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \return DLSDK_SUCCESS or an appropriate error code on failure.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_clear(dlsdk_display_handle display);

/**
 * \brief Function used to obtain a handle to the DP AUX channel associated with this display.
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \return A handle to the DP AUX channel, or NULL if the display is not connected through
 *         DisplayPort.
 */
DLSDK_EXPORT dlsdk_dpaux_handle dlsdk_display_get_dpaux(dlsdk_display_handle display);

/**
 * \brief Function to get the brightness of the display.
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \param[out] brightness Current brightness level (0-100)
 * \return DLSDK_SUCCESS on success, or an error code indicating the failure reason.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_get_brightness(dlsdk_display_handle display,
                                                       uint8_t* brightness);

/**
 * \brief Function to set the brightness of the display.
 *
 * \param[in] display Display handle. Must be valid and non-NULL
 * \param[in] brightness Brightness level to set (0-100)
 * \return DLSDK_SUCCESS on success, or an error code indicating the failure reason.
 */
DLSDK_EXPORT dlsdk_status dlsdk_display_set_brightness(dlsdk_display_handle display,
                                                       uint8_t brightness);
/** @}*/  // End group displayApi


/** \defgroup dpAuxApi DpAux
 *  \brief Functions for DpAux channel on the attached display
 *  @{
 */
/**
 * Reads data from native DP AUX registers using 16-byte burst transactions. Larger requests are
 * serviced by chaining multiple burst reads into the provided buffer. The function waits for a
 * response for up to 400 µs.
 * \param[in] dpaux DP AUX channel handle. Must be valid and non-NULL
 * @param auxRegAddress   DP register address
 * @param data            Buffer for incoming data
 * @param length          Data buffer length (0 <= length < 256)
 * @return status response from DP peer
 */
DLSDK_EXPORT dlsdk_aux_status dlsdk_dpaux_read(dlsdk_dpaux_handle dpaux,
                                               uint32_t auxRegAddress,
                                               uint8_t* data,
                                               uint8_t length);

/**
 * Writes data to native DP AUX registers using 16-byte burst transactions. Larger requests are
 * split into multiple burst writes from the provided buffer. The function waits for a response for
 * up to 400 µs.
 * \param[in] dpaux DP AUX channel handle. Must be valid and non-NULL
 * @param auxRegAddress   DP register address
 * @param data            Buffer of write data
 * @param length          Data buffer length (0 <= length < 256)
 * @return status response from DP peer
 */
DLSDK_EXPORT dlsdk_aux_status dlsdk_dpaux_write(dlsdk_dpaux_handle dpaux,
                                                uint32_t auxRegAddress,
                                                const uint8_t* data,
                                                uint8_t length);

/**
 * Reads I2C data using an AUX transaction.
 * \param[in] dpaux DP AUX channel handle. Must be valid and non-NULL
 * @param deviceAddress   I2C device address (bits 6:0)
 * @param data            Read data buffer
 * @param length          Data buffer length (0 <= length < 256)
 * @param mot             A value of 1 indicates the middle of a transaction; a value of 0
 *                        terminates the I2C transaction with a stop.
 * @param timeoutMs       Allowed timeout per transaction.
 * @return status response from DP peer
 */
DLSDK_EXPORT dlsdk_aux_status dlsdk_dpaux_i2c_read(dlsdk_dpaux_handle dpaux,
                                                   uint8_t deviceAddress,
                                                   uint8_t* data,
                                                   uint8_t length,
                                                   uint8_t mot,
                                                   uint32_t timeoutMs);

/**
 * Writes I2C data using an AUX transaction.
 * \param[in] dpaux DP AUX channel handle. Must be valid and non-NULL
 * @param deviceAddress   I2C device address (bits 6:0)
 * @param data            Write data buffer
 * @param length          Data buffer length (0 <= length < 256)
 * @param mot             A value of 1 indicates the middle of a transaction; a value of 0
 *                        terminates the I2C transaction with a stop.
 * @param timeoutMs       Allowed timeout per transaction.
 * @return status response from DP peer
 */
DLSDK_EXPORT dlsdk_aux_status dlsdk_dpaux_i2c_write(dlsdk_dpaux_handle dpaux,
                                                    uint8_t deviceAddress,
                                                    const uint8_t* data,
                                                    uint8_t length,
                                                    uint8_t mot,
                                                    uint32_t timeoutMs);

/**
 * Writes and reads I2C data using an AUX transaction.
 * \param[in] dpaux DP AUX channel handle. Must be valid and non-NULL
 * @param deviceAddress   I2C device address (bits 6:0)
 * @param writeData       Write data buffer
 * @param writeLength     Write data buffer length (0 <= length < 256).
 * @param readData        Read data buffer.
 * @param readLength      Read data buffer length (0 <= length < 256).
 * @param timeoutMs       Allowed timeout per transaction.
 * @return status response from DP peer
 */
DLSDK_EXPORT dlsdk_aux_status dlsdk_dpaux_i2c_write_and_read(dlsdk_dpaux_handle dpaux,
                                                             uint8_t deviceAddress,
                                                             const uint8_t* writeData,
                                                             uint8_t writeLength,
                                                             uint8_t* readData,
                                                             uint8_t readLength,
                                                             uint32_t timeoutMs);

/**
 * Begins exclusive access to the DP AUX channel.
 * \param[in] dpaux DP AUX channel handle. Must be valid and non-NULL
 * \param[in] timeoutMs Maximum time to wait to acquire exclusive access, in milliseconds
 * \return DLSDK_SUCCESS or reason for failure
 */
DLSDK_EXPORT dlsdk_status dlsdk_dpaux_begin_exclusive_access(dlsdk_dpaux_handle dpaux,
                                                             uint32_t timeoutMs);

/**
 * Ends exclusive access to the DP AUX channel.
 * \param[in] dpaux DP AUX channel handle. Must be valid and non-NULL
 * \return DLSDK_SUCCESS or reason for failure
 */
DLSDK_EXPORT dlsdk_status dlsdk_dpaux_end_exclusive_access(dlsdk_dpaux_handle dpaux);

/**
 * \brief Function used to detect whether a monitor is connected to the DP AUX channel and force
 *        link training.
 * \param[in] dpaux DP AUX channel handle. Must be valid and non-NULL
 * \return DLSDK_SUCCESS if a monitor is detected, DLSDK_UNSUCCESSFUL_NO_MONITOR if no monitor
 *         is detected, or another error code on failure.
 */
DLSDK_EXPORT dlsdk_status dlsdk_dpaux_detect(dlsdk_dpaux_handle dpaux);

/**
 * \brief Function used to enable polling for DP-connected monitors. Useful for detecting hotplug
 *        events on monitors that do not support HPD interrupts.
 * \param[in] dpaux DP AUX channel handle. Must be valid and non-NULL
 * \return DLSDK_SUCCESS or reason for failure. If the device does not support virtual HPD polling,
 *         the function will return DLSDK_UNSUCCESSFUL.
 */
DLSDK_EXPORT dlsdk_status dlsdk_dpaux_enable_virtual_hpd_polling(dlsdk_dpaux_handle dpaux);

/**
 * \brief Function used to disable polling for DP-connected monitors.
 * \param[in] dpaux DP AUX channel handle. Must be valid and non-NULL
 * \return DLSDK_SUCCESS or reason for failure. If the device does not support virtual HPD polling,
 *         the function will return DLSDK_UNSUCCESSFUL.
 */
DLSDK_EXPORT dlsdk_status dlsdk_dpaux_disable_virtual_hpd_polling(dlsdk_dpaux_handle dpaux);
/** @}*/  // End group dpAuxApi

/** \defgroup deviceApi Device
 *  \brief Functions for manipulating/controlling attached devices
 *  @{
 */

/**
 * \brief Function used to obtain the unique identifier of the device.
 *
 * \details Based on the device's USB serial number, so the identifier persists across unplugging
 *          and power cycling. Example: "USB_6015-10096418", where 6015 is the USB product ID
 *          (in hex) and 10096418 is the serial number. The DisplayLink USB vendor ID (0x17e9) is
 *          not included.
 *
 * \param[in] device Device handle. Must be valid and non-NULL
 * \return A string containing the unique device identifier.
 */
DLSDK_EXPORT const char* dlsdk_device_id(dlsdk_device_handle device);

/**
 * \brief Function used to obtain the firmware version of the device.
 *
 * \param[in] device Device handle. Must be valid and non-NULL
 * \return A string containing the firmware version of the device.
 */
DLSDK_EXPORT const char* dlsdk_device_firmware_version(dlsdk_device_handle device);

/**
 * \brief Function used to obtain the attached displays for a given device.
 *
 * \details The caller must allocate enough memory to store the display handles
 *          (dlsdk_display_handle) before calling this method. If there are more attached displays
 *          than the allocated memory can hold, the function returns DLSDK_NOT_ENOUGH_SPACE.
 *          The user must then allocate memory equal to the returned size before calling the
 *          function again.
 *          Display handles are owned by their device and must not be freed by the caller; they
 *          remain valid until the associated device handle is released with dlsdk_teardown().
 * \param[in] device Device handle. Must be valid and non-NULL
 * \param[in, out] displays A pointer to a contiguous memory block large enough to store the
 *                          display handles connected to the device. If NULL, the function returns
 *                          only the number of attached displays in the size parameter.
 * \param[in, out] size [in]: the number of display handles that can be stored in the displays
 *                            parameter.
 *                      [out]: The number of attached displays.
 * \return DLSDK_NOT_ENOUGH_SPACE if there are more attached displays than allocated memory for
 *         storing \p dlsdk_display_handle.
 */
DLSDK_EXPORT dlsdk_status dlsdk_device_get_displays(dlsdk_device_handle device,
                                                    dlsdk_display_handle* displays,
                                                    unsigned int* size);

/**
 * \brief Function used to obtain a handle to the DP AUX channel available on a given device.
 * \details Some devices may not have a DP AUX channel, in which case this function returns NULL.
 *          This function is useful when low-level access to the DP AUX channel is required and the
 *          display is not enumerated automatically. Otherwise, obtain the DP AUX channel handle
 *          through the display object by using dlsdk_display_get_dpaux().
 *
 * \param[in] device Device handle. Must be valid and non-NULL
 * \param[in] output The output index for which to obtain the DP AUX channel handle
 * \return A handle to the DP AUX channel, or NULL if the device does not support the channel, the
 *         output is not a DisplayPort output, or the output does not exist.
 */
DLSDK_EXPORT dlsdk_dpaux_handle dlsdk_device_get_dpaux(dlsdk_device_handle device,
                                                       unsigned int output);

/** \brief Function used to update the firmware of a device
 *  \details This synchronous function updates the firmware on a specific device and blocks until
 *           the process is complete, which may take up to 30 seconds.
 *           It is required when the DisplayLink Direct is initialised with a null firmware path
 *           and can also be used to update firmware on demand.
 *           Keep the firmware up to date to ensure optimal device performance, reliability, and
 *           access to the latest features.
 * After a successful update, the device and display handles remain valid, but the rendering
 * pipeline must be reestablished and the display must be powered on again.
 *  \param[in] device The device handle. Must be valid and non-NULL.
 *  \param[in] firmwarePath The path to the firmware directory provided by DisplayLink. Must be
 *                          valid and non-NULL.
 *  \return DLSDK_SUCCESS on success, otherwise an error code. DLSDK_UNSUCCESSFUL_NO_DEVICE if the
 *          device is not found within 15 seconds after firmware update.
 */
DLSDK_EXPORT dlsdk_status dlsdk_device_update_firmware(dlsdk_device_handle device,
                                                       const char* firmwarePath);
/** @}*/  // End group deviceApi

/** \defgroup  systemApi System-level
 *  \brief Functions not related to a particular device or display
 *  @{
 */

/**
 * \brief Function used to initialise the internals of the DisplayLink Direct
 *
 * \details This method must be called by the user prior to invoking any other functionality.
 * \return DLSDK_SUCCESS on success, otherwise an error code.
 */
DLSDK_EXPORT dlsdk_status dlsdk_initialise(void);

/** \brief Function used to initialise the DisplayLink Direct with a specific configuration
 *  \details This method must be called by the user prior to invoking any other functionality.
 *  \param[in] config The configuration structure containing the firmware path and other settings.
 *                    See dlsdk_config for details.
 *  \return DLSDK_SUCCESS on success, otherwise an error code.
 */
DLSDK_EXPORT dlsdk_status dlsdk_initialise_with_config(const dlsdk_config* config);

/**
 * \brief Function used to clean up the DisplayLink Direct internals when a user no longer
 *        requires its use
 *
 * \details This method must be called by the user prior the application exiting.
 */
DLSDK_EXPORT void dlsdk_teardown(void);

/**
 * \brief Function used to obtain the version of the DisplayLink Direct.
 *
 * \return A string containing the DisplayLink Direct version in Python PEP 440 format.
 */
DLSDK_EXPORT const char* dlsdk_version(void);

/**
 * \brief Function used to obtain the attached devices.
 *
 * \details The caller must allocate enough memory to store the device handles (dlsdk_device_handle)
 *          before calling this method.
 *          If there are more attached devices than the allocated memory can hold, the function
 *          returns DLSDK_NOT_ENOUGH_SPACE. The user must then allocate memory equal to the returned
 *          size before calling the function again.
 *
 * \param[in, out] devices A pointer to a contiguous memory block large enough to store the
 *                         connected device handles. If NULL is passed, the function returns only
 *                         the number of connected devices in the size parameter.
 * \param[in, out] size [in]: the number of device handles that can be stored in \p devices
 *                      [out]: The number of attached devices.
 * \return DLSDK_NOT_ENOUGH_SPACE if there are more attached devices than allocated memory for
 *         storing \p dlsdk_device_handle.
 */
DLSDK_EXPORT dlsdk_status dlsdk_get_devices(dlsdk_device_handle* devices, unsigned int* size);

/**
 * \brief Function used to restart a device.
 *
 * \details On calling this function, the device will be restarted.
 *          The device and display handles remain valid after restart, but the rendering pipeline
 *          must be reestablished and any displays must be powered on again.
 *          In embedded mode, the device and displays will no longer be accessible through their
 *          handles and the user must reinitialise the system to obtain new handles.
 *
 * \param[in] device Device handle. Must be valid and non-NULL
 */
DLSDK_EXPORT dlsdk_status dlsdk_restart_device(dlsdk_device_handle device);

/**
 * \brief Registers a hotplug callback function.
 *
 * \details This function registers a callback to be invoked when a hotplug event occurs and
 *          returns a registration handle for later unregistration.
 *          Only one callback can be registered, and registering a new callback will overwrite the
 *          previous one.
 *          The callback will be invoked for all devices and displays currently connected and for
 *          all subsequent hotplug events from the moment the callback is registered.
 *          All callbacks are invoked on an internal DisplayLink Direct worker thread.
 *          When a system is destroyed, all hotplug callbacks will be automatically unregistered and
 *          their handles invalidated.
 *
 * \param[in]  cb         the function to be invoked when a hotplug event occurs. Must be non-NULL.
 * \param[in]  user_data  user data to pass to the callback function. Can be NULL if not needed.
 * \param[out] outHandle  Callback registration handle. Must be non-NULL.
 *                        On success, receives a non-NULL handle.
 * \returns DLSDK_SUCCESS on success, otherwise an error code.
 */
DLSDK_EXPORT dlsdk_status dlsdk_register_hotplug_callback(dlsdk_hotplug_callback_fn cb,
                                                          void* user_data,
                                                          dlsdk_hotplug_callback_handle* outHandle);

/**
 * \brief Unregisters a hotplug callback.
 *
 * \details You must not register or unregister callbacks from within a callback.
 *          The callback will not be invoked after this function returns successfully. If the
 *          callback is currently running, this function blocks until it completes before
 *          unregistering and returning.
 *
 * \param[in] handle  The callback registration handle returned during registration.
 * \returns DLSDK_SUCCESS on success, DLSDK_INVALID_HANDLE if \p handle is unknown or already
 *          unregistered, otherwise an error code.
 */
DLSDK_EXPORT dlsdk_status dlsdk_unregister_hotplug_callback(dlsdk_hotplug_callback_handle handle);
/** @}*/  // End group systemApi
/** @}*/  // End group apiMethods
/** @}*/  // End group dlSdkCApi

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace dl { namespace sdk {
/** \defgroup  cpptagWrapper DisplayLink Direct c++ Wrapper
 *  \brief C++ Helper classes built around the c-API
 *  \details Implementation to be used as a reference.
 *  \note Synaptics provides this example code "as is" and is not responsible for any problems,
 *        damages, or losses that may result from using it.
 *  @{
 */
using Rect = dlsdk_rect;

struct Config final
{
  Config() = default;
  Config(const char* firmwarePath, uint32_t flags = 0)
    : flags(flags)
  {
    if (firmwarePath)
      this->firmwarePath = std::string(firmwarePath);
  }

  std::string firmwarePath;
  uint32_t flags = 0;
};


/** \defgroup cpptagDpAuxApi DP AUX Helper Class
 *  \brief Class methods for the DP AUX channel
 *  @{
 */
class DpAux final
{
public:
  // Methods to read and write data over the AUX channel
  explicit DpAux(dlsdk_dpaux_handle dpaux)
    : m_dpaux(dpaux)
  {
  }

  //! For more information, please refer to dlsdk_dpaux_read()
  dlsdk_aux_status auxReadData(uint32_t auxRegAddress, uint8_t* data, uint8_t length)
  {
    return dlsdk_dpaux_read(m_dpaux, auxRegAddress, data, length);
  }

  //! For more information, please refer to dlsdk_dpaux_write()
  dlsdk_aux_status auxWriteData(uint32_t auxRegAddress, const uint8_t* data, uint8_t length)
  {
    return dlsdk_dpaux_write(m_dpaux, auxRegAddress, data, length);
  }

  //! For more information, please refer to dlsdk_dpaux_i2c_read()
  dlsdk_aux_status auxI2cReadData(uint8_t deviceAddress,
                                  uint8_t* data,
                                  uint8_t length,
                                  bool mot,
                                  std::chrono::milliseconds timeout)
  {
    return dlsdk_dpaux_i2c_read(m_dpaux, deviceAddress, data, length, mot, timeout.count());
  }

  //! For more information, please refer to dlsdk_dpaux_i2c_write()
  dlsdk_aux_status auxI2cWriteData(uint8_t deviceAddress,
                                   const uint8_t* data,
                                   uint8_t length,
                                   bool mot,
                                   std::chrono::milliseconds timeout)
  {
    return dlsdk_dpaux_i2c_write(m_dpaux, deviceAddress, data, length, mot, timeout.count());
  }

  //! For more information, please refer to dlsdk_dpaux_i2c_write_and_read()
  dlsdk_aux_status auxI2cWriteAndReadData(uint8_t deviceAddress,
                                          const uint8_t* writeData,
                                          uint8_t writeLength,
                                          uint8_t* readData,
                                          uint8_t readLength,
                                          std::chrono::milliseconds timeout)
  {
    return dlsdk_dpaux_i2c_write_and_read(m_dpaux, deviceAddress,
                                          writeData, writeLength,
                                          readData, readLength,
                                          timeout.count());
  }

  //! For more information, please refer to dlsdk_dpaux_begin_exclusive_access()
  dlsdk_status beginExclusiveAccess(std::chrono::milliseconds timeout)
  {
    return dlsdk_dpaux_begin_exclusive_access(m_dpaux, timeout.count());
  }

  //! For more information, please refer to dlsdk_dpaux_end_exclusive_access()
  dlsdk_status endExclusiveAccess()
  {
    return dlsdk_dpaux_end_exclusive_access(m_dpaux);
  }

  dlsdk_status detect()  //! For more information, please refer to dlsdk_dpaux_detect()
  {
    return dlsdk_dpaux_detect(m_dpaux);
  }

  //! For more information, please refer to dlsdk_dpaux_enable_virtual_hpd_polling()
  dlsdk_status enableVirtualHpdPolling()
  {
    return dlsdk_dpaux_enable_virtual_hpd_polling(m_dpaux);
  }

  //! For more information, please refer to dlsdk_dpaux_disable_virtual_hpd_polling()
  dlsdk_status disableVirtualHpdPolling()
  {
    return dlsdk_dpaux_disable_virtual_hpd_polling(m_dpaux);
  }

private:
  dlsdk_dpaux_handle m_dpaux = nullptr;
};

class DpAuxExclusiveAccess final
{
public:
  explicit DpAuxExclusiveAccess(DpAux* dpaux, std::chrono::milliseconds timeout)
    : m_dpaux(dpaux)
    , m_status(dpaux ? dpaux->beginExclusiveAccess(timeout) : DLSDK_INVALID_HANDLE)
  {
  }

  ~DpAuxExclusiveAccess()
  {
    if (m_dpaux && m_status == DLSDK_SUCCESS) {
      m_dpaux->endExclusiveAccess();
    }
  }

  DpAuxExclusiveAccess(const DpAuxExclusiveAccess&) = delete;
  DpAuxExclusiveAccess& operator=(const DpAuxExclusiveAccess&) = delete;

  dlsdk_status status() const
  {
    return m_status;
  }

private:
  DpAux* const m_dpaux;
  const dlsdk_status m_status;
};
  /** @}*/  // End group cpptagDpAuxApi

/** \defgroup cpptagDisplayApi Display Helper Class
 *  \brief Class methods for manipulating/controlling displays attached to a device
 *  @{
 */
struct DisplayHandle final
{
  //! For more information, please refer to dlsdk_display_id()
  std::string id() const
  {
    return dlsdk_display_id(m_display);
  }

  //! For more information, please refer to dlsdk_display_size()
  Rect size() const
  {
    return dlsdk_display_size(m_display);
  }

  //! For more information, please refer to dlsdk_display_edid()
  dlsdk_status edid(uint8_t* data, uint32_t* len) const
  {
    return dlsdk_display_edid(m_display, data, len);
  }

  //! For more information, please refer to dlsdk_display_get_capabilities()
  dlsdk_status getCapabilities(dlsdk_display_capabilities* capabilities) const
  {
    return dlsdk_display_get_capabilities(m_display, capabilities);
  }

  //! For more information, please refer to dlsdk_display_modes()
  dlsdk_status modes(dlsdk_display_mode* modes, unsigned int* count) const
  {
    return dlsdk_display_modes(m_display, modes, count);
  }

  //! For more information, please refer to dlsdk_display_preferred_mode()
  dlsdk_status preferredMode(dlsdk_display_mode* mode) const
  {
    return dlsdk_display_preferred_mode(m_display, mode);
  }

  //! For more information, please refer to dlsdk_display_power_on()
  dlsdk_status powerOn()
  {
    return dlsdk_display_power_on(m_display);
  }

  //! For more information, please refer to dlsdk_display_power_on_with_options()
  dlsdk_status powerOn(const dlsdk_display_power_on_options& options)
  {
    return dlsdk_display_power_on_with_options(m_display, &options);
  }

  //! For more information, please refer to dlsdk_display_power_off()
  dlsdk_status powerOff()
  {
    return dlsdk_display_power_off(m_display);
  }

  //! For more information, please refer to dlsdk_display_show()
  dlsdk_status show(const dlsdk_frame& frame)
  {
    return dlsdk_display_show(m_display, &frame);
  }

  //! For more information, please refer to dlsdk_display_wait_on_show()
  dlsdk_status waitOnShow(std::chrono::milliseconds timeout = std::chrono::milliseconds::max())
  {
    if (timeout == std::chrono::milliseconds::max()) {
      return dlsdk_display_wait_on_show(m_display);
    } else {
      return dlsdk_display_wait_on_show_for(m_display, static_cast<uint32_t>(timeout.count()));
    }
  }

  //! For more information, please refer to dlsdk_display_clear()
  dlsdk_status clear()
  {
    return dlsdk_display_clear(m_display);
  }

  //! For more information, please refer to dlsdk_display_get_dpaux()
  DpAux getDpAux()
  {
    return DpAux(dlsdk_display_get_dpaux(m_display));
  }

  //! For more information, please refer to dlsdk_display_get_brightness()
  dlsdk_status getBrightness(uint8_t* brightness)
  {
    return dlsdk_display_get_brightness(m_display, brightness);
  }

  //! For more information, please refer to dlsdk_display_set_brightness()
  dlsdk_status setBrightness(uint8_t brightness)
  {
    return dlsdk_display_set_brightness(m_display, brightness);
  }

private:
  dlsdk_display_handle m_display = nullptr;
};
/** @}*/  // End group cpptagDisplayApi

/** \defgroup cpptagDeviceApi Device Helper Class
 *  \brief Class methods for manipulating/controlling attached displays
 *  @{
 */
struct DeviceHandle final
{
  //! For more information, please refer to dlsdk_restart_device()
  dlsdk_status restart()
  {
    return dlsdk_restart_device(m_device);
  }

  //! For more information, please refer to dlsdk_device_id()
  std::string id() const
  {
    return dlsdk_device_id(m_device);
  }

  //! For more information, please refer to dlsdk_device_firmware_version()
  std::string firmwareVersion() const
  {
    return dlsdk_device_firmware_version(m_device);
  }

  //! For more information, please refer to dlsdk_device_get_displays()
  std::vector<DisplayHandle> getDisplays()
  {
    unsigned int count = 1;
    std::vector<DisplayHandle> displays(count);
    static_assert(sizeof(dlsdk_display_handle) == sizeof(DisplayHandle), "");
    static_assert(alignof(dlsdk_display_handle) == alignof(DisplayHandle), "");
    dlsdk_status status = DLSDK_UNSUCCESSFUL;
    while (status != DLSDK_SUCCESS) {
      status = dlsdk_device_get_displays(m_device,
                                         reinterpret_cast<dlsdk_display_handle*>(displays.data()),
                                         &count);
      if (status != DLSDK_NOT_ENOUGH_SPACE && status != DLSDK_SUCCESS) {
        displays.clear();
        break;
      } else if (status == DLSDK_NOT_ENOUGH_SPACE) {
        count++;
      }
      displays.resize(count);
    }
    return displays;
  }

  //! For more information, please refer to dlsdk_device_get_dpaux()
  DpAux getDpAux(unsigned int output)
  {
    return DpAux(dlsdk_device_get_dpaux(m_device, output));
  }

  //! For more information, please refer to dlsdk_device_update_firmware()
  dlsdk_status updateFirmware(const char* firmwarePath)
  {
    return dlsdk_device_update_firmware(m_device, firmwarePath);
  }

private:
  dlsdk_device_handle m_device = nullptr;
};
/** @}*/  // End group cpptagDeviceApi

/** \defgroup cpptagSystemApi System Helper Class
 *  \brief Class methods not related to a particular device or display
 *  @{
 */

struct HotplugEventData
{
  uint32_t version;          /**< See dlsdk_hotplug_event_data.version. */
  dlsdk_hotplug_event event; /**< See dlsdk_hotplug_event_data.event. */
  DeviceHandle device;       /**< See dlsdk_hotplug_event_data.device. */
  std::string device_id;     /**< See dlsdk_hotplug_event_data.device_id. */
  DisplayHandle display;     /**< See dlsdk_hotplug_event_data.display. */
  std::string display_id;    /**< See dlsdk_hotplug_event_data.display_id. */
};

using CallbackHandle = dlsdk_hotplug_callback_handle;
using CallbackFn = std::function<void(const HotplugEventData&)>;

struct System final
{
  //! For more information, please refer to dlsdk_initialise()
  System()
  {
    m_initialisedStatus = dlsdk_initialise();
  }

  //! For more information, please refer to dlsdk_initialise_with_config()
  explicit System(const Config& config)
  {
    dlsdk_config cConfig;
    DLSDK_CONFIG_INIT(&cConfig);
    cConfig.flags = config.flags;
    cConfig.firmwarePath = config.firmwarePath.empty() ? nullptr : config.firmwarePath.c_str();
    m_initialisedStatus = dlsdk_initialise_with_config(&cConfig);
  }

  //! For more information, please refer to dlsdk_teardown()
  ~System()
  {
    dlsdk_teardown();
  }

  dlsdk_status initialised()
  {
    return m_initialisedStatus;
  }

  //! For more information, please refer to dlsdk_get_devices()
  std::vector<DeviceHandle> getDevices()
  {
    unsigned int count = 0;
    std::vector<DeviceHandle> devices;
    static_assert(sizeof(dlsdk_device_handle) == sizeof(DeviceHandle), "");
    static_assert(alignof(dlsdk_device_handle) == alignof(DeviceHandle), "");
    dlsdk_status status = DLSDK_SUCCESS;
    do {
      status = dlsdk_get_devices(nullptr, &count);
      if (count == 0)
        break;
      devices.resize(count);
      status = dlsdk_get_devices(reinterpret_cast<dlsdk_device_handle*>(devices.data()), &count);
    } while (status == DLSDK_NOT_ENOUGH_SPACE);
    devices.resize(count);
    return devices;
  }

  //! For more information, please refer to dlsdk_register_hotplug_callback()
  dlsdk_status registerHotplugCallback(const CallbackFn& cb, CallbackHandle* outHandle)
  {
    static auto cppCallback = [](dlsdk_hotplug_event_data* data, void* userData) {
      HotplugEventData eventData;
      eventData.version = data->version;
      eventData.event = data->event;
      eventData.device = *reinterpret_cast<DeviceHandle*>(&data->device);
      eventData.device_id = data->device_id ? std::string(data->device_id) : std::string();
      eventData.display = *reinterpret_cast<DisplayHandle*>(&data->display);
      eventData.display_id = data->display_id ? std::string(data->display_id) : std::string();

      CallbackContext* context = reinterpret_cast<CallbackContext*>(userData);
      std::lock_guard<std::mutex> lock(context->mutex);
      if (context->userCallback) {
        context->userCallback(eventData);
      }
    };
    {
      std::lock_guard<std::mutex> lock(m_callbackContext.mutex);
      m_callbackContext.userCallback = cb;
    }
    return dlsdk_register_hotplug_callback(cppCallback, &m_callbackContext, outHandle);
  }

  //! For more information, please refer to dlsdk_unregister_hotplug_callback()
  dlsdk_status unregisterHotplugCallback(CallbackHandle handle)
  {
    const dlsdk_status status = dlsdk_unregister_hotplug_callback(handle);
    if (status == DLSDK_SUCCESS) {
      std::lock_guard<std::mutex> lock(m_callbackContext.mutex);
      m_callbackContext.userCallback = nullptr;
    }
    return status;
  }

private:
  struct CallbackContext
  {
    std::mutex mutex;
    CallbackFn userCallback;
  };

  CallbackContext m_callbackContext;
  dlsdk_status m_initialisedStatus;
};
/** @}*/  // End group cpptagSystemApi
}}
/** @}*/  // End group cpptagWrapper
#endif  // __cplusplus
