# ClockOS 2.0 — Phoenix
## User Manual

### 1. Hardware requirements

- Arduino Nano (ATmega328P)
- DS3231 RTC module
- HW-069 TM1637 display
- Jumper wires

### 2. Wiring

Disconnect power before connecting components.

**DS3231 RTC**

| Module | Arduino Nano |
|---|---|
| VCC | 5V |
| GND | GND |
| SDA | A4 |
| SCL | A5 |

**HW-069 display**

| Module | Arduino Nano |
|---|---|
| VCC | 5V |
| GND | GND |
| CLK | D2 |
| DIO | D3 |

### 3. Installation

1. Install Arduino IDE.
2. Install RTClib by Adafruit.
3. Install TM1637Display by Avishay Orpaz.
4. Open the ClockOS sketch.
5. Select Arduino Nano and the correct COM port.
6. Compile and upload.

### 4. Serial Monitor

Baud rate: 9600

Line ending: Newline

### 5. Available commands

| Command | Function |
|---|---|
| HELP | List commands |
| BOOT | Return to normal operation |
| RECOVERY | Enter recovery environment |
| SAFE | Enter basic safe mode |
| STATUS | Display system status |
| TEST ALL | Run diagnostics |
| TEST RTC | Check RTC |
| TEST RAM | Estimate available RAM |
| TEST DISPLAY | Test display segments |
| SET HH:MM:SS | Configure time |
| BRIGHT 0-7 | Adjust brightness |
| LOG | View error history |
| RESET LOG | Clear error history |
| ERRORS | View current fault status |
| VERSION | Show firmware version |
| UPTIME | Show system uptime |
| FACTORY CONFIRM | Restore default settings and clear logs |
| REBOOT | Display restart instructions |

### 6. Error codes

| Code | Description |
|---|---|
| E000 | No detected RTC errors |
| E001 | RTC oscillator stopped |
| E002 | RTC not detected |
| E003 | RTC communication failure |
| E004 | Invalid RTC data |
| E005 | Invalid time command |
| E006 | RTC time frozen |

### 7. Automatic recovery

ClockOS enters recovery after three consecutive RTC failures if valid time is unavailable.

Set the correct time or resolve the RTC fault before returning to normal operation.

### 8. Operating modes

- COS2: Startup logo.
- rECo: Recovery environment.
- SAFE: Basic diagnostic mode.
- 8888: Display segment test.
- HH:MM: Normal clock operation.

### 9. Known limitations

- Safe mode does not isolate hardware.
- Display diagnostics require visual inspection.
- Software time is lost after power loss.
- Restarting requires the physical reset button.

### 10. Firmware information

Version: 2.0.0

Codename: Phoenix

Release date: 24 November 2026

License: MIT
