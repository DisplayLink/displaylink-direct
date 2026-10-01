import time

import numpy as np
import dlsdk

dlsdk.create_system()

devices = dlsdk.get_devices()
assert len(devices) == 1  # exactly one device connected
device = devices[0]

displays = device.get_displays()
assert len(displays) == 1  # exactly one display connected
display = displays[0]
display.power_on()

size = display.size()
frame = np.empty((size.height, size.width, 4), dtype=np.uint8)
frame[:, :, 0] = 0xC6  # B
frame[:, :, 1] = 0x81  # G
frame[:, :, 2] = 0x00  # R
frame[:, :, 3] = 0xFF  # X (unused), DLSDK_PIXEL_FORMAT_XRGB byte order is B,G,R,X

display.show(dlsdk.DLSDK_PIXEL_FORMAT_XRGB, frame)
display.wait_on_show()

time.sleep(10)  # keep the frame visible for 10 seconds

dlsdk.delete_system()
