# Contributing to DisplayLink Direct

Thanks for your interest in DisplayLink Direct!

This repository is the public hub for the SDK — its documentation, header
files, language bindings, and samples.

## License

- The **documentation, samples, and bindings** in this repository are covered by
  the terms in [LICENSE](LICENSE).
- The **core libraries are proprietary and closed source**. They are distributed
  as pre-built binaries via GitHub Release Assets and are **not** part of this
  repository's source. Third-party components are listed in
  [NOTICE](NOTICE).

By contributing, you agree that your contributions are licensed under the same
terms as the content you are modifying.

## What contributions are welcome

We welcome:

- **Documentation improvements** — fixes, clarifications, and additions to the
  content under [docs/](docs/) that is hosted at
  <https://displaylink.github.io/displaylink-direct/>.
- **Sample code** — improvements or new examples under [samples/](samples/).
- **Suggestions and feedback** — share ideas for new features, better APIs, or
  clearer documentation through
  [GitHub Issues](https://github.com/displaylink/displaylink-direct/issues).

Please note: because the core libraries are proprietary, we **cannot accept code
changes to the closed-source core**. Bindings and headers track the shipped
binaries, so changes there are reviewed on a case-by-case basis.

## How to contribute

1. **Open an issue** to discuss significant changes before starting work.
2. **Fork** the repository and create a branch for your change.
3. Make your edits and, for documentation changes, preview them locally:
   ```bash
   ./docs/serve.sh
   ```
4. **Open a pull request** with a clear description of what the change does and
   why it is needed.

Pull requests for the hosted documentation and samples are especially welcome!

## Reporting bugs and requesting features

Use [GitHub Issues](https://github.com/displaylink/displaylink-direct/issues) and
include as much detail as possible: the platform, device, SDK version, and steps
to reproduce. See the
[Troubleshooting guide](https://displaylink.github.io/displaylink-direct/support/faq) for more information.
