int CLOCK_PIN = D1;
int DATA_PIN = D3;
int STROBE_PIN = D4;
int Z_PIN = A2;
int LED_PIN = D7;

// Whether we have a clock/strobe pulse
volatile uint8_t clock_pulse = 0;
volatile uint8_t strobe_pulse = 0;

// Data set to shift register
volatile uint8_t data[9];

// Data on the S lines
volatile uint8_t S0 = 0;
volatile uint8_t S1 = 0;
volatile uint8_t S2 = 0;

// Whether to set an output on Z
volatile uint8_t set_output = 0;
// How long we have been attempting to send the Z pulse
int output_counter = 0;

// Button requested to be pressed
int button_int = -1;
// Sequence to action from button_int
int sequence_action_number = -1;
// Ready to accept a button press
uint8_t ready = 1;
// Counter for press down
int counter = 0;

// Status LEDs
volatile int led_state[10];
volatile int led_state_count[10];

// Current state of the machine
volatile int machine_state_raw[9];
int m_state = 0;

// S0 S1 S2 values
uint8_t sequence[10][3] = {
    /* 0 */ {1, 1, 1}, // Y7 SW3
    /* 1 */ {1, 1, 1}, // Y7 SW6 
    /* 2 */ {0, 1, 1}, // Y6 SW2
    /* 3 */ {0, 1, 1}, // Y6 SW5
    /* 4 */ {1, 0, 1}, // Y5 SW1
    /* 5 */ {1, 0, 1}, // Y5 SW4
    /* 6 */ {0, 0, 1}, // Y4 ?
    /* 7 */ {0, 0, 1}, // Y4 ?
    /* 8 */ {0, 0, 0},  // Y0 POT1
    /* 9 */ {0, 0, 0}  // Y0 POT1
};

// Button request to sequence mapping
uint8_t mapping[7] = {
    0, 4, 2, 0, 5, 3, 1
};

// Current sequence number
volatile int sequence_number = 0;

void setup() {
    Particle.function("setButton", setButton);
    Particle.function("turnOn", turnOn);
    Particle.variable("state", m_state);
    
    pinMode(CLOCK_PIN, INPUT);
    pinMode(DATA_PIN, INPUT);
    pinMode(STROBE_PIN, INPUT);
    pinMode(Z_PIN, INPUT); // Input until an output is made; otherwise all readings from the front panel board would be LOW
    
    pinMode(LED_PIN, OUTPUT);
    
    digitalWrite(Z_PIN, LOW);
    
    attachInterrupt(CLOCK_PIN, clockPulse, RISING, 0);
    attachInterrupt(STROBE_PIN, strobePulse, RISING, 0);
}

int turnOn(String button) {
    if (m_state == 0) { // Machine is off
        button_int = 1;
        return 2;
    }
    return 1;
}

int setButton(String button) {
    button_int = button.toInt();
    
    if (button_int < 1 || button_int > 6) {
        button_int = -1;
        return 0;
    }
    
    if (ready == 0) {
        return 0;
    } else {
        return 1;
    }
}

/**
 * Get the shift register data
 */
void clockPulse() {
    data[clock_pulse] = pinReadFast(DATA_PIN);
    clock_pulse++;
}

/**
 * Get the multiplexer data
 */
void strobePulse() {
    detachInterrupt(STROBE_PIN);
    detachInterrupt(CLOCK_PIN);
    
    sequence_number++;
    
    // Set the Z pin if requested
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
    
    clock_pulse = 0; // Reset clock pulse count
    strobe_pulse = 1;
}

void loop() {
    digitalWrite(LED_PIN, ready);
    
    // If we have a strobe pulse...
    if (strobe_pulse == 1) {
        // Reset the request pin (should be a few moments after the machine has sampled it)
        pinResetFast(Z_PIN);
        pinMode(Z_PIN, INPUT);
        
        // Reset sequence if we're at the start of it
        if (
            S0 == sequence[0][0] &&
            S1 == sequence[0][1] &&
            S2 == sequence[0][2] &&
            data[0] == 0 &&
            data[1] == 1
        ) {
            sequence_number = 0;
        }
        
        // If we're available to accept a command, and one has been sent...
        if (ready == 1 && button_int > 0) {
            // Button mapping to sequence
            sequence_action_number = mapping[button_int];
            sequence_action_number--;
            if (sequence_action_number < 0) {
                sequence_action_number = 9;
            }
            ready = 0;
            button_int = -1;
        }
        
        // If we match the sequence_action_number
        if (sequence_number == sequence_action_number) {
            pinMode(Z_PIN, OUTPUT); // Set the pin to output as its not fast enough in the INT
            set_output = 1;
            output_counter++;
        } else {
            set_output = 0;
        }
        
        // Stop pressing the button after x seconds
        if (output_counter > 10) { // Strobe occurs every 2ms, so this is a button held for ~20ms
            output_counter = 0;
            set_output = 0;
            ready = 1;
            sequence_action_number = -1;
        }
        
        determineLedState();

        // Reset data array (probably a cleaner way of doing this)
        data[0] = data[1] = data[2] = data[3] = data[4] = data[5] = data[6] = data[7] = 0;
        strobe_pulse = 0;
        
        // Reattach the interrupts for the next sequence
        attachInterrupt(CLOCK_PIN, clockPulse, RISING, 0);
        attachInterrupt(STROBE_PIN, strobePulse, RISING, 0);
    }
}

/**
 * Determine the LED state.
 */
void determineLedState() 
{
    // Determine the LED state
    if (data[1] == 0) {
        led_state[3] = data[5]; // QP2
        led_state[2] = data[6]; // QP1
        led_state[1] = data[7]; // QP0
    } else {
        led_state[9] = data[3]; // QP4
        led_state[8] = data[4]; // QP3
        led_state[7] = data[5]; // QP2
        led_state[6] = data[6]; // QP1
        led_state[5] = data[7]; // QP0
    }

    get_led_state();
}

void get_led_state() {
    led_state_count[0]++;
    for (int i = 1; i < 10; i++) {
        led_state_count[i] += (int) led_state[i];
    }
    
    if (led_state_count[0] == 500) {
        for (int i = 1; i < 10; i++) {
            if (led_state_count[i] == 0) {
                machine_state_raw[i] = 0;
            } else if (led_state_count[i] == led_state_count[0]) {
                machine_state_raw[i] = 1;
            } else if (led_state_count[i] < led_state_count[0]) {
                machine_state_raw[i] = 2;
            }
            led_state_count[i] = 0;
        }
        
        led_state_count[0] = 0;
        
        /** Possible states:
        * 0 OFF
        * 1 BUSY
        * 2 READY
        * 3 VENDING
        * 4 GROUNDS FULL
        * 5 NO WATER
        * 6 ERROR
        */
        
        /**
         * LED1 - 1 CUP
         * LED2 - 2 CUP
         * LED3 - Steam
         * LED5 - Water
         * LED6 - Grinds
         * LED7 - Error
         * LED8 - Decaulk
         * LED9 - Eco
        */
        
        if (!machine_state_raw[1] && !machine_state_raw[2] && !machine_state_raw[3] && !machine_state_raw[4] && !machine_state_raw[5] && !machine_state_raw[6] && !machine_state_raw[7] && !machine_state_raw[8] && !machine_state_raw[9]) {
            m_state = 0;
        } else if (machine_state_raw[1] == 2 && machine_state_raw[2] == 2) {
            m_state = 1;   
        } else if ((machine_state_raw[1] == 1 && machine_state_raw[2] == 0) || (machine_state_raw[1] == 0 && machine_state_raw[2] == 1)) {
            m_state = 3;
        } else if (machine_state_raw[7] == 1) {
            m_state = 4;
        } else if (machine_state_raw[7] == 2) {
            m_state = 6;
        } else if (machine_state_raw[5] == 1) {
            m_state = 5;
        } else if (machine_state_raw[1] == 1 && machine_state_raw[2] == 1) {
            m_state = 2;
        } else {
            m_state = 6;
        }
    }
}