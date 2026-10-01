# Why DisplayLink Direct?

Driving external displays traditionally relies on native video outputs (HDMI,
DisplayPort, USB-C Alt Mode) and a full graphics-driver and compositor stack.
That approach is limiting on systems with few or no spare video outputs, on
headless or embedded hardware, and anywhere a heavyweight display stack is
impractical. DisplayLink Direct addresses these challenges.

## Not enough native video outputs

Many hosts — laptops, single-board computers, industrial PCs — expose only one
or two native display connectors. DisplayLink Direct lets you drive **multiple
additional displays over a single USB connection**, well beyond what the host's
native outputs allow.

## Heavyweight driver and compositor dependencies

A conventional DisplayLink deployment needs a host graphics driver and
integration with the system compositor. DisplayLink Direct **requires neither**:
your application submits frame buffers straight to each display through a small,
C-compatible API. This removes a major integration burden and eliminates a class
of driver/compositor compatibility problems.

## Embedded and resource-constrained systems

Embedded targets often can't afford a general-purpose display stack or
unpredictable memory behavior. DisplayLink Direct is lightweight and offers an
**embedded mode** that performs no dynamic memory allocation at runtime, giving
predictable, deterministic resource usage suitable for constrained and
safety-conscious environments.

## Bandwidth-limited transports

Sending raw, uncompressed pixels over general-purpose buses is expensive.
DisplayLink **compresses frame deltas on the host** and packetizes them over USB
bulk transfers to a hardware decoder in the dock or dongle, making efficient use
of USB bandwidth instead of demanding a dedicated video link.

## Hosts without a GPU

Because DisplayLink Direct encodes frames on the CPU and sends them to the
device's hardware decoder, it **does not depend on a host GPU**. It can run on
headless servers, GPU-less single-board computers, and minimal embedded systems,
driving real displays from a frame buffer without any graphics accelerator.

## Direct, application-level control

Applications that need to render and route specific content to specific outputs
gain **fine-grained control**: enumerate devices and displays, address each
output by a stable hardware identifier, submit frames in a range of pixel
formats, read display EDIDs, and synchronize output across multiple displays —
all from your own code.
