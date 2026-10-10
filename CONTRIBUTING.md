# Contributing

Keep examples under their framework root with two-digit lower-case snake_case names. Arduino directories and `.ino` files must match. Use the single IDF board component for all hardware definitions; regenerate its data after YAML changes.

Run the static commands in [validation](docs/validation.md). Report which framework versions were actually compiled and which hardware was tested. Preserve third-party licenses. Do not add downloaded libraries, managed components, build directories, credential headers, firmware ZIPs or local machine paths. Factory binaries are separate from source-build artifacts.
