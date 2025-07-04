# Changelog

## [1.0.0] - 2024-01-XX

### Added
- Initial release of CEC firmware for Video Game Module
- USB serial debug interface with commands: SCAN, POWER_ON, POWER_OFF, STATUS, CUSTOM
- UART interface for Flipper Zero communication
- Core CEC protocol implementation adapted from proven RPi code
- Rate limiting to prevent command loops
- Status LED feedback
- Device discovery functionality
- Power control (on/off) commands
- Custom CEC command support

### Features
- **USB Debug Interface**: Connect via USB serial at 115200 baud
- **Flipper Integration**: UART communication ready for Flipper Zero apps
- **CEC Protocol**: Full implementation with timing-accurate bit transmission
- **Anti-Loop Protection**: 2-second rate limiting from original RPi code
- **Visual Feedback**: LED blinks indicate command execution
- **Robust Error Handling**: Timeout protection and bus checking

### Hardware Support
- Flipper Zero Video Game Module (RP2040-based)
- HDMI CEC via pin GPIO 20
- USB-C for debugging and power
- Built-in LED for status indication

### Compatibility
- Tested with Samsung, Sony, LG TVs
- Works with any CEC-compatible HDMI device
- Compatible with Raspberry Pi Pico SDK
