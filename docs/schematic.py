"""Schematic of the moto-control fan controller, plain ESP32 WROOM-32 (schemdraw).

Run:  /home/shovol/Lab/iot/moto-control/.venv/bin/python docs/schematic.py  -> docs/schematic.svg
Source of truth for pins is include/config.h (BOARD_ESP32DEV section). Only used pins are drawn.
The fan load side (12 V supply, fan, 1N4007 flyback) is not drawn; see docs/BENCH_BRINGUP.md.
"""
from pathlib import Path

import schemdraw
import schemdraw.elements as elm
from schemdraw.elements import IcPin

OUT = Path(__file__).with_name("schematic.svg")
ROW = 1.6  # vertical pitch between ESP32 pins

# ESP32 right-hand pins, top to bottom. None = spacer row (keeps groups apart).
RIGHT = ["D18", "D23", "D17", "D16", "D25", None,
         "D21", "D22", None, None,
         "D5", None,
         "D4"]
N = len(RIGHT)

schemdraw.use("svg")
with schemdraw.Drawing(file=str(OUT), show=False) as d:
    d.config(unit=3, fontsize=11)

    # schemdraw numbers slots bottom to top, so slot = N - index.
    esp = elm.Ic(
        pins=[IcPin(name="3V3", side="left", slot="2/2"),
              IcPin(name="GND", side="left", slot="1/2")]
             + [IcPin(name=nm, side="right", slot=f"{N - i}/{N}")
                for i, nm in enumerate(RIGHT) if nm],
        size=(3.2, ROW * N),
        pinspacing=ROW,
        leadlen=1,
        edgepadding=0.4,
        lblsize=11,
    ).label("ESP32 WROOM-32\n(GPIO numbers)", "top", fontsize=12)
    d += esp

    def row(pin):
        return esp[pin]

    def vdd(pt):
        """Power stub leaving a right-side pin: right, then up to a 3V3 symbol."""
        d.add(elm.Line().at(pt).right(0.4))
        d.add(elm.Line().up(0.4))
        d.add(elm.Vdd().label("3V3"))

    def gnd(pt):
        d.add(elm.Line().at(pt).right(0.4))
        d.add(elm.Line().down(0.3))
        d.add(elm.Ground())

    def note(x, y, text, halign="left"):
        d.add(elm.Label().at((x, y)).label(text, halign=halign, fontsize=10))

    def block(pins, size, label, anchor, at):
        """Peripheral IC box; theta(0) because earlier Line().left() flips direction."""
        ic = elm.Ic(pins=pins, size=size, pinspacing=ROW, leadlen=1,
                    edgepadding=0.5, lblsize=10).label(label, "center", fontsize=10)
        d.add(ic.at(at).anchor(anchor).theta(0))
        return ic

    # Power rails (left side)
    d += elm.Line().at(esp["3V3"]).left(0.6)
    d += elm.Vdd().label("3V3")
    d += elm.Line().at(esp["GND"]).left(0.6)
    d += elm.Ground()

    # OLED, 4-wire software SPI: D18 CLK, D23 MOSI, D17 DC, D16 CS, D25 RES
    oled_pins = [("D0 (CLK)", "clk"), ("D1 (MOSI)", "mosi"), ("DC", "dc"),
                 ("CS", "cs"), ("RES", "res")]
    oled = block(
        [IcPin(name=nm, anchorname=an, side="left", slot=f"{5 - i}/5")
         for i, (nm, an) in enumerate(oled_pins)]
        + [IcPin(name="VCC", side="right", slot="2/2"),
           IcPin(name="GND", side="right", slot="1/2")],
        (4.6, ROW * 4 + 1.2), "SSD1306 OLED\n128x64 SPI", "dc",
        (row("D17")[0] + 3.2, row("D17")[1]))
    for gpio, an in (("D18", "clk"), ("D23", "mosi"), ("D17", "dc"),
                     ("D16", "cs"), ("D25", "res")):
        d += elm.Line().at(row(gpio)).to(oled[an])
    vdd(oled["VCC"])
    gnd(oled["GND"])

    # ADS1115 on I2C: D21 SDA, D22 SCL; battery divider on A0
    ads = block(
        [IcPin(name="SDA", side="left", slot="2/2"),
         IcPin(name="SCL", side="left", slot="1/2"),
         IcPin(name="VCC", side="right", slot="3/3"),
         IcPin(name="A0", side="right", slot="2/3"),
         IcPin(name="GND", side="right", slot="1/3")],
        (4.0, ROW * 2 + 0.8), "ADS1115\nADC", "SDA",
        (row("D21")[0] + 3.2, row("D21")[1]))
    d += elm.Line().at(row("D21")).to(ads["SDA"])
    d += elm.Line().at(row("D22")).to(ads["SCL"])
    vdd(ads["VCC"])
    gnd(ads["GND"])

    # Battery divider 6:1 (nominal 50k / 10k): BATT+ - R1 - A0 node - R2 - GND
    d += elm.Line().at(ads["A0"]).right(2.4)
    d += elm.Dot()
    node = d.here
    d += elm.Resistor().at(node).up().length(1.4)
    d += elm.Line().up(0.3)
    d += elm.Vdd().label("BATT+ (12 V)")
    d += elm.Resistor().at(node).down().length(1.4)
    d += elm.Line().down(0.3)
    d += elm.Ground()
    note(node[0] + 0.4, node[1] + 0.9, "50k")
    note(node[0] + 0.4, node[1] - 0.9, "10k")
    note(node[0] + 0.4, node[1] - 0.0, "ADC tap")

    # DS18B20 on 1-Wire: D5 with 4.7k pull-up to 3V3
    d += elm.Line().at(row("D5")).right(1.2)
    d += elm.Dot()
    pnode = d.here
    d += elm.Resistor().at(pnode).up().length(1.4)
    d += elm.Line().up(0.3)
    d += elm.Vdd().label("3V3")
    note(pnode[0] - 0.3, pnode[1] + 0.9, "4.7k pull-up", halign="right")
    d += elm.Line().at(pnode).right(1.0)
    ds = block(
        [IcPin(name="DQ", side="left"),
         IcPin(name="VCC", side="right", slot="2/2"),
         IcPin(name="GND", side="right", slot="1/2")],
        (3.8, ROW + 1.0), "DS18B20\ncoolant probe", "DQ", d.here)
    vdd(ds["VCC"])
    gnd(ds["GND"])

    # D4: fan MOSFET module and fan-ON indicator LED (same GPIO, in parallel)
    d += elm.Line().at(row("D4")).right(1.2)
    d += elm.Dot()
    fnode = d.here
    d += elm.Resistor().at(fnode).down().length(1.4)
    d += elm.LED().down().color("blue").length(1.4)
    d += elm.Line().down(0.3)
    d += elm.Ground()
    note(fnode[0] - 0.6, fnode[1] - 0.9, "220", halign="right")
    note(fnode[0] + 1.1, fnode[1] - 2.6, "fan-ON indicator LED")
    d += elm.Line().at(fnode).right(1.0)
    mos = block(
        [IcPin(name="SIG", side="left"), IcPin(name="SIG-GND", side="bottom")],
        (5.0, ROW + 1.0), "LR7843 module\n(active-HIGH assumed)", "SIG", d.here)
    gnd_pt = mos["SIG-GND"]
    d += elm.Line().at(gnd_pt).down(0.4)
    d += elm.Ground()
    note(mos["SIG"][0] + 6.3, mos["SIG"][1],
         "load side: 12 V + fan\n+ 1N4007 flyback\n(not drawn)")
