<script>
document.addEventListener('DOMContentLoaded', function() {
  const button = document.getElementById('toggleWheelLinks');
  const container = document.getElementById('wheelLinksContainer');

  button.addEventListener('click', function() {
    if (container.classList.contains('visible')) {
      container.classList.remove('visible');
      button.textContent = "Show Python wheels";
    } else {
      container.classList.add('visible');
      button.textContent = "Hide Python wheels";
    }
  });
});
</script>

# Python Bindings

The Python bindings let you integrate DisplayLink Direct into your
Python applications. Pre-built wheels (`.whl`) for supported Python versions and
architectures are published as [GitHub Release Assets]({{ config.repo_url }}/releases/).

## Prerequisites

- Python 3.10 or higher.
- `pip` available in your selected Python environment.
- DisplayLink Direct host runtime dependencies installed on Linux:
  - `libusb >= 1.0.16`
  - `libc6 >= 2.31`

## Install from GitHub Releases

The wheels are published as assets on the GitHub release page.


Use `pip`'s `--find-links` flag pointing to this page.
`pip` selects the wheel matching your Python version, architecture, and desired
package version:

```bash
pip install --find-links \
    '{{ config.site_url }}pip-index' 'dlsdk'
```

Use the `--no-index` flag to prefer wheels from `--find-links` over wheels
published on PyPI.

Alternatively, install a specific wheel asset directly from a release tag by URL:

```bash
pip install --no-index \
   '{{ config.repo_url }}/releases/download/v{{ latest_version }}/dlsdk-{{ full_latest_version }}-cp312-cp312-manylinux_2_31_x86_64.whl'
```

Replace the Python tag (`cp312`) and platform tag (`manylinux_2_31_x86_64`)
with values that match your interpreter and architecture.

<a href="../../pip-index/" hidden aria-hidden="true" tabindex="-1"></a>

## Verify installation

```bash
python -c "import dlsdk; help(dlsdk)"
```

## Additional references

Use the Linux installation page for host setup details, including USB
permissions and `displaylink-driver` service configuration.
