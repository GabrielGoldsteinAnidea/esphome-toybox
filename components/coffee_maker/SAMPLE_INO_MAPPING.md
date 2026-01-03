# sample.ino to coffee-maker.cpp Mapping

## Variable Mapping

| sample.ino | coffee-maker.cpp | Type | Purpose |
|-----------|-----------------|------|---------|
| `clock_pulse` | `bit_count_` | uint8_t | Bits received in current byte |
| `strobe_pulse` | `frame_complete_` | uint8_t | Flag: complete frame ready |
| `data[9]` | `current_frame_[9]` | uint8_t[] | Shift register output bytes |
| `S0, S1, S2` | `mux_s0_, mux_s1_, mux_s2_` | uint8_t | Multiplexer state lines |
| `set_output` | `button_active_` | uint8_t | Button press currently active |
| `output_counter` | `button_strobe_counter_` | uint8_t | Strobe cycles since button active |
| `button_int` | `pending_button_command_` | uint8_t | Button press queued (1-6) |
| `led_state[10]` | `led_state_[10]` | uint8_t[] | LED state (0=OFF, 1=ON, 2=FLASH) |
| `led_state_count[10]` | `led_state_count_[10]` | uint16_t[] | Accumulator for LED averaging |
| `sequence_number` | NOT YET | int | Multiplexer cycle counter |
| `machine_state_raw[9]` | NOT YET | int | LED interpretation for each pin |
| `m_state` | NOT YET | int | Overall machine state |

---

## Function Mapping

### ISR Handlers

#### `void clockPulse()` → `void CoffeeMaker::handle_clock_interrupt()`

**sample.ino**:
```cpp
void clockPulse() {
    data[clock_pulse] = pinReadFast(DATA_PIN);
    clock_pulse++;
}
```

**coffee-maker.cpp**:
```cpp
void CoffeeMaker::handle_clock_interrupt() {
  if (gpio_data_ != nullptr) {
    bool data_bit = gpio_data_->digital_read();
    current_byte_ |= (data_bit << bit_count_);
    bit_count_++;
    if (bit_count_ >= 8) {
      // Store byte in frame
      current_frame_[frame_byte_count_] = current_byte_;
      frame_byte_count_++;
      // Reset for next byte
      current_byte_ = 0;
      bit_count_ = 0;
    }
  }
}
```

**Differences**:
- sample.ino stores individual bits in array (9×8=72 element space, uses 72)
- coffee-maker.cpp accumulates bits into bytes first (9 bytes)
- More memory-efficient but requires ISR arithmetic
- **Correctness**: Same protocol, different assembly strategy

---

#### `void strobePulse()` → `void CoffeeMaker::handle_strobe_interrupt()`

**sample.ino** (partial):
```cpp
void strobePulse() {
    detachInterrupt(STROBE_PIN);
    detachInterrupt(CLOCK_PIN);
    
    sequence_number++;
    
    if (set_output == 1) {
        pinSetFast(Z_PIN);
    }
    
    S0 = S1 = S2 = 0;
    for (uint8_t i = 0; i < 35; i++) {
        S0 = (pinReadFast(STROBE_PIN) || S0);
        if (S0 == 1) {
            S1 = (pinReadFast(DATA_PIN) || S1);
            S2 = (pinReadFast(CLOCK_PIN) || S2);
        }
    }
    
    clock_pulse = 0;
    strobe_pulse = 1;
}
```

**coffee-maker.cpp** (current):
```cpp
void CoffeeMaker::handle_strobe_interrupt() {
  // Complete any partial byte
  if (bit_count_ > 0 && frame_byte_count_ < 9) {
    current_frame_[frame_byte_count_] = current_byte_;
    frame_byte_count_++;
    // ...
  }
  
  if (frame_byte_count_ > 0) {
    frame_complete_ = 1;
  }
  
  if (button_active_ && gpio_zio_ != nullptr) {
    gpio_zio_->digital_write(false);
  }
  
  // TODO: Multiplexer reading (35-cycle loop)
  // TODO: Button execution logic
}
```

**Missing**:
1. Multiplexer reading loop (S0, S1, S2)
2. Button press execution (check sequence match, timing)
3. Sequence number counter
4. Interrupt re-attachment (if using detach/attach pattern)

---

### Main Loop Processing

#### `void loop()` → `void CoffeeMaker::update()`

**sample.ino** (simplified):
```cpp
void loop() {
    digitalWrite(LED_PIN, ready);
    
    if (strobe_pulse == 1) {
        pinResetFast(Z_PIN);
        pinMode(Z_PIN, INPUT);
        
        // Reset sequence if at start
        if (S0 == sequence[0][0] && S1 == sequence[0][1] && 
            S2 == sequence[0][2] && data[0] == 0 && data[1] == 1) {
            sequence_number = 0;
        }
        
        // Handle button command
        if (ready == 1 && button_int > 0) {
            sequence_action_number = mapping[button_int];
            sequence_action_number--;
            if (sequence_action_number < 0) {
                sequence_action_number = 9;
            }
            ready = 0;
            button_int = -1;
        }
        
        // Execute button press if sequence matches
        if (sequence_number == sequence_action_number) {
            pinMode(Z_PIN, OUTPUT);
            set_output = 1;
            output_counter++;
        } else {
            set_output = 0;
        }
        
        // Stop pressing after timeout
        if (output_counter > 10) {
            output_counter = 0;
            set_output = 0;
            ready = 1;
            sequence_action_number = -1;
        }
        
        determineLedState();
        strobe_pulse = 0;
    }
}
```

**coffee-maker.cpp** (current):
```cpp
void CoffeeMaker::update() {
  if (frame_complete_) {
    frame_complete_ = 0;
    
    // Parse frame and extract LED states
    uint8_t data_block = current_frame_[1];
    
    if (data_block == 0) {
      // Block 1: LEDs 1-3
      one_cup_ready_state_ = (current_frame_[7] != 0);
      // ... etc
    } else if (data_block == 1) {
      // Block 2: LEDs 5-9
      water_empty_state_ = (current_frame_[7] != 0);
      // ... etc
    }
  }
  
  // Publish to sensor entities
  if (one_cup_ready_ != nullptr) {
    one_cup_ready_->publish_state(one_cup_ready_state_);
  }
  // ... etc
}
```

**Missing**:
1. Button command execution state machine
2. Sequence number tracking and reset
3. LED state averaging and interpretation
4. Debounce/stability logic

---

## Button Control Flow

### sample.ino Sequence

```
User calls setButton(button_id)
  ↓
Sets button_int = button_id (1-6)
  ↓
Main loop checks: if (ready == 1 && button_int > 0)
  ↓
Maps button_int to sequence_action_number using mapping[] array
  ↓
Sets ready = 0, button_int = -1
  ↓
Waits for multiplexer to reach sequence_action_number (via strobe_isr)
  ↓
When sequence_number == sequence_action_number:
  - Switches Z_PIN to OUTPUT
  - Sets set_output = 1
  - Increments output_counter
  ↓
After output_counter > 10 (~20ms):
  - Stops setting set_output
  - Switches Z_PIN back to INPUT
  - Sets ready = 1
  ↓
Button press complete, ready for next command
```

### coffee-maker.cpp Sequence (Current)

```
External call: set_one_cup_request()
  ↓
Sets pending_button_command_ = 4
  ↓
Main loop (update) queues the command
  ↓
strobe_isr() processes:
  - NOT YET IMPLEMENTED
  ↓
Should execute Z pin timing based on sequence match
```

**Gap**: The strobe_isr() needs to implement the button execution logic.

---

## LED State Interpretation

### sample.ino Mapping

**When data[1] == 0** (Block 1):
```
LED[3] = data[5]  (QP2) → hot_water (bit 5)
LED[2] = data[6]  (QP1) → two_cup_ready (bit 6)
LED[1] = data[7]  (QP0) → one_cup_ready (bit 7)
```

**When data[1] == 1** (Block 2):
```
LED[9] = data[3]  (QP4) → grind_disabled (bit 3)
LED[8] = data[4]  (QP3) → decalcification_needed (bit 4)
LED[7] = data[5]  (QP2) → error (bit 5)
LED[6] = data[6]  (QP1) → grounds_full (bit 6)
LED[5] = data[7]  (QP0) → water_empty (bit 7)
```

### coffee-maker.cpp Mapping (Current)

```cpp
if (data_block == 0) {
  one_cup_ready_state_ = (current_frame_[7] != 0);
  two_cup_ready_state_ = (current_frame_[6] != 0);
  hot_water_state_ = (current_frame_[5] != 0);
}
else if (data_block == 1) {
  water_empty_state_ = (current_frame_[7] != 0);
  grounds_full_state_ = (current_frame_[6] != 0);
  error_state_ = (current_frame_[5] != 0);
  decalcification_needed_state_ = (current_frame_[4] != 0);
  grind_disabled_state_ = (current_frame_[3] != 0);
}
```

**Status**: ✓ Correctly mapped to sensors

---

## Differences Summary

| Aspect | sample.ino | coffee-maker.cpp | Status |
|--------|-----------|-----------------|--------|
| Bit Assembly | Bit array (72 elements) | Byte accumulation | Better |
| Button Control | Sample.ino's state machine | Queued + TODO in ISR | Needs completion |
| LED Averaging | 500-sample window | Direct mapping only | TODO |
| Multiplexer Reading | 35-cycle loop in strobe_isr | Documented, not impl | TODO |
| Machine State | Complex state machine | Not implemented | Optional |
| ZIO Pin Control | pinMode switching | Mode switching via flags | Better abstraction |
| Sensor Mapping | LED state → machine state | LED state → sensor state | More flexible |

---

## Next Implementation Steps

1. **HIGH PRIORITY**: Implement multiplexer reading (S0/S1/S2) in strobe_isr
2. **HIGH PRIORITY**: Implement button press execution in strobe_isr
3. **MEDIUM**: Add LED state averaging logic in update()
4. **MEDIUM**: Refine LED interpretation if needed
5. **LOW**: Add complex machine state logic (if needed for automations)
6. **UNKNOWN**: Identify and implement analog sensor sources

---

## Testing Checklist

- [ ] Verify frame assembly with known signal patterns
- [ ] Test button execution timing (20ms pulse duration)
- [ ] Verify LED state transitions
- [ ] Test LED averaging window (500 samples at update interval)
- [ ] Validate sensor publishing to Home Assistant
- [ ] Test button press on real hardware
- [ ] Verify multiplexer synchronization
- [ ] Check Z pin open-drain behavior

