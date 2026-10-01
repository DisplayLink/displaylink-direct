# Samples

Sample applications for DisplayLink Direct.

For full guides and API docs, see the MkDocs site:

- Code Samples: https://displaylink.github.io/displaylink-direct/code-samples/
- Getting Started: https://displaylink.github.io/displaylink-direct/getting-started/installation-linux/
- Python bindings: https://displaylink.github.io/displaylink-direct/getting-started/python-bindings/


## Prerequisites (Linux)

Install common build/runtime tools used by the C/C++ samples:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libusb-1.0-0 read-edid
```

For video samples, install one media backend:

```bash
# VLC backend (default)
sudo apt-get install -y libvlc-dev vlc
```

```bash
# GStreamer backend (optional)
sudo apt-get install -y \
   libgstreamer1.0-dev \
   libgstreamer-plugins-base1.0-dev \
   gstreamer1.0-plugins-base \
   gstreamer1.0-plugins-good \
   gstreamer1.0-plugins-bad \
   gstreamer1.0-libav
```

## Build and Run (quick start)

Use the repository root as `CMAKE_PREFIX_PATH` (`../..` from sample subdirectories).

## Other sample apps

In addition to the media and utility samples, this repository also includes minimal sample apps:

- `samples/minimal-c/minimal-example.c`
- `samples/minimal-c++/minimal-example.cpp`
- `samples/minimal-python/minimal-example.py`

Documentation versions of these snippets are available at:

- https://displaylink.github.io/displaylink-direct/api-reference/minimal-example/

### C sample: minimal frame output

```bash
cd samples/minimal-c
cmake -DCMAKE_PREFIX_PATH=../.. -S . -B build
cmake --build build
./build/minimal-example
```

### C++ sample: minimal frame output

```bash
cd samples/minimal-c++
cmake -DCMAKE_PREFIX_PATH=../.. -S . -B build
cmake --build build
./build/minimal-example
```

### Python sample: minimal frame output

```bash
cd samples/minimal-python
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python minimal-example.py
```

### C++ sample: read EDID

```bash
cd samples/dl-get-edid
cmake -DCMAKE_PREFIX_PATH=../.. -S . -B build
cmake --build build
./build/dl-get-edid -q | parse-edid
```

### C++ sample: video clone (VLC backend)

```bash
cd samples/video
cmake -DCMAKE_PREFIX_PATH=../.. -S . -B build
cmake --build build
./build/video-clone /path/to/video.mp4
```

### Python sample: screensaver

```bash
cd samples/screensaver
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python screensaver.py
```

## USB permissions (run without sudo)

To access DisplayLink USB devices as a non-root user:

```bash
sudo groupadd displaylink
sudo usermod -aG displaylink $USER
echo 'SUBSYSTEM=="usb", MODE="0660", GROUP="displaylink"' | sudo tee /etc/udev/rules.d/00-usb-permissions.rules
sudo udevadm control --reload-rules
```

Log out and log back in so group membership takes effect.

## Logs

Encrypted DisplayLink Direct logs are written to `/var/log/displaylink` when that directory exists.
