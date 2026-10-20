# Run it in Wokwi (no hardware)

`diagram.json` + `wokwi.toml` describe a minimal circuit for the [Wokwi](https://wokwi.com) simulator.
It runs the `esp32dev_bench` firmware (fan ON at 30 C, OFF at 28 C), so the fan is easy to trigger.

- **VS Code:** install the Wokwi extension, run `pio run -e esp32dev_bench`, then *Wokwi: Start Simulator*.
- **Try it:** click the DS18B20 and raise its temperature above 30 C. The blue FAN LED (GPIO4)
  turns on and the serial line shows `fan=ON`. Drop it below 28 C and it turns off.
- **Automated test:** `wokwi-cli --scenario test/fan.test.yaml --timeout 30000 .`
  (needs a free `WOKWI_CLI_TOKEN`).

What is simulated: DS18B20 on GPIO5 (4.7k pullup to 3V3), fan LED + 220R on GPIO4.

What is not:
- **ADS1115** (battery voltage): Wokwi has no ADS1115 part that I know of. The firmware logs
  `ADS1115:ABSENT`, reads 0 V and the motohour meter stays stopped.
- **OLED**: the firmware drives an SSD1306 over software SPI; Wokwi's `board-ssd1306` is I2C only.
  SPI has no ACK, so the firmware just bit-bangs into nothing and carries on. Use the serial output.
