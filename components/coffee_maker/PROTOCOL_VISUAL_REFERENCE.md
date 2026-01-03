# Coffee Maker Protocol Visual Reference

## Data Reception Flowchart

```
STROBE rising edge (frame start)
         ↓
    Reset counters
         ↓
CLOCK rising edge (8 times)
    ├─ Read DATA pin (1 bit)
    ├─ Assemble into byte (LSB first: bit_count 0→7)
    └─ Increment bit_count
         ↓
    bit_count == 8?
         │
         ├─ YES: Store byte in frame[frame_byte_count_]
         │       Reset bit_count = 0, byte = 0
         │       Increment frame_byte_count_
         │
         └─ NO: Continue waiting for next CLOCK edge
         ↓
    Repeat 8 more times (for byte 2)
         ↓
    ... (repeat for all 9 bytes)
         ↓
    STROBE rising edge (frame end)
         ├─ Complete any partial byte
         ├─ Set frame_complete_ = 1
         ├─ Signal main loop
         └─ Ready for next frame
```

## Frame Structure

```
Byte 0: Control/Sync
├─ Purpose: Indicates start of frame
├─ Expected: 0 (usually)
└─ Sample.ino: Checks data[0] == 0 for reset

Byte 1: Block Selector
├─ Determines which LED group is present
├─ Value 0: LEDs 1-3 present (bits from bytes 5,6,7)
├─ Value 1: LEDs 5-9 present (bits from bytes 3,4,5,6,7)
└─ Coffee Maker: Uses this to route data to correct sensors

Byte 2: Padding/Unknown
├─ Purpose: Unknown (possibly multiplexer sync?)
└─ Not used in current implementation

Bytes 3-7: Sensor Data Bits
├─ Bit 0: Reserved/unused
├─ Bit 1: Reserved/unused
├─ Bit 2: Reserved/unused
├─ Bit 3: Grind Disabled LED (when block==1)
├─ Bit 4: Decalcification Needed LED (when block==1)
├─ Bit 5: Error/Hot Water LED (selected by block)
├─ Bit 6: Grounds Full/Two Cup Ready LED (selected by block)
└─ Bit 7: Water Empty/One Cup Ready LED (selected by block)

Byte 8: Extra/Unused
└─ Spare or multiplexer data?
```

## LED State Mapping (from sample.ino)

### Block 0 (data[1] == 0)
```
Frame bits:  5  6  7  |  Frame bits:  5  6  7
             ↓  ↓  ↓  |               ↓  ↓  ↓
           [QP2][QP1][QP0]  becomes  [3][2][1]
             ↓  ↓  ↓  |               ↓  ↓  ↓
           Hot Water, Two Cup Ready, One Cup Ready
```

**Mapping**:
- `data[7]` → LED[1] → one_cup_ready
- `data[6]` → LED[2] → two_cup_ready
- `data[5]` → LED[3] → hot_water

### Block 1 (data[1] == 1)
```
Frame bits:  3  4  5  6  7  |  Frame bits:  3  4  5  6  7
             ↓  ↓  ↓  ↓  ↓  |               ↓  ↓  ↓  ↓  ↓
           [QP4][QP3][QP2][QP1][QP0]  becomes  [9][8][7][6][5]
             ↓  ↓  ↓  ↓  ↓  |               ↓  ↓  ↓  ↓  ↓
         Grind Disabled, Decalc Needed, Error, Grounds Full, Water Empty
```

**Mapping**:
- `data[7]` → LED[5] → water_empty
- `data[6]` → LED[6] → grounds_full
- `data[5]` → LED[7] → error
- `data[4]` → LED[8] → decalcification_needed
- `data[3]` → LED[9] → grind_disabled

## Button Press Execution Flowchart

```
External Call: set_one_cup_request()
        ↓
  Queue command: pending_button_command_ = 4
        ↓
Main Loop (update() at 60s interval)
        ↓
Wait for next STROBE pulse
        ↓
Strobe ISR fires:
  ├─ Check: pending_button_command_ set?
  │         ├─ NO: Continue
  │         └─ YES: Look up sequence position
  │
  ├─ Increment sequence_number (0-9)
  │
  ├─ Check: sequence_number == target_sequence?
  │         ├─ NO: Continue waiting
  │         └─ YES: Start button press
  │
  ├─ If button_active_:
  │   ├─ Drive Z pin LOW (zio_write(true))
  │   └─ Increment button_strobe_counter_
  │
  └─ If button_strobe_counter_ > 10 (~20ms):
      ├─ Release Z pin (zio_write(false))
      ├─ Clear pending_button_command_
      └─ button_active_ = 0
        ↓
Button press complete!
Coffee maker should respond to command
```

## Multiplexer State Reading (35-cycle loop)

```
During STROBE ISR, after frame is complete:

Initialize: S0 = S1 = S2 = 0

Loop 35 times:
  ├─ Read STROBE pin
  ├─ S0 = S0 || STROBE_value  (accumulate)
  │
  ├─ If S0 == 1:
  │   ├─ S1 = S1 || DATA_value   (read DATA pin)
  │   └─ S2 = S2 || CLOCK_value  (read CLOCK pin)
  │
  └─ Continue loop

Result: S0, S1, S2 indicate multiplexer position
        (Used for button synchronization)

Accumulation logic (||) means:
  - Once S0 reads a 1, it stays 1 (debouncing)
  - Same for S1, S2
  - After 35 iterations: S0, S1, S2 are stable
```

## Button Mapping Reference (from sample.ino)

```
Button ID → Sequence Position (in sequence[] array)

uint8_t mapping[7] = {
    0,  // mapping[0]: unused
    4,  // mapping[1]: Button 1 (Power On) → sequence 4
    2,  // mapping[2]: Button 2 → sequence 2
    0,  // mapping[3]: Button 3 → sequence 0 (special?)
    5,  // mapping[4]: Button 4 (One Cup) → sequence 5
    3,  // mapping[5]: Button 5 (Two Cups) → sequence 3
    1   // mapping[6]: Button 6 (Hot Water) → sequence 1
};

Sequence Positions (from sample.ino):
0: S0=1, S1=1, S2=1  (Y7 SW3)
1: S0=1, S1=1, S2=1  (Y7 SW6)
2: S0=0, S1=1, S2=1  (Y6 SW2)
3: S0=0, S1=1, S2=1  (Y6 SW5)
4: S0=1, S1=0, S2=1  (Y5 SW1)
5: S0=1, S1=0, S2=1  (Y5 SW4)
6: S0=0, S1=0, S2=1  (Y4 ?)
7: S0=0, S1=0, S2=1  (Y4 ?)
8: S0=0, S1=0, S2=0  (Y0 POT1)
9: S0=0, S1=0, S2=0  (Y0 POT1)
```

## LED State Interpretation (sample.ino logic)

```
Sample.ino determines 3 states based on 500-cycle average:

LED State Value:
  0 = ALWAYS OFF (count == 0)
  1 = ALWAYS ON (count == 500)
  2 = FLASHING (count 1-499)

Machine State Logic:
  ┌─ All LEDs OFF → Machine OFF (state 0)
  │
  ├─ LED1 FLASH && LED2 FLASH → BUSY (state 1)
  │
  ├─ (LED1 ON && LED2 OFF) || (LED1 OFF && LED2 ON) → VENDING (state 3)
  │
  ├─ LED6 ON → GROUNDS FULL (state 4)
  │
  ├─ LED6 FLASH → ERROR (state 6)
  │
  ├─ LED5 ON → NO WATER (state 5)
  │
  ├─ LED1 ON && LED2 ON → READY (state 2)
  │
  └─ else → ERROR (state 6)

Note: Each LED can be OFF, ON, or FLASHING (not just binary)
This is why averaging over 500 samples is important!
```

## Timing Diagram (approximate)

```
CLOCK ─┐─┬─┬─┬─┬─┬─┬─┬─┬──────────────┬─┬─┬─...
       └─┘ └ └ └ └ └ └ └              └ └ └
           ↓ read bit ↓ read bit  ↓ read bit
DATA ──────────────────────────────────────
       (serial data, changes on clock edges)
       bit0 bit1 bit2 bit3 bit4 bit5 bit6 bit7

STROBE ──────────────────┐───────────────┬─────
                         └───────────────┘
                    (stays low for frame,
                     rising edge = end)

(At STROBE rising edge:
  35-cycle loop reads S0/S1/S2
  Then back to waiting for CLOCK edges)
```

## Interrupt Priority

```
1. CLOCK ISR
   ├─ Read 1 bit from DATA pin
   ├─ Assemble into byte
   └─ Fires 72 times per frame (9 bytes × 8 bits)

2. STROBE ISR
   ├─ Complete frame
   ├─ Read multiplexer state (S0/S1/S2)
   ├─ Execute button presses if needed
   └─ Fires ~0.5 times per second at 2ms per strobe

Note: CLOCK ISR happens many more times,
      but STROBE ISR does more work per call
```

## State Machine (Button Press)

```
┌─────────────────────────────────────────────┐
│ IDLE STATE                                  │
│ pending_button_command_ = 0                 │
│ button_active_ = 0                          │
└──────────────────┬──────────────────────────┘
                   │
                   │ set_one_cup_request()
                   │ pending_button_command_ = 4
                   │
                   ↓
┌─────────────────────────────────────────────┐
│ WAITING FOR SEQUENCE                        │
│ pending_button_command_ = 4 (queued)        │
│ Waiting: sequence_number == 5 (mapped)      │
└──────────────────┬──────────────────────────┘
                   │
                   │ (After ~2 seconds if sync'd)
                   │
                   ↓
┌─────────────────────────────────────────────┐
│ BUTTON ACTIVE                               │
│ button_active_ = 1                          │
│ Z pin driven LOW                            │
│ button_strobe_counter_ incrementing         │
└──────────────────┬──────────────────────────┘
                   │
                   │ (After ~10 strobe cycles = 20ms)
                   │
                   ↓
┌─────────────────────────────────────────────┐
│ BUTTON RELEASED                             │
│ button_active_ = 0                          │
│ Z pin released (HIGH)                       │
│ pending_button_command_ = 0 (cleared)       │
└──────────────────┬──────────────────────────┘
                   │
                   │ Coffee maker processes button
                   │ (internal reaction time varies)
                   │
                   ↓
              COMPLETE

Coffee maker should show state change
(e.g., brew light, output sound, etc.)
```

---

This visual reference should help understand the protocol flow and timing.
Compare against sample.ino to verify your understanding!
