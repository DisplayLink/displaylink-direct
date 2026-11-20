# Yocto Project Layer

Yocto Project, available at [yoctoproject.org](https://www.yoctoproject.org/), is a build system used to create custom Linux distributions for embedded devices. It is commonly used when you need a controlled, reproducible image with only the components required for a specific product.

The DisplayLink Direct layer packages the prebuilt DisplayLink Direct runtime and development files, Python bindings, a Qt 6 platform plugin, and sample applications in C, C++, and Python. It also includes a complete demo image for the Arduino UNO Q.

The provided KAS configurations cover recipe builds for `x86_64` on Yocto Scarthgap and Wrynose, plus a Wrynose `aarch64` demo image for the UNO Q. The QEMU configurations check recipe and package builds but do not produce a bootable image. Build the UNO Q image with `kas-container build kas/uno-q/wrynose.yml` from the repository root.

See the repository's README for more detailed information.

Source repository: [{{ yocto_layer_repo_name }}]({{ yocto_layer_repo_url }}).
