# Firmware

`12_factory_demo/` contains first-party source firmware and is included in the ESP-IDF source matrix. Its application differs from the numbered walkie-talkie example, while both share the product's Board Manager component.

The top-level `.bin` is the original factory/recovery image. It is preserved separately, excluded from build discovery and never relabeled as a fresh source-built artifact. Its exact source revision and reproducible build instructions are not established here and may be documented later.

For verified source-build artifact packaging, see [releases](../releases/README.md).
