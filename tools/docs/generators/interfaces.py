"""Generated regions read from the headers.

``<!-- generated: implementations -->``  the Base* interfaces (``inc/base/``) and the classes that implement them
                                         on each platform (``inc/mcu/<platform>/``), from the ``class X : public BaseY``
                                         declarations.
``<!-- generated: esp32-classes -->``    each ESP32 class with its interface, header, source and ESP-IDF driver.
``<!-- generated: pure-virtuals -->``    the pure virtual methods of each interface: what a new platform implements.
``<!-- generated: mcu-targets -->``      the ``HF_TARGET_MCU_*`` macros ``inc/utils/McuSelect.h`` accepts, with the
                                         family and name each one selects.
``<!-- generated: quickstart-code -->``  the README copy of the ``quickstart`` section of ``tests/docs/quickstart.cpp``.
"""

from __future__ import annotations

import re

from hardfoc_mkdocs_theme.paths import repo_root

ROOT = repo_root()

# What each interface is for, in a few words (the class names come from the headers).
PURPOSE = {
    "BaseAdc": "Analog inputs: counts and volts, averaging, multi-channel reads",
    "BaseBluetooth": "Bluetooth / BLE: scan and connect",
    "BaseCan": "CAN bus: classic and FD-sized frames, filters, bus status",
    "BaseGpio": "Digital pins: direction, polarity, pulls, interrupts",
    "BaseI2c": "One device on an I2C bus: write, read, write-then-read",
    "BaseLogger": "Tagged, levelled log output",
    "BaseNvs": "Key-value non-volatile storage: integers, strings, blobs",
    "BasePeriodicTimer": "Periodic callback at a microsecond period",
    "BasePio": "Programmable pulse I/O: symbol trains (WS2812, IR)",
    "BasePwm": "PWM channels: frequency, duty, phase",
    "BaseSpi": "One device on an SPI bus: full-duplex transfers",
    "BaseTemperature": "On-chip or external temperature sensor",
    "BaseUart": "Serial port: write, read, flush, printf",
    "BaseUsbSerialJtag": "USB Serial/JTAG console",
    "BaseWifi": "Wi-Fi station and access point",
}
# The ESP-IDF driver each ESP32 class is built on (from the includes of its header and source).
ESP_IDF_DRIVER = {
    "EspAdc": "`esp_adc` (one-shot and continuous, calibration)",
    "EspBluetooth": "NimBLE host",
    "EspCan": "TWAI node API (`esp_twai`)",
    "EspGpio": "`driver/gpio`, RTC I/O, glitch filters",
    "EspI2cDevice": "`driver/i2c_master`",
    "EspLogger": "`esp_log`",
    "EspNvs": "`nvs_flash`",
    "EspPeriodicTimer": "`esp_timer`",
    "EspPio": "RMT (`driver/rmt_tx`, `driver/rmt_rx`)",
    "EspPwm": "LEDC (`driver/ledc`)",
    "EspSpiDevice": "`driver/spi_master`",
    "EspTemperature": "`driver/temperature_sensor`",
    "EspUart": "`driver/uart`",
    "EspUsbSerialJtag": "`driver/usb_serial_jtag`",
    "EspWifi": "`esp_wifi`",
}
PLATFORMS = [("esp32", "ESP32"), ("stm32", "STM32"), ("host", "Host")]
IMPL = re.compile(r"^class (\w+)(?: final)? : public (Base\w+)", re.M)


def _implementations():
    out: dict[str, dict[str, list[tuple[str, str]]]] = {}
    for folder, _ in PLATFORMS:
        for h in sorted((ROOT / "inc/mcu" / folder).glob("*.h")):
            for cls, base in IMPL.findall(h.read_text()):
                if cls.endswith("Stub"):
                    continue
                out.setdefault(base, {}).setdefault(folder, []).append((cls, h.relative_to(ROOT).as_posix()))
    return out


def implementations(page) -> str:
    impls = _implementations()
    bases = sorted(p.stem for p in (ROOT / "inc/base").glob("Base*.h"))
    head = "| Interface | For | " + " | ".join(label for _, label in PLATFORMS) + " |"
    rows = [head, "| --- | --- |" + " --- |" * len(PLATFORMS)]
    for b in bases:
        cells = []
        for folder, _ in PLATFORMS:
            items = impls.get(b, {}).get(folder, [])
            cells.append(", ".join(f"[`{c}`]({page.rel(path)})" for c, path in items) or "-")
        link = f"[`{b}`]({page.rel(f'inc/base/{b}.h')})"
        rows.append(f"| {link} | {PURPOSE.get(b, '')} | " + " | ".join(cells) + " |")
    return "\n".join(rows) + "\n"


def esp32_classes(page) -> str:
    rows = ["| Class | Implements | ESP-IDF driver | Header | Source |", "| --- | --- | --- | --- | --- |"]
    for h in sorted((ROOT / "inc/mcu/esp32").glob("*.h")):
        for cls, base in IMPL.findall(h.read_text()):
            if cls.endswith("Stub"):
                continue
            hp = h.relative_to(ROOT).as_posix()
            src = f"src/mcu/esp32/{h.stem}.cpp"
            src_cell = f"[`{h.stem}.cpp`]({page.rel(src)})" if (ROOT / src).is_file() else "-"
            rows.append(f"| `{cls}` | `{base}` | {ESP_IDF_DRIVER.get(cls, '')} | [`{h.name}`]({page.rel(hp)}) | {src_cell} |")
    return "\n".join(rows) + "\n"


PURE = re.compile(r"virtual\s+[^;{}]*?\b(\w+)\s*\([^;{}]*?\)\s*(?:const\s*)?(?:noexcept\s*)?=\s*0\s*;", re.S)


def pure_virtuals(page) -> str:
    rows = ["| Interface | Methods a port must implement |", "| --- | --- |"]
    for h in sorted((ROOT / "inc/base").glob("Base*.h")):
        names = list(dict.fromkeys(PURE.findall(h.read_text())))
        cells = ", ".join(f"`{n}()`" for n in names) or "-"
        rows.append(f"| [`{h.stem}`]({page.rel(h.relative_to(ROOT).as_posix())}) ({len(names)}) | {cells} |")
    return "\n".join(rows) + "\n"


def mcu_targets() -> str:
    text = (ROOT / "inc/utils/McuSelect.h").read_text()
    starts = list(re.finditer(r"^#(?:ifdef |elif defined\()(HF_TARGET_MCU_\w+)\)?$", text, re.M))
    rows = ["| Macro | Family | `HF_MCU_NAME` |", "| --- | --- | --- |"]
    for i, m in enumerate(starts):
        body = text[m.end() : starts[i + 1].start() if i + 1 < len(starts) else len(text)]
        fam = re.search(r"#define (HF_MCU_FAMILY_\w+)", body)
        name = re.search(r'#define HF_MCU_NAME "([^"]+)"', body)
        if fam and name:
            rows.append(f"| `{m.group(1)}` | `{fam.group(1)}` | {name.group(1)} |")
    return "\n".join(rows) + "\n"


def _section(path: str, name: str) -> str:
    text = (ROOT / path).read_text()
    m = re.search(rf"--8<-- \[start:{name}\]\n(.*?)\n[^\n]*--8<-- \[end:{name}\]", text, re.S)
    if not m:
        raise SystemExit(f"[gen] no section {name!r} in {path}")
    return m.group(1)


def quickstart_code() -> str:
    return "```cpp\n" + _section("tests/docs/quickstart.cpp", "quickstart") + "\n```\n"


REGIONS = {"implementations": implementations, "esp32-classes": esp32_classes, "pure-virtuals": pure_virtuals, "mcu-targets": mcu_targets, "quickstart-code": quickstart_code}
