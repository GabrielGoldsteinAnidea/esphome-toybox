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

// Global pointer for interrupt handling
static CoffeeMaker *global_coffee_maker = nullptr;

// Interrupt service routine wrappers
static void IRAM_ATTR clock_isr() {
  if (global_coffee_maker != nullptr) {
    global_coffee_maker->handle_clock_interrupt();
  }
}

static void IRAM_ATTR strobe_isr() {
  if (global_coffee_maker != nullptr) {
    global_coffee_maker->handle_strobe_interrupt();
  }
}

void CoffeeMaker::setup() {
  ESP_LOGCONFIG(TAG, "Setup Coffee Maker at I2C address 0x%02X", this->get_i2c_address());
  ESP_LOGCONFIG(TAG, "GPIO Configuration:");
  ESP_LOGCONFIG(TAG, "  Clock: GPIO%u", pin_clock_);
  ESP_LOGCONFIG(TAG, "  Data: GPIO%u", pin_data_);
  ESP_LOGCONFIG(TAG, "  Strobe: GPIO%u", pin_strobe_);
  ESP_LOGCONFIG(TAG, "  Zio: GPIO%u", pin_zio_);

  // Store global pointer for ISR access
  global_coffee_maker = this;

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
  
  // TODO: Configure GPIO pins using ESPHome's native GPIO abstraction
  // From sample.ino:
  //   pinMode(CLOCK_PIN, INPUT);
  //   pinMode(DATA_PIN, INPUT);
  //   pinMode(STROBE_PIN, INPUT);
  //   pinMode(Z_PIN, INPUT);  // Input until button press needed
  //   attachInterrupt(CLOCK_PIN, clockPulse, RISING, 0);
  //   attachInterrupt(STROBE_PIN, strobePulse, RISING, 0);
  //
  // ESPHome equivalent:
  // 1. Create InternalGPIOPin objects from pin_clock_, pin_data_, etc.
  // 2. Attach ISRs using gpio namespace interrupt support
  // 3. Configure Z pin with open-drain capability
  // 4. NOTE: Z pin starts as INPUT, switches to OUTPUT when button press needed
  //
  // Implementation notes:
  // - CLOCK and STROBE need rising edge interrupt detection
  // - DATA and CLOCK pins read during STROBE ISR for multiplexer state
  // - Z pin open-drain control requires careful mode switching
}


void CoffeeMaker::update() {
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
    
    // TODO: Implement LED state parsing and averaging
    // For now, log the received frame for debugging
    ESP_LOGV(TAG, "Frame received: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             current_frame_[0], current_frame_[1], current_frame_[2], current_frame_[3],
             current_frame_[4], current_frame_[5], current_frame_[6], current_frame_[7],
             current_frame_[8]);
    
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
  }
  
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
// From sample.ino analysis: Button commands use Z pin synchronization
// Buttons (1-6) are mapped to multiplexer sequence positions (0-9)
// Z pin is driven LOW during the correct sequence to trigger button press

void CoffeeMaker::set_onoff(bool state) {
  // TODO: Implement power on/off command
  // From sample.ino: button_int = 1 (maps to sequence 4)
  // This is the "turn on" button
  // 
  // Implementation: Queue button press, will be executed when
  // multiplexer reaches the correct sequence in strobe_isr()
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
  pending_button_command_ = 4;
  ESP_LOGD(TAG, "Queuing one cup brew request");
}

void CoffeeMaker::set_two_cups_request() {
  // TODO: Implement two cups brew request
  // From sample.ino: mapping[5] = sequence 5
  pending_button_command_ = 5;
  ESP_LOGD(TAG, "Queuing two cups brew request");
}

void CoffeeMaker::set_hot_water_request() {
  // TODO: Implement hot water dispense request
  // From sample.ino: mapping[3] = sequence 3 (steam/hot water)
  pending_button_command_ = 3;
  ESP_LOGD(TAG, "Queuing hot water request");
}

// Interrupt Handler for Clock signal
void CoffeeMaker::handle_clock_interrupt() {
  // Implement clock interrupt handler using ESPHome GPIO abstraction
  // On each CLOCK rising edge, read 1 bit from DATA pin and assemble into byte
  // 
  // From sample.ino: data[clock_pulse] = pinReadFast(DATA_PIN);
  // We accumulate bits into current_byte_ and store when we have 8 bits
  
  if (gpio_data_ != nullptr) {
    bool data_bit = gpio_data_->digital_read();
    
    // Assemble byte from LSB to MSB (bit 0 first) - matching sample.ino
    current_byte_ |= (data_bit << bit_count_);
    bit_count_++;
    
    // When we have 8 bits, store the byte in current frame
    if (bit_count_ >= 8) {
      if (frame_byte_count_ < 9) {
        current_frame_[frame_byte_count_] = current_byte_;
        frame_byte_count_++;
        
        // Complete frame received (9 bytes)
        if (frame_byte_count_ >= 9) {
          frame_complete_ = 1;
          frame_byte_count_ = 0;
        }
      }
      current_byte_ = 0;
      bit_count_ = 0;
    }
  }
}

// Interrupt Handler for Strobe signal  
void CoffeeMaker::handle_strobe_interrupt() {
  // Implement strobe interrupt handler using ESPHome GPIO abstraction
  // STROBE signals end of frame and start of multiplexer read
  // 
  // From sample.ino:
  // 1. Increment sequence_number
  // 2. Handle any pending Z pin output for button press
  // 3. Read multiplexer state (S0, S1, S2) from STROBE/DATA/CLOCK pins
  // 4. Indicates 9 bytes of shift register data complete
  
  // NOTE: This is called after all 8 bits of the 9th byte are received
  // The 35-cycle delay loop in sample.ino appears to be for debouncing
  // the multiplexer lines which are asynchronous to the clock/data signals
  
  // Complete any partial byte if needed
  if (bit_count_ > 0 && frame_byte_count_ < 9) {
    current_frame_[frame_byte_count_] = current_byte_;
    frame_byte_count_++;
    current_byte_ = 0;
    bit_count_ = 0;
  }
  
  // Ensure frame is marked complete
  if (frame_byte_count_ > 0) {
    frame_complete_ = 1;
  }
  
  // Handle button press control if active
  // From sample.ino: if (set_output == 1) { pinSetFast(Z_PIN); }
  // This holds the Z pin LOW to simulate button press
  if (button_active_ && gpio_zio_ != nullptr) {
    // Z pin is open-drain: LOW = button pressed (drive to ground)
    // Setting digital_write(false) should drive LOW
    // NOTE: May need to configure as open-drain in setup()
    gpio_zio_->digital_write(false);
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


// Zio pin write (open-drain output mode)
void CoffeeMaker::zio_write(bool state) {
  // Open-drain control of Z pin for button press simulation
  // From sample.ino:
  //   pinMode(Z_PIN, OUTPUT);  // Set to output when button press active
  //   pinSetFast(Z_PIN);       // Drive LOW = button press (uses digitalWrite convention inverted)
  //   pinResetFast(Z_PIN);     // Release = HIGH = no press
  //   pinMode(Z_PIN, INPUT);   // Return to input after press
  //
  // NOTE: Confusing naming in sample.ino:
  // - pinSetFast appears to mean set OUTPUT and drive LOW (button pressed)
  // - pinResetFast appears to mean release (HIGH)
  // This is unusual - normally SET means drive HIGH
  //
  // Open-drain behavior:
  // - When driving: digital_write(false) = pull to ground (button active)
  // - When releasing: digital_write(true) = tri-state/pull-up (button inactive)
  
  if (gpio_zio_ != nullptr) {
    if (state) {
      // Button pressed: pull Z pin LOW
      // Configure pin as output with open-drain
      gpio_zio_->pin_mode(gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN);
      gpio_zio_->digital_write(false);
      ESP_LOGD(TAG, "Z pin driven LOW (button pressed)");
    } else {
      // Button released: tri-state Z pin (let pull-up release it)
      gpio_zio_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_OPEN_DRAIN);
      gpio_zio_->digital_write(true);
      ESP_LOGD(TAG, "Z pin released (button inactive)");
    }
  }
}

// Zio pin read (input mode)
bool CoffeeMaker::zio_read() {
  // Read Z pin state in input mode
  // From sample.ino: This pin reads multiplexer output or button feedback
  // When configured as input, it should be pulled high and driven low by
  // external circuitry (the coffee maker's button sense circuit)
  //
  // NOTE: In sample.ino, Z_PIN is set to INPUT initially:
  //   pinMode(Z_PIN, INPUT); // Input until an output is made
  // And it reads multiplexer outputs, not discrete sensor state
  
  if (gpio_zio_ != nullptr) {
    // Configure as input to read external state
    gpio_zio_->pin_mode(gpio::FLAG_INPUT);
    bool state = gpio_zio_->digital_read();
    return state;
  }
  return false;
}

}  // namespace coffee_maker
}  // namespace esphome
