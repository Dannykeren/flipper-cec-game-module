/**
 * Flipper Zero Video Game Module - CEC Test Tool Firmware
 * Adapted from your working Raspberry Pi cec_control.py
 * 
 * Features:
 * - USB Serial interface for debugging (connect via USB, use terminal)
 * - UART interface for Flipper Zero communication
 * - Core CEC functionality from your proven RPi code
 * - Same command structure that worked on RPi
 */

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/unique_id.h"
#include "pico/bootrom.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Pin definitions for Video Game Module
#define LED_PIN 25              // Built-in LED for status
#define CEC_PIN 20              // CEC data line (connect to HDMI)
#define FLIPPER_UART_ID uart1   // UART for Flipper communication
#define FLIPPER_TX_PIN 4        // TX to Flipper Zero
#define FLIPPER_RX_PIN 5        // RX from Flipper Zero
#define FLIPPER_UART_BAUD 115200

// CEC Protocol Constants (from HDMI CEC specification)
#define CEC_LOGICAL_ADDRESS_TV 0x00
#define CEC_LOGICAL_ADDRESS_PLAYBACK1 0x04
#define CEC_LOGICAL_ADDRESS_BROADCAST 0x0F

// CEC Command Opcodes (from your working RPi code)
#define CEC_OPCODE_ACTIVE_SOURCE 0x82
#define CEC_OPCODE_INACTIVE_SOURCE 0x9D
#define CEC_OPCODE_GIVE_DEVICE_POWER_STATUS 0x8F
#define CEC_OPCODE_REPORT_POWER_STATUS 0x90
#define CEC_OPCODE_STANDBY 0x36
#define CEC_OPCODE_GET_CEC_VERSION 0x9F
#define CEC_OPCODE_GIVE_OSD_NAME 0x46
#define CEC_OPCODE_GIVE_DEVICE_VENDOR_ID 0x8C

// CEC Timing Constants (microseconds)
#define CEC_START_BIT_LOW 3700
#define CEC_START_BIT_HIGH 800
#define CEC_DATA_BIT_0_LOW 1500
#define CEC_DATA_BIT_0_HIGH 900
#define CEC_DATA_BIT_1_LOW 600
#define CEC_DATA_BIT_1_HIGH 1800
#define CEC_ACK_LOW 1500
#define CEC_ACK_HIGH 900

// Global state
static char command_buffer[256];
static char response_buffer[512];
static uint8_t our_logical_address = CEC_LOGICAL_ADDRESS_PLAYBACK1;
static uint32_t last_command_time = 0;
static const uint32_t COMMAND_COOLDOWN_MS = 2000; // From your RPi rate limiting

// Rate limiting (adapted from your RPi code)
bool is_rate_limited() {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time - last_command_time < COMMAND_COOLDOWN_MS) {
        printf("[RATE_LIMIT] Command blocked to prevent looping\n");
        return true;
    }
    last_command_time = current_time;
    return false;
}

// LED status indication
void set_status_led(bool on) {
    gpio_put(LED_PIN, on);
}

void blink_status_led(int count) {
    for (int i = 0; i < count; i++) {
        set_status_led(true);
        sleep_ms(100);
        set_status_led(false);
        sleep_ms(100);
    }
}

// CEC Bus Control
void cec_set_pin_high() {
    gpio_set_dir(CEC_PIN, GPIO_IN);  // High impedance = logic high
    gpio_pull_up(CEC_PIN);
}

void cec_set_pin_low() {
    gpio_set_dir(CEC_PIN, GPIO_OUT);
    gpio_put(CEC_PIN, 0);
}

bool cec_read_pin() {
    return gpio_get(CEC_PIN);
}

// CEC Bit Transmission
void cec_send_bit(bool bit_value) {
    if (bit_value) {
        // Send logical '1'
        cec_set_pin_low();
        busy_wait_us(CEC_DATA_BIT_1_LOW);
        cec_set_pin_high();
        busy_wait_us(CEC_DATA_BIT_1_HIGH);
    } else {
        // Send logical '0'
        cec_set_pin_low();
        busy_wait_us(CEC_DATA_BIT_0_LOW);
        cec_set_pin_high();
        busy_wait_us(CEC_DATA_BIT_0_HIGH);
    }
}

void cec_send_start_bit() {
    cec_set_pin_low();
    busy_wait_us(CEC_START_BIT_LOW);
    cec_set_pin_high();
    busy_wait_us(CEC_START_BIT_HIGH);
}

bool cec_wait_for_ack() {
    // Wait for ACK (line pulled low by receiver)
    absolute_time_t timeout = delayed_by_us(get_absolute_time(), 2000);
    
    while (!time_reached(timeout)) {
        if (!cec_read_pin()) {
            return true; // ACK received
        }
        busy_wait_us(10);
    }
    return false; // NACK or timeout
}

// CEC Frame Transmission
bool cec_send_frame(uint8_t* data, size_t length) {
    if (length == 0 || length > 16) {
        return false;
    }
    
    // Check if bus is free
    if (!cec_read_pin()) {
        printf("[CEC] Bus busy, cannot send\n");
        return false;
    }
    
    printf("[CEC] Sending frame: ");
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    
    // Send start bit
    cec_send_start_bit();
    
    // Send each byte
    for (size_t byte_idx = 0; byte_idx < length; byte_idx++) {
        uint8_t byte_data = data[byte_idx];
        
        // Send 8 data bits
        for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
            bool bit_value = (byte_data >> bit_idx) & 1;
            cec_send_bit(bit_value);
        }
        
        // Send EOM bit (1 if last byte, 0 otherwise)
        bool is_last_byte = (byte_idx == length - 1);
        cec_send_bit(is_last_byte);
        
        // Wait for ACK
        if (!cec_wait_for_ack()) {
            printf("[CEC] No ACK received for byte %zu\n", byte_idx);
            return false;
        }
    }
    
    printf("[CEC] Frame sent successfully\n");
    return true;
}

// High-level CEC Commands (adapted from your RPi cec_control.py)
bool cec_scan_devices() {
    printf("[CEC] Scanning for devices...\n");
    blink_status_led(2);
    
    // Send polling messages to discover devices
    bool devices_found = false;
    
    for (uint8_t addr = 0; addr <= 14; addr++) {
        if (addr == our_logical_address) continue;
        
        // Create polling message (header only)
        uint8_t polling_frame = (our_logical_address << 4) | addr;
        
        if (cec_send_frame(&polling_frame, 1)) {
            printf("[CEC] Device found at logical address %d\n", addr);
            devices_found = true;
        }
        
        sleep_ms(100); // Brief delay between polls
    }
    
    if (!devices_found) {
        printf("[CEC] No devices found\n");
    }
    
    return devices_found;
}

bool cec_power_on() {
    if (is_rate_limited()) {
        return false;
    }
    
    printf("[CEC] Sending POWER ON command\n");
    blink_status_led(3);
    
    // Send "Active Source" to broadcast
    uint8_t frame[] = {
        (our_logical_address << 4) | CEC_LOGICAL_ADDRESS_BROADCAST, // Header
        CEC_OPCODE_ACTIVE_SOURCE,                                   // Opcode
        0x10, 0x00                                                  // Physical address (1.0.0.0)
    };
    
    bool success = cec_send_frame(frame, sizeof(frame));
    if (success) {
        printf("[CEC] Power ON command sent successfully\n");
    } else {
        printf("[CEC] Failed to send Power ON command\n");
    }
    
    return success;
}

bool cec_power_off() {
    if (is_rate_limited()) {
        return false;
    }
    
    printf("[CEC] Sending POWER OFF command\n");
    blink_status_led(1);
    
    // Send "Standby" to broadcast
    uint8_t frame[] = {
        (our_logical_address << 4) | CEC_LOGICAL_ADDRESS_BROADCAST, // Header
        CEC_OPCODE_STANDBY                                          // Opcode
    };
    
    bool success = cec_send_frame(frame, sizeof(frame));
    if (success) {
        printf("[CEC] Power OFF command sent successfully\n");
    } else {
        printf("[CEC] Failed to send Power OFF command\n");
    }
    
    return success;
}

bool cec_get_power_status() {
    printf("[CEC] Checking power status\n");
    
    // Send "Give Device Power Status" to TV
    uint8_t frame[] = {
        (our_logical_address << 4) | CEC_LOGICAL_ADDRESS_TV, // Header
        CEC_OPCODE_GIVE_DEVICE_POWER_STATUS                  // Opcode
    };
    
    bool success = cec_send_frame(frame, sizeof(frame));
    if (success) {
        printf("[CEC] Power status request sent\n");
    } else {
        printf("[CEC] Failed to send power status request\n");
    }
    
    return success;
}

bool cec_send_custom_command(const char* command_str) {
    if (is_rate_limited()) {
        return false;
    }
    
    printf("[CEC] Sending custom command: %s\n", command_str);
    
    // Parse hex command string (simple implementation)
    uint8_t frame[16];
    size_t frame_length = 0;
    
    // Simple hex parsing - expects format like "40820100" (no spaces)
    size_t cmd_len = strlen(command_str);
    if (cmd_len % 2 != 0 || cmd_len > 32) {
        printf("[CEC] Invalid command format\n");
        return false;
    }
    
    for (size_t i = 0; i < cmd_len; i += 2) {
        char hex_byte[3] = {command_str[i], command_str[i+1], '\0'};
        frame[frame_length++] = (uint8_t)strtol(hex_byte, NULL, 16);
    }
    
    bool success = cec_send_frame(frame, frame_length);
    if (success) {
        printf("[CEC] Custom command sent successfully\n");
    } else {
        printf("[CEC] Failed to send custom command\n");
    }
    
    return success;
}

// Command processor (handles both USB and UART)
void process_command(const char* cmd, bool send_to_flipper) {
    char response[256] = "";
    bool success = false;
    
    // Convert to uppercase for comparison
    char cmd_upper[64];
    strncpy(cmd_upper, cmd, sizeof(cmd_upper) - 1);
    cmd_upper[sizeof(cmd_upper) - 1] = '\0';
    
    for (char* p = cmd_upper; *p; p++) {
        *p = toupper(*p);
    }
    
    // Process commands (same structure as your RPi web interface)
    if (strcmp(cmd_upper, "SCAN") == 0) {
        success = cec_scan_devices();
        snprintf(response, sizeof(response), "SCAN_RESULT:%s", 
                success ? "DEVICES_FOUND" : "NO_DEVICES");
                
    } else if (strcmp(cmd_upper, "POWER_ON") == 0 || strcmp(cmd_upper, "ON") == 0) {
        success = cec_power_on();
        snprintf(response, sizeof(response), "POWER_ON:%s", 
                success ? "SUCCESS" : "FAILED");
                
    } else if (strcmp(cmd_upper, "POWER_OFF") == 0 || strcmp(cmd_upper, "OFF") == 0) {
        success = cec_power_off();
        snprintf(response, sizeof(response), "POWER_OFF:%s", 
                success ? "SUCCESS" : "FAILED");
                
    } else if (strcmp(cmd_upper, "STATUS") == 0) {
        success = cec_get_power_status();
        snprintf(response, sizeof(response), "STATUS:%s", 
                success ? "REQUEST_SENT" : "FAILED");
                
    } else if (strncmp(cmd_upper, "CUSTOM:", 7) == 0) {
        const char* custom_cmd = cmd + 7; // Skip "CUSTOM:"
        success = cec_send_custom_command(custom_cmd);
        snprintf(response, sizeof(response), "CUSTOM:%s", 
                success ? "SUCCESS" : "FAILED");
                
    } else if (strcmp(cmd_upper, "HELP") == 0) {
        snprintf(response, sizeof(response), 
                "COMMANDS: SCAN, POWER_ON, POWER_OFF, STATUS, CUSTOM:xxxx, HELP, VERSION");
        success = true;
        
    } else if (strcmp(cmd_upper, "VERSION") == 0) {
        snprintf(response, sizeof(response), "VERSION:CEC_MODULE_V1.0");
        success = true;
        
    } else {
        snprintf(response, sizeof(response), "ERROR:UNKNOWN_COMMAND");
    }
    
    // Send response via USB (always)
    printf("RESPONSE: %s\n", response);
    
    // Send response via UART to Flipper (if requested)
    if (send_to_flipper && uart_is_writable(FLIPPER_UART_ID)) {
        uart_puts(FLIPPER_UART_ID, response);
        uart_puts(FLIPPER_UART_ID, "\n");
    }
}

// USB command processing
void process_usb_commands() {
    static size_t cmd_pos = 0;
    
    int c = getchar_timeout_us(0); // Non-blocking read
    if (c == PICO_ERROR_TIMEOUT) {
        return;
    }
    
    if (c == '\n' || c == '\r') {
        if (cmd_pos > 0) {
            command_buffer[cmd_pos] = '\0';
            printf("\n[USB] Received command: %s\n", command_buffer);
            process_command(command_buffer, false); // Don't send to Flipper from USB
            cmd_pos = 0;
            printf("\n[USB] Ready for next command: ");
        }
    } else if (c >= 32 && c <= 126 && cmd_pos < sizeof(command_buffer) - 1) {
        command_buffer[cmd_pos++] = c;
        printf("%c", c); // Echo character
    }
}

// UART command processing (from Flipper Zero)
void process_flipper_commands() {
    static size_t cmd_pos = 0;
    
    while (uart_is_readable(FLIPPER_UART_ID)) {
        int c = uart_getc(FLIPPER_UART_ID);
        
        if (c == '\n' || c == '\r') {
            if (cmd_pos > 0) {
                command_buffer[cmd_pos] = '\0';
                printf("[FLIPPER] Received command: %s\n", command_buffer);
                process_command(command_buffer, true); // Send response to Flipper
                cmd_pos = 0;
            }
        } else if (c >= 32 && c <= 126 && cmd_pos < sizeof(command_buffer) - 1) {
            command_buffer[cmd_pos++] = c;
        }
    }
}

// Hardware initialization
void init_hardware() {
    // Initialize stdio for USB
    stdio_init_all();
    
    // Initialize LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
    
    // Initialize CEC pin
    gpio_init(CEC_PIN);
    cec_set_pin_high(); // Start with bus released
    
    // Initialize UART for Flipper communication
    uart_init(FLIPPER_UART_ID, FLIPPER_UART_BAUD);
    gpio_set_function(FLIPPER_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(FLIPPER_RX_PIN, GPIO_FUNC_UART);
    
    // Power-on self-test
    printf("\n\n=================================\n");
    printf("CEC Test Tool - Game Module v1.0\n");
    printf("Based on working RPi implementation\n");
    printf("=================================\n\n");
    
    // Startup blink sequence
    for (int i = 0; i < 5; i++) {
        set_status_led(true);
        sleep_ms(200);
        set_status_led(false);
        sleep_ms(200);
    }
    
    printf("Hardware initialized successfully!\n");
    printf("CEC pin: %d, Flipper UART: TX=%d RX=%d\n", CEC_PIN, FLIPPER_TX_PIN, FLIPPER_RX_PIN);
    printf("\nUSB Debug Interface:\n");
    printf("Commands: SCAN, POWER_ON, POWER_OFF, STATUS, CUSTOM:xxxx, HELP\n");
    printf("Ready for commands: ");
    
    // Send ready signal to Flipper
    uart_puts(FLIPPER_UART_ID, "CEC_MODULE_READY\n");
}

// Main program
int main() {
    init_hardware();
    
    // Main loop
    while (true) {
        // Process USB commands for debugging
        process_usb_commands();
        
        // Process Flipper commands
        process_flipper_commands();
        
        // Heartbeat LED (very brief flash every 5 seconds)
        static uint32_t last_heartbeat = 0;
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_heartbeat > 5000) {
            set_status_led(true);
            sleep_ms(10);
            set_status_led(false);
            last_heartbeat = now;
        }
        
        // Small delay to prevent busy waiting
        sleep_ms(10);
    }
    
    return 0;
}
