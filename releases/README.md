# Source firmware artifacts

The build workflows package successful first-party builds as downloadable CI artifacts. They do not publish Releases and do not package the original factory binary. Generated ZIPs are ignored under `releases/dist/`.

Ubuntu Bash, after a successful default-directory build:

```bash
python3 releases/package_firmware.py --framework esp-idf --selector 08_oled_ssd1309 --version v5.5.5
python3 releases/package_firmware.py --framework arduino --selector 08_oled_ssd1309 --version 3.3.12
```

Each ZIP contains `manifest.json`, `flash_args`, `flash.sh`, `flash.bat`, and `bin/`. The manifest records source path, version, target, Git SHA, timestamp, baud and binary hashes. ESP-IDF 6.2 development packages also record the full framework commit as `framework_ref`; their version label includes the short framework SHA. IDF offsets come from `flasher_args.json`; Arduino uses the pinned core's self-contained merged export at offset zero. Missing/empty exports fail explicitly. Download artifacts from the workflow run's Artifacts section, or use `gh run download RUN_ID --dir releases/downloads`.

Extract the archive and install a compatible esptool (5.x or later). Ubuntu Bash: `sh flash.sh PORT`. Windows PowerShell: `python -m esptool --chip esp32s3 --port COM_PORT --baud 460800 write-flash '@flash_args'`. Close other serial-port applications first. Flashing replaces the selected device's firmware and requires a deliberate user action.

Packaging tests use synthetic exports only; no real source artifact was produced during the static-only normalization.
