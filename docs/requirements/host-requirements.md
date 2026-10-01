# Host Requirements

The performance of DisplayLink Direct depends on the processing power available
on the host. More capable systems provide better performance, but the final
experience depends strongly on the specific workload, display topology, content
type, and update rate.

The following recommended specification was tested with these use cases:

- Two 4K displays with a desktop workload.
- Two 4K displays with playback of a single Full HD video.

## Recommended host specification

For the tested use cases above, Synaptics recommends a host with:

- CPU: 2 GHz or faster with support for NEON, SSE2, or AVX.
- RAM: 4 GB or more is recommended for two or more DisplayLink displays.
- USB: USB 3.2 Gen 1 port.

## Minimal host specification

For a basic or reduced-performance experience, the host should meet at least
the following:

- Processor: 1.2 GHz single-core CPU or higher.
- RAM: 1 GB for one DisplayLink display. For two or more DisplayLink displays,
	2 GB or more is recommended.
- USB: USB 2.0 port (USB 3.2 Gen 1 required for embedded mode)

## Notes

- These requirements describe host capability only. Supported operating systems
	and CPU architectures for this release are listed in the Supported Platforms
	section.
- Higher resolutions, multiple displays, video playback, and frequent frame
	updates increase CPU, memory, and USB bandwidth requirements.
