#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace coffee_maker {

#define COFFEE_MAKER_DEFAULT_ADDR (0x18)
#define RX_BUFFER_SIZE 64

// Forward declaration for interrupt handler
class CoffeeMaker;

class CoffeeMakerEnableSwitch : public switch_::Switch {
 public:
  void set_parent(CoffeeMaker *parent) { parent_ = parent; }

 protected:
  void write_state(bool state) override;

 private:
  CoffeeMaker *parent_{nullptr};
};

class InvertedOpenDrainPin {
 public:
  void set_pin(InternalGPIOPin *pin) {
    pin_ = pin;
    if (pin_ != nullptr) {
      isr_ = pin_->to_isr();
      isr_ready_ = true;
    }
  }

  InternalGPIOPin *pin() const { return pin_; }

  void setup() {
    if (pin_ != nullptr) {
      pin_->setup();
    }
  }

  void pin_mode(gpio::Flags flags) {
    if (pin_ != nullptr) {
      pin_->pin_mode(flags);
    }
  }

  void set_active(bool value) {
    if (pin_ == nullptr) {
      return;
    }
    if (value) {
      pin_->pin_mode(gpio::FLAG_OUTPUT);
      pin_->digital_write(true);
    } else {
      pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN);
    }
  }

  void set_active_isr(bool value) {
    if (!isr_ready_) {
      return;
    }
    if (value) {
      isr_.pin_mode(gpio::FLAG_OUTPUT);
      isr_.digital_write(true);
    } else {
      isr_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN);
    }
  }

 private:
  InternalGPIOPin *pin_{nullptr};
  ISRInternalGPIOPin isr_;
  bool isr_ready_{false};
};

class CoffeeMaker : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  float get_setup_priority() const override;

  // Binary sensor setters
  void set_one_cup_ready(binary_sensor::BinarySensor *sensor) { one_cup_ready_ = sensor; }
  void set_two_cup_ready(binary_sensor::BinarySensor *sensor) { two_cup_ready_ = sensor; }
  void set_hot_water(binary_sensor::BinarySensor *sensor) { hot_water_ = sensor; }
  void set_water_empty(binary_sensor::BinarySensor *sensor) { water_empty_ = sensor; }
  void set_grounds_full(binary_sensor::BinarySensor *sensor) { grounds_full_ = sensor; }
  void set_error(binary_sensor::BinarySensor *sensor) { error_ = sensor; }
  void set_decalcification_needed(binary_sensor::BinarySensor *sensor) { decalcification_needed_ = sensor; }
  void set_grind_disabled(binary_sensor::BinarySensor *sensor) { grind_disabled_ = sensor; }

  // Sensor setters
  void set_coffee_quantity(sensor::Sensor *sensor) { coffee_quantity_ = sensor; }
  void set_coffee_flavor(sensor::Sensor *sensor) { coffee_flavor_ = sensor; }

  // Getter stubs for internal state (to be implemented based on hardware interface)
  bool get_one_cup_ready() const { return one_cup_ready_state_; }
  bool get_two_cup_ready() const { return two_cup_ready_state_; }
  bool get_hot_water() const { return hot_water_state_; }
  bool get_water_empty() const { return water_empty_state_; }
  bool get_grounds_full() const { return grounds_full_state_; }
  bool get_error() const { return error_state_; }
  bool get_decalcification_needed() const { return decalcification_needed_state_; }
  bool get_grind_disabled() const { return grind_disabled_state_; }
  float get_coffee_quantity() const { return coffee_quantity_value_; }
  float get_coffee_flavor() const { return coffee_flavor_value_; }

  // Setter stubs for controlling the coffee maker (to be implemented based on hardware interface)
  void set_onoff(bool state);
  void set_one_cup_request();
  void set_two_cups_request();
  void set_hot_water_request();

  // GPIO Pin configuration setters
  void set_pin_clock(InternalGPIOPin *pin) { gpio_clock_ = pin; }
  void set_pin_data(InternalGPIOPin *pin) { gpio_data_ = pin; }
  void set_pin_strobe(InternalGPIOPin *pin) { gpio_strobe_ = pin; }
  void set_pin_buttons_a(InternalGPIOPin *pin) { buttons_a_.set_pin(pin); }
  void set_pin_buttons_b(InternalGPIOPin *pin) { buttons_b_.set_pin(pin); }
  void set_pin_button_onoff(InternalGPIOPin *pin) { button_onoff_.set_pin(pin); }
  void set_enable_switch(CoffeeMakerEnableSwitch *sw) { enable_switch_ = sw; }

  // Interrupt handlers
  void handle_clock_interrupt();
  void handle_strobe_interrupt();
  void set_enabled(bool enabled);

 private:
  // Data reception variables
  uint8_t rx_buffer_[RX_BUFFER_SIZE];
  uint16_t rx_write_index_{0};
  uint16_t rx_read_index_{0};

  // Bit assembly variables
  uint8_t current_byte_{0};
  uint8_t bit_count_{0};
  uint8_t byte_count_{0};
  void drive_buttons_(bool active);

 protected:
  // Binary sensors
  binary_sensor::BinarySensor *one_cup_ready_{nullptr};
  binary_sensor::BinarySensor *two_cup_ready_{nullptr};
  binary_sensor::BinarySensor *hot_water_{nullptr};
  binary_sensor::BinarySensor *water_empty_{nullptr};
  binary_sensor::BinarySensor *grounds_full_{nullptr};
  binary_sensor::BinarySensor *error_{nullptr};
  binary_sensor::BinarySensor *decalcification_needed_{nullptr};
  binary_sensor::BinarySensor *grind_disabled_{nullptr};

  // Sensors
  sensor::Sensor *coffee_quantity_{nullptr};
  sensor::Sensor *coffee_flavor_{nullptr};

  // Internal state variables (to be synchronized with hardware)
  bool one_cup_ready_state_{false};
  bool two_cup_ready_state_{false};
  bool hot_water_state_{false};
  bool water_empty_state_{false};
  bool grounds_full_state_{false};
  bool error_state_{false};
  bool decalcification_needed_state_{false};
  bool grind_disabled_state_{false};
  float coffee_quantity_value_{0.0f};
  float coffee_flavor_value_{0.0f};

  // GPIO pointers
  InternalGPIOPin *gpio_clock_{nullptr};
  InternalGPIOPin *gpio_data_{nullptr};
  InternalGPIOPin *gpio_strobe_{nullptr};
  InvertedOpenDrainPin buttons_a_;
  InvertedOpenDrainPin buttons_b_;
  InvertedOpenDrainPin button_onoff_;
  ISRInternalGPIOPin isr_clock_;
  ISRInternalGPIOPin isr_data_;
  ISRInternalGPIOPin isr_strobe_;
  CoffeeMakerEnableSwitch *enable_switch_{nullptr};

  // Protocol-specific state variables (from sample.ino analysis)
  uint8_t current_frame_[9];       // Store complete 9-byte frame
  uint8_t frame_byte_count_{0};    // Bytes received in current frame
  uint8_t frame_complete_{0};      // Flag indicating frame ready for parsing
  uint8_t frame_log_pending_{0};   // Log a complete frame outside ISR
  
  // LED state averaging (from sample.ino - over 500 strobe cycles)
  uint16_t led_state_count_[10];   // Counter for each LED state
  uint8_t led_state_[10];          // 0=OFF, 1=ON, 2=FLASHING
  uint16_t averaging_sample_count_{0};  // Sample counter for averaging window
  
  // Button command state
  uint8_t pending_button_command_{0};  // Button number pending execution (1-6)
  uint8_t button_strobe_counter_{0};   // Counter for button press duration
  uint8_t button_active_{0};           // Button press currently active
  bool enabled_{false};
};

}  // namespace coffee_maker
}  // namespace esphome
