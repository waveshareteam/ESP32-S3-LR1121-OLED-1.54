# Source firmware artifacts and Releases

Branch and pull-request builds upload successful source builds to the Actions run's **Artifacts** section. Git mirroring copies branches and tags; it does not create GitHub Releases or their attachments by itself. The original factory binary is excluded from source-build packages.

The **Firmware Release** workflow starts on a stable `vMAJOR.MINOR.PATCH` tag. It verifies that the tag and checked-out commit agree and that the commit is reachable from the default branch. It calls the same static, ESP-IDF and Arduino workflows used for normal CI. All jobs must succeed; failed or skipped prerequisites cannot publish a partial Release.

The complete matrix currently contains **47 archives**: 12 IDF projects × two stable framework versions, 12 builds on the pinned ESP-IDF 6.2 development revision, and 11 Arduino sketches. Development archives are explicitly labeled and retain their framework commit. The release validator rejects missing, duplicate, unexpected or stale-commit archives. After validation, publication creates or updates a draft, verifies every uploaded asset's name, size and SHA-256 digest, then publishes. An existing public Release cannot be overwritten; fixes require a new version. One public asset is downloaded again and independently validated after publication.

The product profile is available in online board pack 0.1.2. Static checks fetch and verify the latest stable pack, and IDF builds resolve the wildcard managed dependency. IDF 10 applies its 8 kHz, two-slot, slot-0 speech format through Board Manager initialization hooks. All required checks and all 47 firmware archives must pass before a stable tag can publish a Release; see [Board Manager setup](../docs/board-manager.md).

## Maintainer procedure

1. Resolve the board-profile integration, review the source and obtain successful checks and builds for the exact commit.
2. Choose an unused stable version and create an annotated tag on that reviewed commit.
3. Push that tag to the primary GitLab repository and ensure the mirror includes tags.
4. Verify the identical tag reaches GitHub, then check the **Firmware Release** workflow and its published assets.

Pushing `main` alone produces CI artifacts. Adding the workflow alone does not publish a Release. No tag is created automatically by repository scripts.

## Local packaging

Ubuntu Bash, after a successful default-directory build:

```bash
python3 releases/package_firmware.py --framework esp-idf --selector 08_oled_ssd1309 --version v5.5.5
python3 releases/package_firmware.py --framework arduino --selector 08_oled_ssd1309 --version 3.3.12
```

Generated ZIPs stay in ignored `releases/dist/`. Download Actions artifacts into `releases/downloads/`, for example with `gh run download RUN_ID --dir releases/downloads`. To check a complete set, use `python3 releases/release_firmware.py validate --sha FULL_SOURCE_COMMIT`.

Schema-2 packages contain `manifest.json`, `SHA256SUMS`, `README.md`, `flash_args.txt`, `flash.sh`, `flash.bat`, and `bin/`. Filenames and manifests record the source commit, example, target and framework version. Timestamps use `SOURCE_DATE_EPOCH` or the source commit timestamp. ESP-IDF offsets come from `flasher_args.json`; original segments are retained and the combined image ends at the last segment. Arduino keeps its canonical merged export once, including boot application data and any core-generated flash padding, when a complete separate component set is unavailable. Missing, empty, overlapping or wrong-target firmware fails packaging.

Extract a package and install esptool 5.x. Ubuntu Bash: `sh flash.sh PORT`. Windows PowerShell: `.\flash.bat COM_PORT`. Both helpers flash the combined image at offset zero and preserve the serial-port argument. Close serial monitors first. Flashing replaces the selected device's firmware.

Artifacts from the initial CI run use the previous schema with `flash_args` and remain downloadable from that run. The Release workflow accepts only schema 2 generated from its own tagged source. Build and archive checks do not establish hardware correctness.
