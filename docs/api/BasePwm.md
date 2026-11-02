# BasePwm API Reference
## Overview

`BasePwm` is the abstract base class for all PWM (Pulse Width Modulation) implementations in the
HardFOC system.
It provides a unified interface for motor control, LED dimming, servo control,
and other PWM applications with comprehensive multi-channel support.

## Features

- **🔢 Multi-Channel Support** - Control multiple PWM channels simultaneously
- **⚡ Variable Frequency** - Configurable frequency per channel with wide range support
- **🎯 Precise Duty Control** - High-resolution duty cycle control with hardware precision
- **🔄 Fade Operations** - Smooth transitions between duty cycle values
- **🛡️ Hardware Protection** - Built-in fault detection and recovery mechanisms
- **📊 Performance Monitoring** - Comprehensive statistics and diagnostics
- **🔧 Lazy Initialization** - Resources allocated only when needed
- **🏎️ Real-Time Optimized** - Designed for time-critical motor control applications

## Header File

```cpp
#include "inc/base/BasePwm.h"
```

## Type Definitions

### Error Codes

```cpp
enum class hf_pwm_err_t : hf_u32_t {
    PWM_SUCCESS = 0,                    // ✅ Success
    PWM_ERR_FAILURE = 1,                // ❌ General failure
    PWM_ERR_NOT_INITIALIZED = 2,        // ⚠️ Not initialized
    PWM_ERR_ALREADY_INITIALIZED = 3,    // ⚠️ Already initialized
    PWM_ERR_INVALID_PARAMETER = 4,      // 🚫 Invalid parameter
    PWM_ERR_NULL_POINTER = 5,           // 🚫 Null pointer
    PWM_ERR_OUT_OF_MEMORY = 6,          // 💾 Out of memory
    PWM_ERR_INVALID_CHANNEL = 7,        // 🔍 Invalid PWM channel
    PWM_ERR_CHANNEL_BUSY = 8,           // 🔄 Channel already in use
    PWM_ERR_CHANNEL_NOT_AVAILABLE = 9,  // 🚫 Channel not available
    PWM_ERR_INSUFFICIENT_CHANNELS = 10, // 📉 Insufficient channels
    PWM_ERR_INVALID_FREQUENCY = 11,     // 📻 Invalid frequency
    PWM_ERR_FREQUENCY_TOO_HIGH = 12,    // 📈 Frequency too high
    PWM_ERR_FREQUENCY_TOO_LOW = 13,     // 📉 Frequency too low
    PWM_ERR_RESOLUTION_NOT_SUPPORTED = 14, // 🎯 Resolution not supported
    PWM_ERR_INVALID_DUTY_CYCLE = 15,    // 🎛️ Invalid duty cycle
    PWM_ERR_DUTY_OUT_OF_RANGE = 16,     // 📏 Duty cycle out of range
    PWM_ERR_HARDWARE_FAULT = 17,        // 💥 Hardware fault
    PWM_ERR_TIMER_CONFLICT = 18,        // ⏱️ Timer resource conflict
    PWM_ERR_PIN_CONFLICT = 19,          // 🔌 Pin already in use
    PWM_ERR_COMMUNICATION_TIMEOUT = 20, // ⏰ Communication timeout
    PWM_ERR_COMMUNICATION_FAILURE = 21, // 📡 Communication failure
    PWM_ERR_DEVICE_NOT_RESPONDING = 22, // 🔇 Device not responding
    PWM_ERR_INVALID_DEVICE_ID = 23,     // 🆔 Invalid device ID
    PWM_ERR_UNSUPPORTED_OPERATION = 24  // 🚫 Unsupported operation
};
```

### Statistics Structure

```cpp
struct hf_pwm_statistics_t {
    hf_u32_t duty_updates_count;        // 🔄 Total duty cycle updates
    hf_u32_t frequency_changes_count;   // 📻 Total frequency changes
    hf_u32_t fade_operations_count;     // 🌟 Total fade operations
    hf_u32_t error_count;               // ❌ Total error count
    hf_u32_t channel_enables_count;     // ✅ Total channel enable operations
    hf_u32_t channel_disables_count;    // ❌ Total channel disable operations
};
```

## Class Interface

```cpp
class BasePwm {
public:
    // 🔧 Lifecycle management
    virtual ~BasePwm() noexcept = default;
    virtual hf_pwm_err_t Initialize() noexcept = 0;
    virtual hf_pwm_err_t Deinitialize() noexcept = 0;
    bool IsInitialized() const noexcept;
    bool EnsureInitialized() noexcept;
    bool EnsureDeinitialized() noexcept;

    // 📡 Channel management
    virtual hf_pwm_err_t EnableChannel(hf_channel_id_t channel_id) noexcept = 0;
    virtual hf_pwm_err_t DisableChannel(hf_channel_id_t channel_id) noexcept = 0;
    virtual bool IsChannelEnabled(hf_channel_id_t channel_id) const noexcept = 0;

    // 🎛️ PWM control
    virtual hf_pwm_err_t SetDutyCycle(hf_channel_id_t channel_id, float duty_cycle) noexcept = 0;
virtual float GetDutyCycle(hf_channel_id_t channel_id) const noexcept = 0;
    virtual hf_pwm_err_t SetFrequency(hf_channel_id_t channel_id, hf_frequency_hz_t frequency) noexcept = 0;
    virtual hf_pwm_err_t GetFrequency(hf_channel_id_t channel_id, hf_frequency_hz_t& frequency) const noexcept = 0;

    // 🌟 Advanced features
    virtual hf_pwm_err_t StartFade(hf_channel_id_t channel_id, float target_duty_percent, 
                                  hf_time_t fade_time_ms) noexcept = 0;
    virtual hf_pwm_err_t StopFade(hf_channel_id_t channel_id) noexcept = 0;
    virtual bool IsFading(hf_channel_id_t channel_id) const noexcept = 0;

    // 📊 Information and diagnostics
    virtual hf_u8_t GetMaxChannels() const noexcept = 0;
    virtual bool IsChannelAvailable(hf_channel_id_t channel_id) const noexcept = 0;
    virtual hf_pwm_err_t GetStatistics(hf_pwm_statistics_t& stats) const noexcept = 0;
    virtual hf_pwm_err_t ResetStatistics() noexcept = 0;
};
```

## Core Methods

### Initialization

```cpp
bool EnsureInitialized() noexcept;
```
**Purpose:** 🚀 Lazy initialization - automatically initializes PWM if not already done  
**Returns:** `true` if successful, `false` on failure  
**Usage:** Call before any PWM operations

### Channel Control

```cpp
hf_pwm_err_t EnableChannel(hf_channel_id_t channel_id) noexcept;
hf_pwm_err_t DisableChannel(hf_channel_id_t channel_id) noexcept;
bool IsChannelEnabled(hf_channel_id_t channel_id) const noexcept;
```
**Purpose:** 🎛️ Enable/disable individual PWM channels  
**Parameters:** Channel ID (0-based indexing)  
**Returns:** Error code or boolean status

### Duty Cycle Control

```cpp
hf_pwm_err_t SetDutyCycle(hf_channel_id_t channel_id, float duty_percent) noexcept;
hf_pwm_err_t GetDutyCycle(hf_channel_id_t channel_id, float& duty_percent) const noexcept;
```
**Purpose:** 🎯 Set/get PWM duty cycle as percentage (0.0 - 100.0)  
**Parameters:** 
- `channel_id` - Target PWM channel
- `duty_percent` - Duty cycle percentage (0.0 = 0%, 100.0 = 100%)

### Frequency Control

```cpp
hf_pwm_err_t SetFrequency(hf_channel_id_t channel_id, hf_frequency_hz_t frequency) noexcept;
hf_pwm_err_t GetFrequency(hf_channel_id_t channel_id, hf_frequency_hz_t& frequency) const noexcept;
```
**Purpose:** ⚡ Set/get PWM frequency in Hz  
**Parameters:**
- `channel_id` - Target PWM channel  
- `frequency` - Frequency in Hz

### Fade Operations

```cpp
hf_pwm_err_t StartFade(hf_channel_id_t channel_id, float target_duty_percent, 
                      hf_time_t fade_time_ms) noexcept;
hf_pwm_err_t StopFade(hf_channel_id_t channel_id) noexcept;
bool IsFading(hf_channel_id_t channel_id) const noexcept;
```
**Purpose:** 🌅 Smooth transitions between duty cycle values  
**Parameters:**
- `target_duty_percent` - Target duty cycle (0.0 - 100.0)
- `fade_time_ms` - Fade duration in milliseconds

## Usage Examples

### Basic Motor Speed Control

```cpp
#include "inc/mcu/esp32/EspPwm.h"

// 🏗️ Create PWM instance for motor control
EspPwm motor_pwm;

void setup_motor_control() {
    // 🚀 Initialize PWM system
    if (!motor_pwm.EnsureInitialized()) {
        printf("❌ Failed to initialize PWM\n");
        return;
    }
    
    // 📡 Enable channel 0 for motor speed control
    hf_pwm_err_t result = motor_pwm.EnableChannel(0);
    if (result != hf_pwm_err_t::PWM_SUCCESS) {
        printf("❌ Failed to enable PWM channel: %s\n", HfPwmErrToString(result));
        return;
    }
    
    // 📻 Set PWM frequency to 20kHz (typical for motor control)
    result = motor_pwm.SetFrequency(0, 20000);
    if (result != hf_pwm_err_t::PWM_SUCCESS) {
        printf("❌ Failed to set frequency: %s\n", HfPwmErrToString(result));
        return;
    }
    
    printf("✅ Motor PWM initialized successfully\n");
}

void set_motor_speed(float speed_percent) {
    // 🎛️ Set motor speed (0-100%)
    hf_pwm_err_t result = motor_pwm.SetDutyCycle(0, speed_percent);
    if (result == hf_pwm_err_t::PWM_SUCCESS) {
        printf("🏎️ Motor speed set to %.1f%%\n", speed_percent);
    } else {
        printf("❌ Failed to set motor speed: %s\n", HfPwmErrToString(result));
    }
}

void motor_control_demo() {
    setup_motor_control();
    
    // 🚀 Gradually increase motor speed
    for (float speed = 0.0f; speed <= 100.0f; speed += 10.0f) {
        set_motor_speed(speed);
        vTaskDelay(pdMS_TO_TICKS(500));  // Wait 500ms
    }
    
    // 🛑 Stop motor
    set_motor_speed(0.0f);
}
```

### LED Dimming with Fade Effects

```cpp
#include "inc/mcu/esp32/EspPwm.h"

class SmartLED {
private:
    EspPwm led_pwm*;
    hf_channel_id_t channel*;
    
public:
    SmartLED(hf_channel_id_t channel) : channel*(channel) {}
    
    bool initialize() {
        // 🚀 Initialize PWM for LED control
        if (!led_pwm*.EnsureInitialized()) {
            printf("❌ Failed to initialize LED PWM\n");
            return false;
        }
        
        // 📡 Enable LED channel
        hf_pwm_err_t result = led_pwm*.EnableChannel(channel*);
        if (result != hf_pwm_err_t::PWM_SUCCESS) {
            printf("❌ Failed to enable LED channel: %s\n", HfPwmErrToString(result));
            return false;
        }
        
        // 📻 Set frequency to 1kHz (good for LED dimming)
        result = led_pwm*.SetFrequency(channel*, 1000);
        if (result != hf_pwm_err_t::PWM_SUCCESS) {
            printf("❌ Failed to set LED frequency: %s\n", HfPwmErrToString(result));
            return false;
        }
        
        printf("✅ Smart LED initialized on channel %u\n", channel*);
        return true;
    }
    
    void set_brightness(float brightness_percent) {
        // 💡 Set LED brightness instantly
        hf_pwm_err_t result = led_pwm*.SetDutyCycle(channel*, brightness_percent);
        if (result == hf_pwm_err_t::PWM_SUCCESS) {
            printf("💡 LED brightness set to %.1f%%\n", brightness_percent);
        } else {
            printf("❌ Failed to set brightness: %s\n", HfPwmErrToString(result));
        }
    }
    
    void fade_to(float target_brightness, hf_time_t fade_time_ms) {
        // 🌟 Start smooth fade to target brightness
        hf_pwm_err_t result = led_pwm*.StartFade(channel*, target_brightness, fade_time_ms);
        if (result == hf_pwm_err_t::PWM_SUCCESS) {
            printf("🌅 Starting fade to %.1f%% over %u ms\n", target_brightness, fade_time_ms);
        } else {
            printf("❌ Failed to start fade: %s\n", HfPwmErrToString(result));
        }
    }
    
    void breathing_effect() {
        printf("🫁 Starting breathing effect...\n");
        
        // 🌟 Fade in over 2 seconds
        fade_to(100.0f, 2000);
        vTaskDelay(pdMS_TO_TICKS(2500));  // Wait for fade + extra
        
        // 🌙 Fade out over 2 seconds
        fade_to(0.0f, 2000);
        vTaskDelay(pdMS_TO_TICKS(2500));  // Wait for fade + extra
    }
    
    bool is_fading() {
        return led_pwm*.IsFading(channel*);
    }
};
```

### Multi-Channel RGB LED Control

```cpp
class RGBController {
private:
    EspPwm rgb_pwm*;
    static constexpr hf_channel_id_t RED_CHANNEL = 0;
    static constexpr hf_channel_id_t GREEN_CHANNEL = 1;
    static constexpr hf_channel_id_t BLUE_CHANNEL = 2;
    
public:
    bool initialize() {
        // 🚀 Initialize RGB PWM controller
        if (!rgb_pwm*.EnsureInitialized()) {
            printf("❌ Failed to initialize RGB PWM\n");
            return false;
        }
        
        // 📡 Enable all RGB channels
        const hf_channel_id_t channels[] = {RED_CHANNEL, GREEN_CHANNEL, BLUE_CHANNEL};
        const char* colors[] = {"🔴 Red", "🟢 Green", "🔵 Blue"};
        
        for (int i = 0; i < 3; i++) {
            hf_pwm_err_t result = rgb_pwm*.EnableChannel(channels[i]);
            if (result != hf_pwm_err_t::PWM_SUCCESS) {
                printf("❌ Failed to enable %s channel: %s\n", colors[i], HfPwmErrToString(result));
                return false;
            }
            
            // 📻 Set frequency to 1kHz for all channels
            result = rgb_pwm*.SetFrequency(channels[i], 1000);
            if (result != hf_pwm_err_t::PWM_SUCCESS) {
                printf("❌ Failed to set %s frequency: %s\n", colors[i], HfPwmErrToString(result));
                return false;
            }
        }
        
        printf("🌈 RGB Controller initialized successfully\n");
        return true;
    }
    
    void set_rgb_color(float red_percent, float green_percent, float blue_percent) {
        // 🎨 Set RGB color components
        struct {
            hf_channel_id_t channel;
            float value;
            const char* name;
            const char* emoji;
        } components[] = {
            {RED_CHANNEL, red_percent, "Red", "🔴"},
            {GREEN_CHANNEL, green_percent, "Green", "🟢"},
            {BLUE_CHANNEL, blue_percent, "Blue", "🔵"}
        };
        
        printf("🎨 Setting RGB color: R=%.1f%%, G=%.1f%%, B=%.1f%%\n", 
               red_percent, green_percent, blue_percent);
        
        for (const auto& comp : components) {
            hf_pwm_err_t result = rgb_pwm*.SetDutyCycle(comp.channel, comp.value);
            if (result != hf_pwm_err_t::PWM_SUCCESS) {
                printf("❌ Failed to set %s %s: %s\n", comp.emoji, comp.name, HfPwmErrToString(result));
            }
        }
    }
    
    void color_demo() {
        printf("🌈 Starting RGB color demo...\n");
        
        // 🔴 Pure red
        set_rgb_color(100.0f, 0.0f, 0.0f);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // 🟢 Pure green  
        set_rgb_color(0.0f, 100.0f, 0.0f);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // 🔵 Pure blue
        set_rgb_color(0.0f, 0.0f, 100.0f);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // 🟡 Yellow (red + green)
        set_rgb_color(100.0f, 100.0f, 0.0f);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // 🟣 Magenta (red + blue)
        set_rgb_color(100.0f, 0.0f, 100.0f);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // 🟦 Cyan (green + blue)
        set_rgb_color(0.0f, 100.0f, 100.0f);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // ⚪ White (all colors)
        set_rgb_color(100.0f, 100.0f, 100.0f);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // ⚫ Off
        set_rgb_color(0.0f, 0.0f, 0.0f);
    }
    
    void rainbow_fade() {
        printf("🌈 Starting rainbow fade effect...\n");
        
        // Start fade operations for smooth color transitions
        rgb_pwm*.StartFade(RED_CHANNEL, 100.0f, 2000);
        vTaskDelay(pdMS_TO_TICKS(500));
        rgb_pwm*.StartFade(GREEN_CHANNEL, 100.0f, 2000);
        vTaskDelay(pdMS_TO_TICKS(500));
        rgb_pwm*.StartFade(BLUE_CHANNEL, 100.0f, 2000);
    }
};
```

### Servo Motor Control

```cpp
class ServoController {
private:
    EspPwm servo_pwm*;
    hf_channel_id_t channel*;
    static constexpr float SERVO_MIN_DUTY = 2.5f;   // 2.5% duty cycle (0 degrees)
    static constexpr float SERVO_MAX_DUTY = 12.5f;  // 12.5% duty cycle (180 degrees)
    static constexpr hf_frequency_hz_t SERVO_FREQ = 50; // 50Hz for standard servos
    
public:
    ServoController(hf_channel_id_t channel) : channel*(channel) {}
    
    bool initialize() {
        // 🚀 Initialize servo PWM
        if (!servo_pwm*.EnsureInitialized()) {
            printf("❌ Failed to initialize servo PWM\n");
            return false;
        }
        
        // 📡 Enable servo channel
        hf_pwm_err_t result = servo_pwm*.EnableChannel(channel*);
        if (result != hf_pwm_err_t::PWM_SUCCESS) {
            printf("❌ Failed to enable servo channel: %s\n", HfPwmErrToString(result));
            return false;
        }
        
        // 📻 Set servo frequency to 50Hz
        result = servo_pwm*.SetFrequency(channel*, SERVO_FREQ);
        if (result != hf_pwm_err_t::PWM_SUCCESS) {
            printf("❌ Failed to set servo frequency: %s\n", HfPwmErrToString(result));
            return false;
        }
        
        // 🎯 Set to center position (90 degrees)
        set_angle(90.0f);
        
        printf("🤖 Servo controller initialized on channel %u\n", channel*);
        return true;
    }
    
    void set_angle(float angle_degrees) {
        // 🎯 Convert angle to PWM duty cycle
        // Servo range: 0-180 degrees maps to 2.5%-12.5% duty cycle
        if (angle_degrees < 0.0f) angle_degrees = 0.0f;
        if (angle_degrees > 180.0f) angle_degrees = 180.0f;
        
        float duty_percent = SERVO_MIN_DUTY + (angle_degrees / 180.0f) * (SERVO_MAX_DUTY - SERVO_MIN_DUTY);
        
        hf_pwm_err_t result = servo_pwm*.SetDutyCycle(channel*, duty_percent);
        if (result == hf_pwm_err_t::PWM_SUCCESS) {
            printf("🤖 Servo angle set to %.1f° (%.2f%% duty)\n", angle_degrees, duty_percent);
        } else {
            printf("❌ Failed to set servo angle: %s\n", HfPwmErrToString(result));
        }
    }
    
    void smooth_move_to(float target_angle, hf_time_t move_time_ms) {
        // 🌟 Smooth movement to target angle
        float current_duty, target_duty;
        servo_pwm*.GetDutyCycle(channel*, current_duty);
        
        // Calculate target duty cycle
        if (target_angle < 0.0f) target_angle = 0.0f;
        if (target_angle > 180.0f) target_angle = 180.0f;
        target_duty = SERVO_MIN_DUTY + (target_angle / 180.0f) * (SERVO_MAX_DUTY - SERVO_MIN_DUTY);
        
        hf_pwm_err_t result = servo_pwm*.StartFade(channel*, target_duty, move_time_ms);
        if (result == hf_pwm_err_t::PWM_SUCCESS) {
            printf("🌟 Servo smoothly moving to %.1f° over %u ms\n", target_angle, move_time_ms);
        } else {
            printf("❌ Failed to start smooth movement: %s\n", HfPwmErrToString(result));
        }
    }
    
    void sweep_demo() {
        printf("🔄 Starting servo sweep demo...\n");
        
        // 🔄 Sweep from 0 to 180 degrees
        for (float angle = 0.0f; angle <= 180.0f; angle += 30.0f) {
            set_angle(angle);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
        // 🔄 Sweep back from 180 to 0 degrees
        for (float angle = 180.0f; angle >= 0.0f; angle -= 30.0f) {
            set_angle(angle);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
        // 🎯 Return to center
        set_angle(90.0f);
    }
};
```

## Performance and Diagnostics

### Statistics Monitoring

```cpp
void monitor_pwm_performance(BasePwm& pwm) {
    hf_pwm_statistics_t stats;
    hf_pwm_err_t result = pwm.GetStatistics(stats);
    
    if (result == hf_pwm_err_t::PWM_SUCCESS) {
        printf("📊 PWM Performance Statistics:\n");
        printf("   🔄 Duty Updates: %u\n", stats.duty_updates_count);
        printf("   📻 Frequency Changes: %u\n", stats.frequency_changes_count);
        printf("   🌟 Fade Operations: %u\n", stats.fade_operations_count);
        printf("   ✅ Channel Enables: %u\n", stats.channel_enables_count);
        printf("   ❌ Channel Disables: %u\n", stats.channel_disables_count);
        printf("   ⚠️ Total Errors: %u\n", stats.error_count);
    }
}
```

## Error Handling Best Practices

### Comprehensive Error Checking

```cpp
hf_pwm_err_t safe_set_duty_cycle(BasePwm& pwm, hf_channel_id_t channel, float duty) {
    // ✅ Validate duty cycle range
    if (duty < 0.0f || duty > 100.0f) {
        printf("❌ Invalid duty cycle: %.2f%% (must be 0-100%%)\n", duty);
        return hf_pwm_err_t::PWM_ERR_DUTY_OUT_OF_RANGE;
    }
    
    // ✅ Check if channel is available
    if (!pwm.IsChannelAvailable(channel)) {
        printf("❌ Channel %u not available\n", channel);
        return hf_pwm_err_t::PWM_ERR_INVALID_CHANNEL;
    }
    
    // ✅ Ensure PWM is initialized
    if (!pwm.EnsureInitialized()) {
        printf("❌ Failed to initialize PWM\n");
        return hf_pwm_err_t::PWM_ERR_NOT_INITIALIZED;
    }
    
    // ✅ Enable channel if not already enabled
    if (!pwm.IsChannelEnabled(channel)) {
        hf_pwm_err_t result = pwm.EnableChannel(channel);
        if (result != hf_pwm_err_t::PWM_SUCCESS) {
            printf("❌ Failed to enable channel %u: %s\n", channel, HfPwmErrToString(result));
            return result;
        }
    }
    
    // 🎛️ Set duty cycle
    return pwm.SetDutyCycle(channel, duty);
}
```

## Performance Considerations

### Optimization Tips

- **🔢 Channel Limits** - Check `GetMaxChannels()` before allocating channels
- **📻 Frequency Ranges** - Respect hardware frequency limitations
- **🎯 Resolution Trade-offs** - Higher frequencies may reduce duty cycle resolution
- **🌟 Fade Performance** - Hardware-based fading is faster than software loops
- **💾 Memory Usage** - Use lazy initialization to save memory

### Typical Performance Ranges

| **Hardware** | **Channels** | **Frequency Range** | **Resolution** |

|--------------|--------------|---------------------|----------------|

| **ESP32-C6 LEDC** | 8 | 1Hz - 40MHz | 1-20 bits |

| **External PWM ICs** | 4-16 | 1Hz - 1.5MHz | 8-16 bits |

| **Motor Controllers** | 2-6 | 1kHz - 100kHz | 10-16 bits |

## Thread Safety

The `BasePwm` class is **not thread-safe**.
For concurrent access,
use appropriate synchronization or consider thread-safe wrapper implementations.

## Related Documentation

- **[EspPwm API Reference](../esp_api/EspPwm.md)** - ESP32-C6 PWM implementation
- **[BaseGpio API Reference](BaseGpio.md)** - GPIO interface for PWM output pins
- **[HardwareTypes Reference](HardwareTypes.md)** - Platform-agnostic type definitions

---


