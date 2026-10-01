# Applications

DisplayLink Direct is a lightweight, driver-free way to drive one or many
displays over USB — without a host GPU, graphics driver, or compositor. That
makes it a good fit for a wide range of applications, from embedded products to
large multi-display installations.

## Digital signage

Deliver content to information boards, menu displays, and advertising screens.
DisplayLink Direct can run on small, GPU-less hosts and drive multiple screens
from a single device, keeping hardware and power requirements low.

## Video walls

Span a single image or video across a grid of displays. The SDK lets you address
each display by a stable hardware identifier, submit the right region of the
frame to each output, and synchronize presentation across displays for a seamless
wall. See the [Video Wall sample](code-samples/video-wall.md).

## Embedded and industrial systems

Add display output to embedded products, HMIs, and industrial controllers.
**Embedded mode** performs no dynamic memory allocation at runtime, giving
predictable, deterministic resource usage suitable for constrained and
long-running deployments.

## Headless and GPU-less hosts

Because frames are encoded on the CPU and decoded by the DisplayLink device,
DisplayLink Direct needs no host GPU. It can run on headless servers, single-board
computers, and minimal systems that have no graphics accelerator at all.

## Kiosks and point-of-sale

Power customer-facing and operator displays for kiosks, ticketing machines, and
point-of-sale terminals — often driving a primary and secondary screen from one
compact host.

## Multi-monitor productivity

Extend a workstation beyond the number of displays its native video outputs
support, driving additional monitors over standard USB connections.

## Remotely controlled displays

Drive displays from a headless or remotely managed host, where the machine
generating the content is administered over the network rather than operated
locally. Because DisplayLink Direct submits frames programmatically and needs no
local GPU or graphics session, it suits remote and centrally managed
deployments — updating signage, dashboards, or status screens from a remote
controller.

## Custom rendering pipelines

Applications that generate frames themselves — dashboards, media players, test
and measurement tools — can submit frame buffers directly to displays in a range
of pixel formats, with full application-level control over what appears on each
output.

