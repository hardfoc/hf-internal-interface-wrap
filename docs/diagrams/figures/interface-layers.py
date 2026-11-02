"""The interface model: code above calls Base* interfaces, one implementation per platform sits below.

Reads: From top to bottom. Drivers, handlers and board HALs hold references to the abstract Base* classes (listed
from inc/base/). Each platform folder under inc/mcu/ implements them with that platform's SDK: ESP32 classes over
ESP-IDF, STM32 classes over STM32Cube HAL handles, and host classes for tests. The application picks one platform
at build time; nothing above the interface line changes.
"""

import re
from pathlib import Path

from hardfoc_mkdocs_theme.figkit import Fig

ROOT = Path(__file__).resolve().parents[3]
IMPL = re.compile(r"^class (\w+)(?: final)? : public (Base\w+)", re.M)


def _impls(folder):
    out = []
    for h in sorted((ROOT / "inc/mcu" / folder).glob("*.h")):
        out += [c for c, _ in IMPL.findall(h.read_text()) if not c.endswith("Stub")]
    return out


def data():
    bases = sorted(p.stem for p in (ROOT / "inc/base").glob("Base*.h"))
    return {
        "bases": bases,
        "esp32": _impls("esp32"),
        "stm32": _impls("stm32"),
        "host": _impls("host"),
        "sources": ["inc/base:Base classes", "inc/mcu:implementations"],
    }


def _rows_h(f, items, w, gap=8):
    """Height chips() will use for ``items`` in width ``w`` (so the three columns can share one height)."""
    h = f.k["pill"] + 10
    cx, cy = 0.0, 0.0
    for s in items:
        pw = f.pill_width(s)
        if cx > 0 and cx + pw > w:
            cx, cy = 0.0, cy + h + gap
        cx += pw + gap
    return cy + h


def build(d):
    f = Fig(
        "interface-layers",
        "Write against the interface, pick the MCU at build time",
        alt="Layer diagram: drivers, handlers and board HALs call the abstract Base interfaces such as BaseGpio, "
        "BaseI2c, BaseSpi and BaseCan; below them the ESP32 classes implement the interfaces with ESP-IDF, the "
        "STM32 classes with STM32Cube HAL handles, and host classes serve tests.",
        width=1200,
        kicker="Architecture",
        subtitle=f"{len(d['bases'])} abstract interfaces in inc/base/; one implementation folder per platform "
        "in inc/mcu/.",
        profile="overview",
        legend=[("slate", "your code"), ("brand", "interfaces"), ("teal", "platform implementations")],
    )
    W = f.body_w
    users = f.card(0, 0, W, "Drivers, handlers, board HALs",
                   ["hf-core handlers, hf-hal-* boards, your application: hold BaseGpio&, BaseI2c&, BaseCan& ..."],
                   tone="slate")
    top = users.b + 40
    chips_h = f.chips(16, f.region_inner_top(top), W - 32, d["bases"], tone="brand")
    base = f.region(0, top, W, f.region_inner_top(top) - top + chips_h + 18, "Interfaces", tone="brand",
                    sub="inc/base/, no platform code")
    gap = 20
    cw = (W - 2 * gap) / 3
    y = base.b + 40
    cols = []
    specs = (("esp32", "ESP32", "ESP-IDF v5.5"), ("stm32", "STM32", "CubeMX HAL handles"), ("host", "Host", "tests"))
    col_h = max(_rows_h(f, d[k] or ["-"], cw - 28) for k, _, _ in specs)
    for i, (key, label, sub) in enumerate(specs):
        x = i * (cw + gap)
        inner = f.region_inner_top(y)
        f.chips(x + 14, inner, cw - 28, d[key] or ["-"], tone="teal")
        cols.append(f.region(x, y, cw, inner - y + col_h + 18, label, tone="teal", sub=sub))
    f.connect(users, base, "bottom", "top", tone="rail")
    for c in cols:
        f.connect(base, c, "bottom", "top", bt=0.5, at=(c.cx - base.x) / base.w, tone="rail")
    return f
