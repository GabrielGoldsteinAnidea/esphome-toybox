# Coffee Maker Protocol Analysis - sample.ino to coffee-maker.cpp/h

## Protocol Overview
The coffee maker uses a shift register + multiplexer interface to communicate sensor states and receive button commands.

### Hardware Interface
- **CLOCK**: Rising edge signals data bit available on DATA line
- **DATA**: Contains individual bits of the shift register (1 bit per clock pulse)
- **STROBE**: Rising edge signals end of frame (9 bytes received)
- **ZIO**: Open-drain bidirectional pin for button commands

### Data Reception Pattern
1. **Shift Register Output**: 8 bits per strobe cycle (9 cycles = 9 bytes)
   - Each CLOCK rising edge: read 1 bit from DATA pin
   - 8 CLOCK pulses = 1 complete byte
   - 9 STROBE cycles = complete status update frame

2. **Data Array Structure** (from sample.ino):
   - `data[0]`: Control byte (data[0] == 0 for LED block 1, usually)
   - `data[1]`: Block selector (0 = LEDs 1-3 present, 1 = LEDs 5-9 present)
   - `data[2]`: Unknown/padding
   - `data[3]-data[7]`: Actual sensor bits
   - `data[8]`: Extra/unused

3. **Multiplexer Control** (S0, S1, S2 read from STROBE/DATA/CLOCK lines):
   - Read during 35-cycle delay loop after STROBE rising edge
   - S0 read from STROBE pin
   - If S0==1, then read S1 from DATA, S2 from CLOCK
   - These pins reflect the coffee maker's multiplexer output selection

### LED State Mapping (from sample.ino)
```
When data[1] == 0:
  LED[3] = data[5]  (QP2)
  LED[2] = data[6]  (QP1) 
  LED[1] = data[7]  (QP0)

When data[1] == 1:
  LED[9] = data[3]  (QP4)
  LED[8] = data[4]  (QP3)
  LED[7] = data[5]  (QP2)
  LED[6] = data[6]  (QP1)
  LED[5] = data[7]  (QP0)
```

### State Determination
- Sample.ino averages LED states over 500 strobe cycles (sampling period ~1 second at 2ms per strobe)
- LED values: 0=OFF, 1=ON, 2=FLASHING (toggle)
- Machine states derived from LED combination patterns (see sample.ino `get_led_state()`)

### Button Command Protocol
1. Sequence table maps button numbers (1-6) to strobe sequence positions
2. When button pressed:
   - Wait for correct sequence number (multiplexer position)
   - Set ZIO pin to OUTPUT
   - Drive ZIO LOW (pinSetFast/digitalWrite)
   - Hold for ~20ms (10 strobe cycles at 2ms each)
   - Release ZIO (pinResetFast/digitalWrite HIGH)
   - Set ZIO back to INPUT

### Differences from ESPHome Implementation

#### NOTE 1: Interrupt Framework
- **Sample.ino**: Uses Particle.attachInterrupt() with low-level pin access (pinReadFast/pinSetFast)
- **ESPHome**: Uses GPIO abstraction layer (InternalGPIOPin) with event-driven interrupt support
- **Action**: ESPHome GPIO abstraction will be slower but more portable

#### NOTE 2: GPIO Pin Access
- **Sample.ino**: 
  - Direct pinReadFast() on STROBE/DATA/CLOCK to read S0/S1/S2 values
  - Relies on hardware timing (35-cycle delay loop for debouncing)
- **ESPHome**:
  - Use InternalGPIOPin::digital_read() in ISR
  - May need different debouncing strategy
  - Delay loop should work in IRAM context

#### NOTE 3: Buffer Management
- **Sample.ino**: Single 9-byte data array, read in main loop
- **ESPHome**: Ring buffer approach (rx_buffer_[], rx_write_index_, rx_read_index_) is more flexible
- **Action**: Keep ring buffer but parse 9-byte frames

#### NOTE 4: State Machine Complexity
- **Sample.ino**: 
  - Tracks sequence_number for button press synchronization
  - LED state averaging over 500 samples
  - Complex state transition logic
- **ESPHome**:
  - Will implement averaging in update() method
  - State transitions can be simpler (publish raw sensor states)
  - Averaging done in binary_sensor logic

#### NOTE 5: ZIO Pin Mode Switching
- **Sample.ino**: 
  - Switches Z_PIN between INPUT and OUTPUT
  - Uses pinSetFast/pinResetFast for open-drain control
- **ESPHome**:
  - InternalGPIOPin has flags for open-drain, pull-up
  - Use digital_write(false) for pull LOW, digital_write(true) for release

#### NOTE 6: Missing Implementation Details
The following from sample.ino are NOT implemented yet:
1. Multiplexer S0/S1/S2 decoding (35-cycle delay loop)
2. LED state averaging and machine state determination
3. Button press sequence synchronization
4. ZIO open-drain control and mode switching
5. Complete state machine for LED interpretation

## Implementation Plan

### Phase 1: Basic Data Reception (DONE in structure, TODO in implementation)
- [x] Ring buffer for received bytes
- [x] Clock ISR to accumulate bits
- [x] Strobe ISR to finalize frames
- [ ] Implement actual bit accumulation logic
- [ ] Implement multiplexer S0/S1/S2 reading

### Phase 2: Sensor State Parsing
- [ ] Parse 9-byte frame from shift register
- [ ] Map data bits to sensor states
- [ ] Implement LED state averaging

### Phase 3: Button Command Interface
- [ ] Implement ZIO pin open-drain control
- [ ] Implement button press sequence (timing)
- [ ] Add state machine for command synchronization

### Phase 4: ESPHome Integration
- [ ] Publish sensor states to binary_sensor/sensor objects
- [ ] Add button/cover/switch entities for controls
- [ ] Integration with Home Assistant automations
