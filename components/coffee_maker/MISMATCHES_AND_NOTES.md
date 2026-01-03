# Known Mismatches & Implementation Notes

## Critical Notes

### NOTE 1: Bit Accumulation Strategy Difference
**sample.ino**: Stores individual bits in bit array
```cpp
volatile uint8_t data[9];  // 9 bytes worth of shift register output
void clockPulse() {
    data[clock_pulse] = pinReadFast(DATA_PIN);  // 1 bit per array element
    clock_pulse++;
}
```
Uses 72 array elements to store 72 bits (9 bytes × 8 bits).

**coffee-maker.cpp**: Accumulates bits into bytes
```cpp
// Bit-level assembly in handle_clock_interrupt()
current_byte_ |= (data_bit << bit_count_);
bit_count_++;
```
More efficient, stores 9 bytes total.

**Correctness**: ✓ Both approaches work, coffee-maker is more efficient
**Action**: No change needed - current implementation is correct

---

### NOTE 2: Interrupt Attachment Pattern
**sample.ino**: Uses Particle.attachInterrupt()
```cpp
attachInterrupt(CLOCK_PIN, clockPulse, RISING, 0);
attachInterrupt(STROBE_PIN, strobePulse, RISING, 0);
```

**coffee-maker.cpp**: Expects ESPHome GPIO abstraction
```cpp
// TODO: Use gpio namespace and InternalGPIOPin
// gpio_clock_->attach_interrupt(clock_isr, RISING);
// gpio_strobe_->attach_interrupt(strobe_isr, RISING);
```

**Challenge**: ESPHome's interrupt API is not yet implemented in this component
**Action**: Need to research correct ESPHome GPIO interrupt attachment pattern

---

### NOTE 3: Multiplexer Reading Timing
**sample.ino**: Reads in strobe_isr with 35-cycle busy loop
```cpp
void strobePulse() {
    // ... 
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

**Why 35 cycles?**
- Appears to be debouncing the multiplexer outputs
- STROBE, DATA, CLOCK pins are being read while in ISR
- Multiplexer output is asynchronous to clock/strobe
- 35 cycles provides settling time for the multiplexer

**coffee-maker.cpp**: Not yet implemented
```cpp
// TODO: Implement 35-cycle delay loop in strobe_isr
// Read S0/S1/S2 from GPIO pins during delay
```

**Challenge**: ISR timing constraints - need to ensure this completes before next strobe
**Action**: Implement with attention to ISR duration limits on ESP8266

---

### NOTE 4: Button Synchronization Dependency
**sample.ino**: Execution dependent on multiplexer sequence
```cpp
// In main loop (strobePulse thread):
if (sequence_number == sequence_action_number) {
    pinMode(Z_PIN, OUTPUT);
    set_output = 1;
    output_counter++;
}
```

**Requires**:
1. Multiplexer S0/S1/S2 reading to determine sequence_number
2. Button mapping table to convert button ID to sequence number
3. Sequence reset detection

**coffee-maker.cpp**: Button execution NOT YET implemented
```cpp
// Current: Only queuing the command
pending_button_command_ = button_id;

// Missing: Execution logic in strobe_isr()
// Must wait for multiplexer sequence match before driving Z pin
```

**Critical**: Without multiplexer reading, button presses will fail
**Action**: Implement multiplexer reading first, then button execution

---

### NOTE 5: Frame Synchronization Assumptions
**sample.ino**: Assumes clean clock/strobe/data signal edges
```cpp
void clockPulse() {
    data[clock_pulse] = pinReadFast(DATA_PIN);
    clock_pulse++;
}
// Relies on CLOCK ISR firing exactly 8 times per byte
// Relies on STROBE ISR firing after last clock pulse
```

**What if**:
- Clock signal has glitches/noise?
- Strobe arrives slightly early/late?
- Data pin settles slowly?

**coffee-maker.cpp**: Same assumptions
```cpp
// No filtering or error checking
// Will assemble incorrect bytes if signals are noisy
```

**Mitigation**: Real hardware may need:
- Pin debouncing configuration
- Clock frequency validation
- Frame boundary checking
- CRC or parity validation

**Note**: sample.ino doesn't show error recovery, so perhaps the protocol is robust

---

### NOTE 6: LED State Averaging Not Yet Implemented
**sample.ino**: Averages over 500 cycles
```cpp
led_state_count[i] += (int) led_state[i];
if (led_state_count[0] == 500) {  // Every 500 strobes
    if (led_state_count[i] == 0) 
        machine_state_raw[i] = 0;      // OFF
    else if (led_state_count[i] == 500)
        machine_state_raw[i] = 1;      // ON
    else
        machine_state_raw[i] = 2;      // FLASHING
    led_state_count[i] = 0;
}
```

**At ~2ms per strobe**: 500 strobes = ~1 second
**At 60s update interval**: We get one averaging window per 30 update cycles

**coffee-maker.cpp**: Direct bit assignment only
```cpp
one_cup_ready_state_ = (current_frame_[7] != 0);
```

**Problem**: Single bit value doesn't distinguish between:
- LED ON (stable 1)
- LED FLASHING (alternating 0/1)
- LED OFF (stable 0)

**Impact**: LED flashing carries information in sample.ino's protocol
- FLASHING might indicate "busy" or "error"
- But our current code treats all non-zero as ON

**Solution Needed**:
1. Implement 500-sample averaging window
2. Distinguish ALWAYS_ON (500), FLASHING (1-499), ALWAYS_OFF (0)
3. Publish separate or indicator for flashing state

---

### NOTE 7: Missing Analog Sensor Implementation
**sample.ino**: No explicit implementation shown
```cpp
// sample.ino has no analog sensor code visible
// But the coffee maker has coffee_quantity and coffee_flavor sensors
```

**coffee-maker.cpp**: Hardcoded to 0
```cpp
float coffee_quantity_value_{0.0f};
float coffee_flavor_value_{0.0f};
```

**Questions**:
- Where do these values come from in the coffee maker protocol?
- Are they in the shift register data?
- Are they in separate I2C registers?
- Are they calculated from LED patterns?

**Note**: Coffee maker specs or datasheet would clarify
**Action**: May need hardware investigation or documentation review

---

### NOTE 8: Open-Drain Z Pin Mode Switching
**sample.ino**:
```cpp
pinMode(Z_PIN, INPUT);      // Initially input (idle)
// Later, in strobePulse():
if (set_output == 1) {
    pinSetFast(Z_PIN);      // Drive LOW (button pressed)
}
// And after timeout:
pinResetFast(Z_PIN);
pinMode(Z_PIN, INPUT);      // Return to input
```

**Confusing naming**: 
- `pinSetFast(Z_PIN)` appears to mean "set as OUTPUT and drive LOW"
- `pinResetFast(Z_PIN)` appears to mean "release to HIGH"
- This is inverted from typical naming

**coffee-maker.cpp**: Used flags-based mode switching
```cpp
void CoffeeMaker::zio_write(bool state) {
  if (state) {
    gpio_zio_->pin_mode(gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN);
    gpio_zio_->digital_write(false);  // Pull LOW
  } else {
    gpio_zio_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_OPEN_DRAIN);
    gpio_zio_->digital_write(true);   // Tri-state (release)
  }
}
```

**Note**: This assumes ESPHome's GPIO has `pin_mode()` with FLAGS
**Risk**: Actual API may be different
**Action**: Verify ESPHome GPIO abstraction API

---

### NOTE 9: Sequence Number Tracking Not Implemented
**sample.ino**:
```cpp
volatile int sequence_number = 0;
void strobePulse() {
    sequence_number++;  // Increment on each strobe
    // ...
}
// Later in loop():
if (sequence_number == sequence_action_number) {
    // Execute button
}
```

**coffee-maker.cpp**: Variable declared but not incremented
```cpp
// TODO: Increment in strobe_isr()
// Used by button execution logic
```

**How it works**: Multiplexer cycles through 10 positions (0-9)
- Each STROBE pulse = 1 cycle
- Button press must happen at the right cycle
- Timing is synchronous to the multiplexer, not free-running

**Implementation Needed**:
1. Track sequence_number (0-9) incrementing on each strobe
2. Compare against sequence_action_number
3. Execute button press when they match
4. Reset to 0 when reaching end of cycle

---

### NOTE 10: State Machine Complexity
**sample.ino**: Implements full machine state interpretation
```cpp
// Complex logic converting LED patterns to machine state:
if (!machine_state_raw[1] && !machine_state_raw[2] && ...) {
    m_state = 0;  // OFF
} else if (machine_state_raw[1] == 2 && machine_state_raw[2] == 2) {
    m_state = 1;  // BUSY
} else if ((machine_state_raw[1] == 1 && machine_state_raw[2] == 0) || 
           (machine_state_raw[1] == 0 && machine_state_raw[2] == 1)) {
    m_state = 3;  // VENDING
} // ... etc
```

**coffee-maker.cpp**: Individual sensor states only
```cpp
// No complex state interpretation
// Just publish individual LED states
```

**Philosophy difference**:
- sample.ino: Single "machine state" enum (0-6)
- coffee-maker: Individual sensor states, let Home Assistant handle logic

**Tradeoff**:
- sample.ino more abstracted but less flexible
- coffee-maker more data exposed but Home Assistant does interpretation

**Note**: Either approach works; coffee-maker's approach is more maintainable

---

## Summary of Mismatches

| Issue | sample.ino | coffee-maker.cpp | Status | Impact |
|-------|-----------|-----------------|--------|--------|
| Bit assembly | Bit array | Byte accumulation | Implemented differently | None (both correct) |
| Multiplexer reading | In strobe_isr | TODO | Not implemented | **CRITICAL** - needed for buttons |
| Button execution | In main loop/strobe_isr | TODO in strobe_isr | Queued only, not executed | **CRITICAL** - buttons won't work |
| LED averaging | 500-sample window | Direct mapping | Missing | **MEDIUM** - misses flashing state |
| Machine state | Complex logic | Not implemented | Not needed | Low (Home Assistant does it) |
| Analog sensors | Not shown | Hardcoded to 0 | Unknown source | **MEDIUM** - sensors broken |
| Z pin control | pinMode switching | pin_mode flags | Different API | Needs testing |
| Sequence tracking | Incremented in ISR | Not implemented | Missing | **CRITICAL** - needed for buttons |
| Interrupt attachment | attachInterrupt() | TODO | Not implemented | **CRITICAL** - no data reception yet |
| Signal robustness | Assumed clean | Assumed clean | Same | May need debouncing |

---

## Critical Path Forward

1. **MUST DO FIRST**: Implement GPIO interrupt attachment
   - Without this, no data reception at all
   - Blocking all other work

2. **MUST DO SECOND**: Implement multiplexer reading
   - Button functionality depends on this
   - Needed for proper synchronization

3. **MUST DO THIRD**: Implement button execution logic
   - Complete the button press state machine
   - Add sequence number tracking

4. **SHOULD DO**: Implement LED averaging
   - Detects flashing state (carries meaning in protocol)
   - Better state interpretation

5. **SHOULD DO**: Identify analog sensor source
   - Sensors are broken without this
   - May require hardware documentation

6. **NICE TO HAVE**: Implement complex state machine
   - Current approach (individual sensors) is already flexible
   - Only needed if Home Assistant integration becomes complex

---

## Testing Priority

1. Test ISR reception of clock/strobe signals
   - Enable logging in handle_clock_interrupt() and handle_strobe_interrupt()
   - Verify frame assembly works with known signal patterns

2. Test multiplexer reading
   - Log S0, S1, S2 values as read
   - Verify sequence_number increments correctly

3. Test button execution
   - Send button command and verify Z pin behavior
   - Check timing (20ms pulse duration)

4. Test LED interpretation
   - Verify ON/OFF state detection
   - Then add averaging for flashing detection

5. Test sensor publishing
   - Verify binary sensors update in Home Assistant
   - Verify sensor values are reasonable

6. Test button press on hardware
   - Verify actual coffee maker responds to commands
   - Check timing constraints aren't violated

