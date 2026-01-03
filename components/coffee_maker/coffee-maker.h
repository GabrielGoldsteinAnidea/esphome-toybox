#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace coffee_maker {

#define COFFEE_MAKER_DEFAULT_ADDR (0x18)
#define RX_BUFFER_SIZE 64

// Forward declaration for interrupt handler
class CoffeeMaker;

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
  void set_pin_clock(uint8_t pin) { pin_clock_ = pin; }
  void set_pin_data(uint8_t pin) { pin_data_ = pin; }
  void set_pin_strobe(uint8_t pin) { pin_strobe_ = pin; }
  void set_pin_zio(uint8_t pin) { pin_zio_ = pin; }

  // Interrupt handlers
  void handle_clock_interrupt();
  void handle_strobe_interrupt();
  void zio_write(bool state);
  bool zio_read();

 private:
  // Data reception variables
  uint8_t rx_buffer_[RX_BUFFER_SIZE];
  uint16_t rx_write_index_{0};
  uint16_t rx_read_index_{0};

  // Bit assembly variables
  uint8_t current_byte_{0};
  uint8_t bit_count_{0};
  uint8_t byte_count_{0};

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

  // GPIO Pin numbers
  uint8_t pin_clock_{255};
  uint8_t pin_data_{255};
  uint8_t pin_strobe_{255};
  uint8_t pin_zio_{255};

  // GPIO pointers
  GPIOPin *gpio_clock_{nullptr};
  GPIOPin *gpio_data_{nullptr};
  GPIOPin *gpio_strobe_{nullptr};
  GPIOPin *gpio_zio_{nullptr};};

}  // namespace coffee_maker
}  // namespace esphome