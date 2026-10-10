# One-bit SDMMC file I/O

Before the first build, complete the [Board Manager setup](../../../docs/board-manager.md) and select `esp32_s3_lr1121_oled_1_54_xf` with `idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf`.

Board Manager's named `sdcard` device owns slot, pins, clock and mount configuration. Insert a writable test card. The example writes and reads `/sdcard/hello.txt`, renames it to `foo.txt`, writes/reads `nihao.txt`, then unmounts the card. Existing files with these test names can be replaced.

The schematic connects only CLK, CMD and D0; four-bit and UHS options from the generic upstream example do not apply. Formatting is not automatic. An absent or unmountable card produces an error and returns.

See [framework setup](../../../docs/examples.md) and [shared board integration](../../../docs/board-manager.md). No LVGL or Adapter dependency is enabled. The pytest file is a manual test requiring real hardware and a disposable test card; it is not part of static CI.
