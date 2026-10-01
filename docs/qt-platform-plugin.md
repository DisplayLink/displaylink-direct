# Qt Platform Plugin

The `qdlsdk` plugin is a [Qt Platform Abstraction (QPA)](https://doc.qt.io/qt-6/qpa.html) plugin that lets an
unmodified Qt GUI application render directly onto DisplayLink Direct displays, without requiring a Linux graphical
environment such as X11, Wayland, GNOME, or any other compositor. Qt applications can run directly from a virtual
terminal (VT) without a GPU. It registers itself under the platform name `dlsdk` and is selected in the same way as any
other QPA plugin, via the `-platform` command-line argument or the `QT_QPA_PLATFORM` environment variable.

!!! note "Distributed separately"
    The Qt platform plugin is built and distributed from its own repository, separate from the core
    DisplayLink Direct SDK described elsewhere in this documentation. It links against the `dlsdk::dlsdk`
    CMake target described in [Running on Linux](../getting-started/installation-linux.md).

<!-- TODO: this points at the pre-release source repo; update to the public repo URL once published -->
Source repository: [{{qt_plugin_repo_name }}]({{ qt_plugin_repo_url }}).

## Input handling

Input handling is Qt's responsibility, not DisplayLink Direct's. The plugin does not read input from
DisplayLink devices or hardware. It simply wires up Qt's existing standard Linux input backends,
the same way other Linux QPA plugins (e.g. `eglfs`, `linuxfb`) do:

- **libinput** is used when Qt was built with libinput support.
- Otherwise, the plugin falls back to **evdev** keyboard, mouse, and touch managers.

## Running an application

Point `QT_PLUGIN_PATH` at the plugin's `plugins` output directory and pass `-platform dlsdk` to your
application:

```bash
QT_PLUGIN_PATH=/path/to/build/src/plugin/plugins ./my-qt-app -platform dlsdk
```

!!! note "USB permissions"
    Accessing DisplayLink USB devices requires the same permissions described in
    [USB permissions](../getting-started/installation-linux.md#usb-permissions). Without them, the plugin
    may need to be run with `sudo`, or the user may need to be added to a group with access to
    `/dev/bus/usb/*`.
