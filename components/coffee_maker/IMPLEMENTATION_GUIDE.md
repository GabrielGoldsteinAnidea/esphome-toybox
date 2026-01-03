# Coffee Maker Component - Protocol Implementation Progress

## Successfully Implemented

### Data Reception Framework ✓
- Ring buffer with frame assembly (9-byte frames)
- Clock ISR: Reads DATA pin on rising edge, accumulates bits into bytes
- Strobe ISR: Completes frame, signals to main loop
- Bit-level assembly (LSB first to match sample.ino)

### Basic Sensor Publishing ✓
- All 8 binary sensors implemented:
  - one_cup_ready, two_cup_ready, hot_water
  - water_empty, grounds_full, error
  - decalcification_needed, grind_disabled
- Two analog sensors implemented:
  - coffee_quantity (cups), coffee_flavor

### Button Command Structure ✓
- Command queuing system (pending_button_command_)
- Methods for all button presses:
  - set_onoff() → button 1
  - set_one_cup_request() → button 4
  - set_two_cups_request() → button 5
  - set_hot_water_request() → button 3

### ZIO Pin Control ✓
- zio_write() with open-drain support (notes on mode switching)
- zio_read() for input mode
- Button active counter for timing

### Protocol Documentation ✓
- PROTOCOL_NOTES.md with complete analysis
- Detailed comments in all ISR implementations
- LED state mapping documented
- Button sequence reference

---

## Still TODO / Not Implemented

### 1. GPIO Pin Initialization in setup()
**Status**: Skeleton only with detailed comments
**What's needed**:
```cpp
// Create InternalGPIOPin objects from pin numbers:
gpio_clock_ = new InternalGPIOPin(pin_clock_, INPUT);
gpio_data_ = new InternalGPIOPin(pin_data_, INPUT);
gpio_strobe_ = new InternalGPIOPin(pin_strobe_, INPUT);
gpio_zio_ = new InternalGPIOPin(pin_zio_, INPUT);  // Initially input

// Attach interrupts:
gpio_clock_->attach_interrupt(clock_isr, RISING);
gpio_strobe_->attach_interrupt(strobe_isr, RISING);
```

**Challenge**: ESPHome's GPIO abstraction API varies - need to check actual interface
**References**: esphome/core/gpio.h, esphome/components/gpio/

### 2. Multiplexer State Reading (S0, S1, S2)
**Status**: Documented in comments, not implemented
**From sample.ino**:
```cpp
S0 = S1 = S2 = 0;
for (uint8_t i = 0; i < 35; i++) {
  S0 = (pinReadFast(STROBE_PIN) || S0);
  if (S0 == 1) {
    S1 = (pinReadFast(DATA_PIN) || S1);
    S2 = (pinReadFast(CLOCK_PIN) || S2);
  }
}
```

**What's needed**:
- Add delay loop in strobe_isr() after frame assembly
- Read S0 from STROBE pin
- Read S1 from DATA pin (if S0 == 1)
- Read S2 from CLOCK pin (if S0 == 1)
- Store in mux_s0_, mux_s1_, mux_s2_

**Why**: Button synchronization - Z pin must be driven LOW at correct multiplexer position

### 3. LED State Averaging
**Status**: Direct mapping only (no averaging)
**From sample.ino**:
```cpp
// Average over 500 strobe cycles (~1 second at 2ms per cycle)
// Count samples: 0=OFF, 500=ON, 1-499=FLASHING
for (int i = 1; i < 10; i++) {
  led_state_count[i] += (int) led_state[i];
}
if (led_state_count[0] == 500) {  // After 500 samples
  if (led_state_count[i] == 0) 
    machine_state_raw[i] = 0;      // OFF
  else if (led_state_count[i] == 500)
    machine_state_raw[i] = 1;      // ON
  else
    machine_state_raw[i] = 2;      // FLASHING
  led_state_count[i] = 0;
}
```

**What's needed**:
- Accumulate LED bit values over averaging_sample_count_
- After reaching 500 samples (~30 seconds at 60s update interval):
  - Determine state: ALWAYS_ON (500), ALWAYS_OFF (0), or FLASHING (1-499)
  - Update sensor published state
  - Reset counters

**Why**: Distinguishes between steady LED state and flashing (which also carries meaning)

### 4. Button Press Execution
**Status**: Queued but not executed
**From sample.ino**:
```cpp
if (sequence_number == sequence_action_number) {
  pinMode(Z_PIN, OUTPUT);
  set_output = 1;
  output_counter++;
}
// Stop pressing after 10 strobe cycles (~20ms)
if (output_counter > 10) {
  output_counter = 0;
  set_output = 0;
  ready = 1;
  sequence_action_number = -1;
}
```

**What's needed** (in strobe_isr):
1. Check if pending_button_command_ is set
2. Look up sequence position from button mapping (4→sequence 4, etc.)
3. When sequence_number matches:
   - Set button_active_ = 1
   - Call zio_write(true) to drive Z pin LOW
   - Start button_strobe_counter_
4. After ~10 strobe cycles:
   - Set button_active_ = 0
   - Call zio_write(false) to release Z pin
   - Clear pending_button_command_

**Dependencies**: Requires multiplexer S0/S1/S2 reading to work properly

### 5. Machine State Determination
**Status**: Not implemented
**From sample.ino** (complex logic based on LED combinations):
```cpp
if (!LED1 && !LED2 && !LED3 && !LED4 && !LED5 && !LED6 && !LED7 && !LED8 && !LED9)
  m_state = 0;  // OFF
else if (LED1==FLASH && LED2==FLASH)
  m_state = 1;  // BUSY
else if ((LED1==ON && LED2==OFF) || (LED1==OFF && LED2==ON))
  m_state = 3;  // VENDING
else if (LED6 == ON)
  m_state = 4;  // GROUNDS_FULL
else if (LED6 == FLASH)
  m_state = 6;  // ERROR
else if (LED5 == ON)
  m_state = 5;  // NO_WATER
else if (LED1==ON && LED2==ON)
  m_state = 2;  // READY
else
  m_state = 6;  // ERROR
```

**What's needed**:
- Decision tree/state machine in update() after averaging
- Output as single "machine_state" sensor or individual status indicators
- Or keep separate binary sensors and let Home Assistant do logic

**Complexity**: May not be needed if individual LEDs are sufficient

### 6. Analog Sensor Values (coffee_quantity, coffee_flavor)
**Status**: Hardcoded to 0
**Missing**: Where do these values come from?
- Could be additional ADC readings (not in shift register?)
- Could be decoded from multiplexer outputs
- Could be from I2C register reads
- **Need to check coffee maker specs**

**What's needed**:
- Identify source of analog data
- Decode from protocol
- Apply scaling/calibration

---

## Integration Checklist

- [ ] GPIO pin initialization with interrupt attachment
- [ ] Test ISR reception of clock/strobe signals
- [ ] Verify 9-byte frame assembly in ring buffer
- [ ] Multiplexer state reading implementation
- [ ] LED state averaging (500-sample window)
- [ ] Button press execution (Z pin timing)
- [ ] LED-to-sensor-state mapping refinement
- [ ] Identify and implement analog sensor value sources
- [ ] Full end-to-end protocol testing with actual hardware
- [ ] Home Assistant entity discovery and integration

---

## Known Issues / Notes

### NOTE 1: ISR Wrapper Functions Unused
Currently `clock_isr()` and `strobe_isr()` are defined but not attached to GPIO pins.
Once GPIO attachment is implemented, these warnings will resolve.

### NOTE 2: Open-Drain Z Pin Control
The Z pin mode switching (INPUT ↔ OUTPUT) timing is critical:
- Should only be OUTPUT during button press window (~20ms)
- Must return to INPUT immediately after
- Sample.ino's approach: `pinMode(Z_PIN, OUTPUT)` only when set_output==1

### NOTE 3: Frame Assembly Assumptions
Current implementation assumes:
- CLOCK frequency high enough to sample cleanly
- STROBE rising edge occurs after 9×8 clock pulses
- No clock/strobe synchronization issues

Real hardware may need debouncing or clock filtering.

### NOTE 4: Timing Sensitivity
Button press execution depends on multiplexer sequence alignment:
- Must know current sequence number (from S0/S1/S2 readings)
- Button timing window is ~2 strobe cycles (narrow!)
- Missing the window means button press fails silently

This is why sample.ino reads S0/S1/S2 pins so carefully.

### NOTE 5: Coffee Maker Specifications
Many details inferred from sample.ino without knowing actual hardware:
- Protocol timing (clock frequency, strobe period)
- LED mapping and meaning
- Button response timing and requirements
- Electrical characteristics (voltage levels, drive strength)

Should compare against coffee maker service manual if available.

---

## Code Structure

```
coffee-maker.cpp:
├── ISR wrappers
│   ├── clock_isr() ✓ defined, attached TBD
│   └── strobe_isr() ✓ defined, attached TBD
├── CoffeeMaker::setup()
│   ├── ✓ State initialization
│   ├── ✓ Global pointer for ISR
│   └── TODO: GPIO attachment
├── CoffeeMaker::handle_clock_interrupt()
│   ├── ✓ Bit accumulation (LSB first)
│   ├── ✓ Byte assembly
│   └── ✓ Frame buffer management
├── CoffeeMaker::handle_strobe_interrupt()
│   ├── ✓ Frame completion signaling
│   ├── TODO: Multiplexer S0/S1/S2 reading (35-cycle loop)
│   └── TODO: Button press timing control
├── CoffeeMaker::update()
│   ├── ✓ Frame parsing (basic)
│   ├── TODO: LED state averaging
│   └── ✓ Sensor publishing
└── Button control
    ├── ✓ set_onoff(), set_one_cup_request(), etc.
    ├── ✓ Command queueing
    └── TODO: Execution in strobe_isr()
```

---

## Estimated Remaining Work

- **Easy**: GPIO initialization (~1-2 hours with API research)
- **Medium**: Multiplexer reading + button execution (~2-4 hours testing)
- **Hard**: LED state averaging + machine state logic (~4-6 hours design/test)
- **Unknown**: Analog sensor source identification (need hardware specs)

**Total**: 8-15 hours of implementation + testing

---

## References

- sample.ino: Complete reference implementation (Arduino/Particle)
- PROTOCOL_NOTES.md: Detailed protocol analysis
- coffee-maker.h: State variable definitions
- coffee-maker.cpp: Implementation with detailed TODOs
