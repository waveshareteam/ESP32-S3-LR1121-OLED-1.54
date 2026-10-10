"""Manual hardware test; requires a board and a writable SD card."""
import pytest
from pytest_embedded_idf.utils import idf_parametrize

@pytest.mark.sdcard_sdmode
@idf_parametrize("target", ["esp32s3"], indirect=["target"])
def test_examples_sd_card_sdmmc(dut):
    dut.expect_exact("File written", timeout=60)
    dut.expect_exact("Reading file /sdcard/foo.txt", timeout=30)
    dut.expect_exact("Reading file /sdcard/nihao.txt", timeout=30)
    dut.expect_exact("Card unmounted", timeout=30)
