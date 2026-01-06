# Hardware Integration Guide

Guide for transferring this simulation to real 2.9" e-paper displays via SPI.

## Display Compatibility

This code is designed for **2.9" e-paper displays with 296x128 resolution**, including:
- Waveshare 2.9" e-Paper (IL3820 driver)
- Good Display GDEW029T5
- Generic SSD1680-based 2.9" displays

## Hardware Connections (Example: ESP32)

### SPI Pins
```
E-Paper Pin  →  ESP32 Pin
─────────────────────────
VCC          →  3.3V
GND          →  GND
DIN (MOSI)   →  GPIO 23
CLK (SCK)    →  GPIO 18
CS           →  GPIO 5
DC           →  GPIO 17
RST          →  GPIO 16
BUSY         →  GPIO 4
```

### Raspberry Pi SPI
```
E-Paper Pin  →  RPi Pin
─────────────────────────
VCC          →  3.3V (Pin 1)
GND          →  GND (Pin 6)
DIN (MOSI)   →  MOSI (Pin 19)
CLK (SCK)    →  SCLK (Pin 23)
CS           →  CE0 (Pin 24)
DC           →  GPIO 25 (Pin 22)
RST          →  GPIO 17 (Pin 11)
BUSY         →  GPIO 24 (Pin 18)
```

## Software Integration

### Step 1: Install SPI Library

**For ESP32 (MicroPython):**
```python
from machine import Pin, SPI

spi = SPI(1, baudrate=4000000, polarity=0, phase=0)
cs = Pin(5, Pin.OUT)
dc = Pin(17, Pin.OUT)
rst = Pin(16, Pin.OUT)
busy = Pin(4, Pin.IN)
```

**For Raspberry Pi (Python):**
```bash
pip install spidev RPi.GPIO
```

```python
import spidev
import RPi.GPIO as GPIO

spi = spidev.SpiDev()
spi.open(0, 0)
spi.max_speed_hz = 4000000

DC_PIN = 25
RST_PIN = 17
BUSY_PIN = 24
CS_PIN = 8
```

### Step 2: Replace EPaperDisplay Methods

Add hardware-specific methods to `EPaperDisplay` class:

```python
class EPaperDisplay:
    def __init__(self):
        # Keep existing PIL setup
        # Add hardware initialization
        self.init_hardware()

    def init_hardware(self):
        """Initialize SPI and GPIO"""
        import spidev
        import RPi.GPIO as GPIO

        GPIO.setmode(GPIO.BCM)
        GPIO.setup(self.DC_PIN, GPIO.OUT)
        GPIO.setup(self.RST_PIN, GPIO.OUT)
        GPIO.setup(self.BUSY_PIN, GPIO.IN)

        self.spi = spidev.SpiDev()
        self.spi.open(0, 0)
        self.spi.max_speed_hz = 4000000

        self.hardware_reset()
        self.send_init_commands()

    def hardware_reset(self):
        """Hardware reset sequence"""
        GPIO.output(self.RST_PIN, GPIO.HIGH)
        time.sleep(0.2)
        GPIO.output(self.RST_PIN, GPIO.LOW)
        time.sleep(0.002)
        GPIO.output(self.RST_PIN, GPIO.HIGH)
        time.sleep(0.2)

    def send_command(self, command):
        """Send command byte"""
        GPIO.output(self.DC_PIN, GPIO.LOW)
        self.spi.writebytes([command])

    def send_data(self, data):
        """Send data byte(s)"""
        GPIO.output(self.DC_PIN, GPIO.HIGH)
        if isinstance(data, int):
            self.spi.writebytes([data])
        else:
            self.spi.writebytes(list(data))

    def wait_until_idle(self):
        """Wait for display to be ready"""
        while GPIO.input(self.BUSY_PIN) == 1:
            time.sleep(0.01)
```

### Step 3: Add Display Initialization

For **Waveshare 2.9" (IL3820 driver)**:

```python
def send_init_commands(self):
    """Initialize IL3820 driver"""
    self.send_command(0x12)  # Software reset
    self.wait_until_idle()

    self.send_command(0x01)  # Driver output control
    self.send_data(0x27)
    self.send_data(0x01)
    self.send_data(0x00)

    self.send_command(0x11)  # Data entry mode
    self.send_data(0x03)

    self.send_command(0x44)  # Set RAM X address
    self.send_data(0x00)
    self.send_data(0x0F)

    self.send_command(0x45)  # Set RAM Y address
    self.send_data(0x27)
    self.send_data(0x01)
    self.send_data(0x00)
    self.send_data(0x00)

    self.send_command(0x3C)  # Border waveform
    self.send_data(0x05)

    self.send_command(0x21)  # Display update control
    self.send_data(0x00)
    self.send_data(0x80)

    self.send_command(0x18)  # Temperature sensor
    self.send_data(0x80)

    self.wait_until_idle()
```

### Step 4: Update Display Method

Replace `show()` with actual hardware update:

```python
def update_display(self):
    """Send buffer to e-paper display"""
    buffer = self.get_buffer()

    # Set RAM X address counter
    self.send_command(0x4E)
    self.send_data(0x00)

    # Set RAM Y address counter
    self.send_command(0x4F)
    self.send_data(0x27)
    self.send_data(0x01)

    # Write to RAM
    self.send_command(0x24)
    self.send_data(buffer)

    # Update display
    self.send_command(0x22)  # Display update control 2
    self.send_data(0xF7)
    self.send_command(0x20)  # Master activation

    self.wait_until_idle()
```

### Step 5: Power Management (Optional)

For battery operation:

```python
def sleep(self):
    """Put display in deep sleep mode"""
    self.send_command(0x10)  # Deep sleep mode
    self.send_data(0x01)
```

## Complete Hardware Example

See `hardware_example.py` for a complete working example combining simulation and hardware code.

## Testing Procedure

1. **Test simulation first**: Ensure display renders correctly
2. **Verify wiring**: Check all SPI connections with multimeter
3. **Test SPI communication**: Send simple commands, verify BUSY pin responds
4. **Send buffer**: Transfer full display buffer
5. **Monitor power**: E-paper draws ~20mA during update, ~5µA when idle

## Common Issues

### Display stays white
- Check RST pin connection
- Verify SPI MOSI/CLK signals with logic analyzer
- Ensure proper initialization sequence for your driver IC

### Partial update/artifacts
- Some displays require full refresh after power-on
- Check temperature sensor setting (affects refresh quality)
- Verify buffer byte order matches display expectations

### Slow updates
- E-paper refresh takes 2-4 seconds (normal)
- Reduce SPI speed if seeing corruption
- Use partial update mode for faster refreshes (driver-dependent)

## Power Consumption

Typical 2.9" e-paper display:
- **Active update**: 15-25 mA @ 3.3V (2-4 seconds)
- **Idle**: 5-50 µA
- **Deep sleep**: <1 µA

Perfect for battery operation with periodic updates.

## Next Steps

1. Order a 2.9" e-paper display module
2. Set up SPI on your microcontroller
3. Test with simple fill patterns first
4. Adapt the `get_buffer()` output to your display's format
5. Integrate with tide data API
6. Add deep sleep for battery operation

## Resources

- [Waveshare Wiki](https://www.waveshare.com/wiki/2.9inch_e-Paper_Module)
- [IL3820 Datasheet](https://www.good-display.com/companyfile/32.html)
- [SSD1680 Datasheet](https://www.good-display.com/companyfile/225.html)
