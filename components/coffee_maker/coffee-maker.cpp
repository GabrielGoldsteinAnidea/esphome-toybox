#include "coffee-maker.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <string>

namespace esphome {
namespace coffee_maker {

static const char *TAG = "coffee_maker";

// Interrupt service routine wrappers
static void IRAM_ATTR clock_isr(CoffeeMaker *arg) {
  if (arg != nullptr) {
    arg->handle_clock_interrupt();
  }
}

static void IRAM_ATTR strobe_isr(CoffeeMaker *arg) {
  if (arg != nullptr) {
    arg->handle_strobe_interrupt();
  }
}

void CoffeeMakerEnableSwitch::write_state(bool state) {
  if (parent_ != nullptr) {
    parent_->set_enabled(state);
  }
  this->publish_state(state);
}

void CoffeeMaker::setup() {
  ESP_LOGCONFIG(TAG, "Setup Coffee Maker at I2C address 0x%02X", this->get_i2c_address());
  ESP_LOGCONFIG(TAG, "GPIO Configuration:");
  LOG_PIN("  Clock: ", gpio_clock_);
  LOG_PIN("  Data: ", gpio_data_);
  LOG_PIN("  Strobe: ", gpio_strobe_);
  LOG_PIN("  Buttons A: ", buttons_a_.pin());
  LOG_PIN("  Buttons B: ", buttons_b_.pin());
  LOG_PIN("  Buttons C: ", button_onoff_.pin());

  // Initialize receive buffer and frame assembly
  rx_write_index_ = 0;
  rx_read_index_ = 0;
  current_byte_ = 0;
  bit_count_ = 0;
  byte_count_ = 0;
  
  // Initialize protocol state
  frame_byte_count_ = 0;
  frame_complete_ = 0;
  
  // Initialize LED state averaging
  for (int i = 0; i < 10; i++) {
    led_state_count_[i] = 0;
    led_state_[i] = 0;
  }
  averaging_sample_count_ = 0;
  
  // Initialize button command state
  pending_button_command_ = 0;
  button_strobe_counter_ = 0;
  button_active_ = 0;

  ESP_LOGCONFIG(TAG, "Coffee Maker interface initialized");
  
  if (gpio_clock_ == nullptr || gpio_data_ == nullptr || gpio_strobe_ == nullptr || button_onoff_.pin() == nullptr) {
    ESP_LOGE(TAG, "Missing required GPIO pin configuration");
    this->mark_failed();
    return;
  }

  gpio_clock_->setup();
  gpio_data_->setup();
  gpio_strobe_->setup();
  
  button_onoff_.setup();

  gpio_clock_->pin_mode(gpio::FLAG_INPUT);
  gpio_data_->pin_mode(gpio::FLAG_INPUT);// | gpio::FLAG_PULLUP);
  gpio_strobe_->pin_mode(gpio::FLAG_INPUT);


  button_onoff_.pin_mode(gpio::FLAG_INPUT);

  isr_clock_ = gpio_clock_->to_isr();
  isr_data_ = gpio_data_->to_isr();
  isr_strobe_ = gpio_strobe_->to_isr();


  this->set_enabled(false);

}


void CoffeeMaker::update() {
  
  //ESP_LOGD(TAG, "Data pin %d", gpio_data_->digital_read());
  
  if (!enabled_) {
    return;
  }

  // Process received frames from interrupt handler
  // From sample.ino: 9-byte frames received, parsed for LED states
  
  if (frame_complete_) {
    // Frame received: current_frame_[0..8]
    // Parse according to sample.ino protocol:
    //
    // data[0]: Control byte (usually 0 or sentinel)
    // data[1]: Block selector (0 = LEDs 1-3, 1 = LEDs 5-9)
    // data[2]: Padding/unknown
    // data[3..7]: Actual sensor bits
    // data[8]: Extra/unused
    
    frame_complete_ = 0;
    
    // From sample.ino: LED state mapping
    // When data[1] == 0:
    //   LED[3] = data[5], LED[2] = data[6], LED[1] = data[7]
    // When data[1] == 1:
    //   LED[9] = data[3], LED[8] = data[4], LED[7] = data[5], LED[6] = data[6], LED[5] = data[7]
    
    if (frame_log_pending_) {
      frame_log_pending_ = 0;
      ESP_LOGD(TAG, "Frame received: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
               current_frame_[0], current_frame_[1], current_frame_[2], current_frame_[3],
               current_frame_[4], current_frame_[5], current_frame_[6], current_frame_[7],
               current_frame_[8]);
    }
  


    #if 0   
    // Temporary direct mapping (without averaging) for testing:
    // Parse LED states from the frame
    uint8_t data_block = current_frame_[1];
    
    if (data_block == 0) {
      // Block 1: LEDs 1-3 (bits from data[5], data[6], data[7])
      // LED1 = data[7], LED2 = data[6], LED3 = data[5]
      one_cup_ready_state_ = (current_frame_[7] != 0);
      two_cup_ready_state_ = (current_frame_[6] != 0);
      hot_water_state_ = (current_frame_[5] != 0);
    } else if (data_block == 1) {
      // Block 2: LEDs 5-9 (bits from data[3..7])
      // LED5 = data[7], LED6 = data[6], LED7 = data[5], LED8 = data[4], LED9 = data[3]
      water_empty_state_ = (current_frame_[7] != 0);
      grounds_full_state_ = (current_frame_[6] != 0);
      error_state_ = (current_frame_[5] != 0);
      decalcification_needed_state_ = (current_frame_[4] != 0);
      grind_disabled_state_ = (current_frame_[3] != 0);
    }
    
    // TODO: Implement state averaging like sample.ino
    // sample.ino averages LED states over 500 strobe cycles (~1 second)
    // to distinguish between ON (always 1), OFF (always 0), and FLASHING (alternating)
    #endif

    memset(current_frame_, 0, sizeof(current_frame_));

    gpio_clock_->attach_interrupt(clock_isr, this, gpio::INTERRUPT_RISING_EDGE);
    gpio_strobe_->attach_interrupt(strobe_isr, this, gpio::INTERRUPT_RISING_EDGE);

  }
  
  #if 0
  // Publish sensor states to ESPHome entities
  if (one_cup_ready_ != nullptr) {
    one_cup_ready_->publish_state(one_cup_ready_state_);
  }
  if (two_cup_ready_ != nullptr) {
    two_cup_ready_->publish_state(two_cup_ready_state_);
  }
  if (hot_water_ != nullptr) {
    hot_water_->publish_state(hot_water_state_);
  }
  if (water_empty_ != nullptr) {
    water_empty_->publish_state(water_empty_state_);
  }
  if (grounds_full_ != nullptr) {
    grounds_full_->publish_state(grounds_full_state_);
  }
  if (error_ != nullptr) {
    error_->publish_state(error_state_);
  }
  if (decalcification_needed_ != nullptr) {
    decalcification_needed_->publish_state(decalcification_needed_state_);
  }
  if (grind_disabled_ != nullptr) {
    grind_disabled_->publish_state(grind_disabled_state_);
  }
  
  // TODO: Publish analog sensor states (coffee_quantity, coffee_flavor)
  // These would need to be decoded from the protocol
  // Possibly from the shift register data or a separate I2C register
  if (coffee_quantity_ != nullptr) {
    coffee_quantity_->publish_state(coffee_quantity_value_);
  }
  if (coffee_flavor_ != nullptr) {
    coffee_flavor_->publish_state(coffee_flavor_value_);
  }
  #endif
}


void CoffeeMaker::dump_config() {
  ESP_LOGCONFIG(TAG, "Coffee Maker:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Coffee Maker communication failed!");
  }

  LOG_UPDATE_INTERVAL(this);
  
  if (one_cup_ready_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Binary Sensor: One Cup Ready");
  }
  if (two_cup_ready_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Binary Sensor: Two Cup Ready");
  }
  if (hot_water_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Binary Sensor: Hot Water");
  }
  if (water_empty_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Binary Sensor: Water Empty");
  }
  if (grounds_full_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Binary Sensor: Grounds Full");
  }
  if (error_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Binary Sensor: Error");
  }
  if (decalcification_needed_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Binary Sensor: Decalcification Needed");
  }
  if (grind_disabled_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Binary Sensor: Grind Disabled");
  }
  if (coffee_quantity_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Sensor: Coffee Quantity");
  }
  if (coffee_flavor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Sensor: Coffee Flavor");
  }
}

float CoffeeMaker::get_setup_priority() const { 
  return setup_priority::DATA; 
}

// Setter implementation stubs for controlling the coffee maker
// From sample.ino analysis: Button commands use button line synchronization
// Buttons (1-6) are mapped to multiplexer sequence positions (0-9)
// Button lines are open-drain and driven LOW during the correct sequence to trigger button press

void CoffeeMaker::set_onoff(bool state) {
  // TODO: Implement power on/off command
  // From sample.ino: button_int = 1 (maps to sequence 4)
  // This is the "turn on" button
  // 
  // Implementation: Queue button press, will be executed when
  // multiplexer reaches the correct sequence in strobe_isr()
  if (!enabled_) {
    ESP_LOGD(TAG, "Ignored power command while disabled");
    return;
  }
  if (state) {
    pending_button_command_ = 1;  // Turn on button
    ESP_LOGD(TAG, "Queuing power ON command");
  } else {
    // Power off may use a different button or method
    // NOTE: sample.ino doesn't show power-off mechanism
    ESP_LOGD(TAG, "Power OFF not implemented");
  }
}

void CoffeeMaker::set_one_cup_request() {
  // TODO: Implement one cup brew request
  // From sample.ino: sequence mapping shows button ID for one cup
  // mapping[4] = sequence 4 (one cup ready from reading button_int)
  // Implementation: Queue button 4 for execution
  if (!enabled_) {
    ESP_LOGD(TAG, "Ignored one cup request while disabled");
    return;
  }
  pending_button_command_ = 4;
  ESP_LOGD(TAG, "Queuing one cup brew request");
}

void CoffeeMaker::set_two_cups_request() {
  // TODO: Implement two cups brew request
  // From sample.ino: mapping[5] = sequence 5
  if (!enabled_) {
    ESP_LOGD(TAG, "Ignored two cups request while disabled");
    return;
  }
  pending_button_command_ = 5;
  ESP_LOGD(TAG, "Queuing two cups brew request");
}

void CoffeeMaker::set_hot_water_request() {
  // TODO: Implement hot water dispense request
  // From sample.ino: mapping[3] = sequence 3 (steam/hot water)
  if (!enabled_) {
    ESP_LOGD(TAG, "Ignored hot water request while disabled");
    return;
  }
  pending_button_command_ = 3;
  ESP_LOGD(TAG, "Queuing hot water request");
}

// Interrupt Handler for Clock signal
void CoffeeMaker::handle_clock_interrupt() {
  if (!enabled_) {
    return;
  }

  // Implement clock interrupt handler using ESPHome GPIO abstraction
  // On each CLOCK rising edge, read 1 bit from DATA pin and assemble into byte
  // 
  // From sample.ino: data[clock_pulse] = pinReadFast(DATA_PIN);
  // We accumulate bits into current_byte_ and store when we have 8 bits
  

  if (gpio_data_ != nullptr) {
    bool data_bit = gpio_data_->digital_read();//isr_data_.digital_read();
    
    // Assemble byte from LSB to MSB (bit 0 first) - matching sample.ino
    current_byte_ |= (data_bit << bit_count_);
    bit_count_++;
    
    // When we have 8 bits, store the byte in current frame
    if (bit_count_ >= 8) {
      //ESP_LOGD(TAG, "Sub: %02X frame_byte_count_ %d", current_byte_, frame_byte_count_);

      if (frame_byte_count_ < 9) {
        current_frame_[frame_byte_count_] = current_byte_;
        frame_byte_count_++;
        

      }
      current_byte_ = 0;
      bit_count_ = 0;
      //ESP_LOGD(TAG, "clk bit_count_ reset");
    }
  }
}

// Interrupt Handler for Strobe signal  
void CoffeeMaker::handle_strobe_interrupt() {
  if (!enabled_) {
    return;
  }



  // Implement strobe interrupt handler using ESPHome GPIO abstraction
  // STROBE signals end of frame and start of multiplexer read
  // 
  // From sample.ino:
  // 1. Increment sequence_number
  // 2. Handle any pending button line output for button press
  // 3. Read multiplexer state (S0, S1, S2) from STROBE/DATA/CLOCK pins
  // 4. Indicates 9 bytes of shift register data complete
  
  //ESP_LOGD(TAG, "STROBE");

  // NOTE: This is called after all 8 bits of the 9th byte are received
  // The 35-cycle delay loop in sample.ino appears to be for debouncing
  // the multiplexer lines which are asynchronous to the clock/data signals
  
  //return;
  
  // Reset byte and bit coutner
  if (bit_count_ > 0 && frame_byte_count_ < 9) {
    // current_frame_[frame_byte_count_] = current_byte_;
    frame_byte_count_++;
    current_byte_ = 0;
    bit_count_ = 0;
    //ESP_LOGD(TAG, "stb bit_count_ reset");
  }
  
  // // Ensure frame is marked complete
  // if (frame_byte_count_ > 0) {
  //   frame_complete_ = 1;
  // }

  // Complete frame received (9 bytes)
  if (frame_byte_count_ >= 9) {
    frame_complete_ = 1;
    frame_log_pending_ = 1;
    frame_byte_count_ = 0;

    // Disable receive until we can process frame
    gpio_clock_->detach_interrupt();
    gpio_strobe_->detach_interrupt();
    //ESP_LOGD(TAG, "clk frame complete");
  }

  if (button_active_) {
    if (button_strobe_counter_++ > 10) {
      button_active_ = 0;
      pending_button_command_ = 0;
      button_strobe_counter_ = 0;

  
      #if 0

      buttons_c_.set_active_isr(true);
      #endif
    } else {
      
      #if 0

      buttons_c_.set_active_isr(false);
      #endif
  }
  

  
  } else if (pending_button_command_ > 0) {
    button_active_ = 1;
    button_strobe_counter_ = 0;
  }
  
  // TODO: Read multiplexer state (S0, S1, S2) from GPIO pins
  // From sample.ino's 35-cycle loop:
  // S0 = (pinReadFast(STROBE_PIN) || S0);  // Read from STROBE pin
  // if (S0 == 1) {
  //   S1 = (pinReadFast(DATA_PIN) || S1);  // Read from DATA pin
  //   S2 = (pinReadFast(CLOCK_PIN) || S2); // Read from CLOCK pin
  // }
  // The multiplexer selects which output to read - used for button synchronization
  // For now, skipping this as it's not critical for basic sensor reading


}


void CoffeeMaker::set_enabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }
  enabled_ = enabled;

  drive_buttons_(false);

  if (!enabled_) {
    pending_button_command_ = 0;
    button_active_ = 0;
    button_strobe_counter_ = 0;
    frame_complete_ = 0;
    frame_log_pending_ = 0;

    gpio_clock_->detach_interrupt();
    gpio_strobe_->detach_interrupt();

  }else{

    gpio_clock_->attach_interrupt(clock_isr, this, gpio::INTERRUPT_RISING_EDGE);
    gpio_strobe_->attach_interrupt(strobe_isr, this, gpio::INTERRUPT_RISING_EDGE);
    

  }
}

void CoffeeMaker::drive_buttons_(bool active) {

  button_onoff_.set_active(active);
}

}  // namespace coffee_maker
}  // namespace esphome
