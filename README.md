# Flipper Zero Game Module - CEC Test Tool Firmware

[![Build Status](https://github.com/Dannykeren/flipper-cec-game-module/workflows/Build%20RP2040%20CEC%20Module%20Firmware/badge.svg)](https://github.com/Dannykeren/flipper-cec-game-module/actions)
[![Release](https://img.shields.io/github/v/release/Dannykeren/flipper-cec-game-module)](https://github.com/Dannykeren/flipper-cec-game-module/releases)

**Custom firmware for the Flipper Zero Video Game Module that adds HDMI CEC (Consumer Electronics Control) functionality.**

This firmware is adapted from the proven Raspberry Pi CEC implementation and provides both USB debugging and Flipper Zero integration.

## ✨ Features

- 🎮 **USB Serial Interface** - Debug and test CEC commands directly via USB
- 🐬 **Flipper Zero Integration** - UART communication with Flipper Zero apps
- 📺 **Full CEC Protocol** - Power control, device discovery, custom commands
- 🔄 **Rate Limiting** - Anti-loop protection from proven RPi implementation
- 💡 **Status LED** - Visual feedback for command execution
- 🛠️ **Debugging Tools** - Real-time command logging via USB serial

## 🚀 Quick Start

### 1. Download Firmware
- Go to [Releases](https://github.com/Dannykeren/flipper-cec-game-module/releases)
- Download `flipper_cec_module.uf2`

### 2. Flash the Game Module
1. **Hold BOOTSEL button** on the Video Game Module
2. **Connect via USB-C** while holding BOOTSEL
3. **Module appears as USB drive** "RPI-RP2"
4. **Drag `flipper_cec_module.uf2`** to the drive
5. **Module reboots** with new firmware automatically

### 3. Test via USB (Recommended First)
1. **Connect Game Module** to computer via USB-C
2. **Open serial terminal** (PuTTY, Arduino IDE, etc.)
3. **Connect at 115200 baud** to the module's COM port
4. **Type commands** to test CEC functionality

## 🎯 USB Debug Commands

Connect via USB serial and use these commands:

| Command | Description | Example |
|---------|-------------|---------|
| `SCAN` | Scan for CEC devices | `> SCAN` |
| `POWER_ON` | Send power on command | `> POWER_ON` |
| `POWER_OFF` | Send power off command | `> POWER_OFF` |
| `STATUS` | Check device power status | `> STATUS` |
| `CUSTOM:xxxx` | Send custom hex command | `> CUSTOM:40820100` |
| `HELP` | Show available
