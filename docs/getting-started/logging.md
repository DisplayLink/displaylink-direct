# Logging

## Logging on Linux

DisplayLink Direct writes log messages to `/var/log/displaylink/dlsdk.log`.
The `/var/log/displaylink` directory must exist before the library is used. It
is not created automatically.

## Logging on Android

DisplayLink Direct writes log messages to Android's `logcat`. The log entries
are tagged `DisplayLink-Direct`.