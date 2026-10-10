"""Manual product hardware test; requires pytest-embedded and a connected board."""
import pytest
from pytest_embedded_idf.utils import idf_parametrize

@pytest.mark.generic
@idf_parametrize("target", ["esp32s3"], indirect=["target"])
def test_i2ctools_example(dut):
    dut.expect_exact("i2c-tools>", timeout=20)
    dut.write("i2cdetect")
    dut.expect_exact("3c", timeout=20)
    dut.expect_exact("51", timeout=20)
