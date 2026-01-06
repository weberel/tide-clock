# Hardware Versions

## Rev A (Tested)

First production version. Fully tested and working.

### Key ICs

| Component | Part Number | Function |
|-----------|-------------|----------|
| MCU | STM32F411RET6 | ARM Cortex-M4, 512KB flash, 128KB RAM |
| RTC | DS3231MZ+ | Real-time clock with temperature-compensated crystal |
| Boost Converter | TPS61099DRVR | 3.3V output from AA batteries |
| NFC | NT3H2111 | NFC tag IC (not used in firmware) |
| P-FET | AO3401A | Power gating for display and RTC VCC |
| N-FET | AO3400A | Signal level shifting |

### Features
- 2.9" e-paper display (SSD1680 controller)
- Button for time setting (active LOW)
- AA battery power
- Deep sleep with RTC alarm wake

### Pin Configuration
| Pin | Function |
|-----|----------|
| PA0 | Wakeup (DS3231 INT via P-FET inverter) |
| PA1 | DS3231 VCC gate (P-FET, active LOW) |
| PA2 | NT3H2111 VCC gate (P-FET, active LOW, unused) |
| PA3 | Display VCC gate (P-FET, active LOW) |
| PB1 | Button input (active LOW) |
| PB6/PB7 | I2C1 (DS3231) |
| PC11 | LED2 |
| PD2 | LED1 |

---

## Rev B (Not Yet Tested)

Improved version with NFC time setting and external power output.

### Key ICs

| Component | Part Number | Function |
|-----------|-------------|----------|
| MCU | STM32F411RET6 | ARM Cortex-M4, 512KB flash, 128KB RAM |
| RTC | DS3231MZ+ | Real-time clock with temperature-compensated crystal |
| Boost Converter | TPS61099DRVR | 3.3V output from AA batteries |
| NFC | ST25DV04K-JFR6D3 | Dynamic NFC tag with I2C interface |
| P-FET | AO3401A | Power gating |
| N-FET | AO3400A | Signal level shifting |

### Changes from Rev A

| Change | Rev A | Rev B |
|--------|-------|-------|
| NFC chip | NT3H2111 (unused) | ST25DV04K (active) |
| Time setting | Button (PB1) | NFC (phone writes time) |
| PA2 function | NT3H2111 power gate | NFC LPD (Low Power Down) |
| Button | PB1 (active LOW) | Removed |
| Wakeup source | RTC alarm only | RTC alarm OR NFC field detect |
| External power | None | PB9 (3V3 output with P-FET gate) |
| DS3231 RST pullup | Present (parasitic) | Removed |
| ST-Link debug | Basic | Debug pin exposed (PB3/SWO) |

### Wakeup Circuit (Rev B)

PA0 receives OR'd signal from two sources:
1. **DS3231 alarm**: INT pin (active LOW) → P-FET inverter → HIGH
2. **ST25DV GPO**: CMOS output → HIGH when RF field detected

Both signals go through Schottky diodes for OR-ing, with PA0 pulled LOW.
Rising edge on PA0 wakes MCU from STOP mode.

### Pin Configuration
| Pin | Function |
|-----|----------|
| PA0 | Wakeup (OR'd: RTC alarm + NFC field detect) |
| PA1 | DS3231 VCC gate (P-FET, active LOW) |
| PA2 | ST25DV LPD (HIGH = low power, LOW = active) |
| PA3 | Display VCC gate (P-FET, active LOW) |
| PB3 | Debug output (SWO) |
| PB6/PB7 | I2C1 (DS3231 + ST25DV) |
| PB9 | External 3V3 gate (P-FET, active LOW) |
| PC11 | LED2 |
| PD2 | LED1 |

### NFC Time Sync Protocol

Phone app writes 8 bytes to ST25DV user memory at offset 0x0010:

| Byte | Content |
|------|---------|
| 0 | Year - 2000 |
| 1 | Month (1-12) |
| 2 | Day (1-31) |
| 3 | Hour (0-23) |
| 4 | Minute (0-59) |
| 5 | Second (0-59) |
| 6 | Checksum (XOR of bytes 0-5) |
| 7 | Magic (0xA5 = valid) |

When NFC field is detected:
1. MCU wakes via GPO → PA0
2. Reads time sync data from ST25DV
3. If valid (magic + checksum), updates DS3231 RTC
4. Clears magic byte to prevent re-reading
5. Optionally updates display to confirm sync
