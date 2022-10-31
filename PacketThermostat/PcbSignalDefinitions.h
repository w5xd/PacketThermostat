#pragma once
// bit numbers in the output shift register on the PCB****AND in the input status*********************
enum OutregBits { BN_W_FAILSAFE, BN_R = BN_W_FAILSAFE, BN_Z2, BN_FIRST_SIGNAL=BN_Z2, BN_Z1, BN_W, BN_ZX, BN_X2, BN_X1, 
    BN_LAST_SIGNAL=BN_X1, BN_X3, NUMBER_OF_SIGNALS };

// On the PCB, 
//             Six signals appear at both input and output: BN_X1, BN_X2, BN_Z1, BN_Z2, BN_ZX, BN_W
//             BN_W_FAILSAFE is only an output, It controls the hardware relay on wire W
//             BN_R is only an input. It corresponds to the R wire having 24V AC present
//             BN_X3 is only an output. It shares its bit position with BN_R.

static const uint8_t NUM_HVAC_INPUT_SIGNALS = BN_LAST_SIGNAL + 1 - BN_FIRST_SIGNAL;

static const uint8_t INPUT_SIGNAL_MASK = 
    (1 << BN_Z2) | (1 << BN_Z1) | (1 << BN_W) | (1 << BN_ZX) | (1 << BN_X2) | (1 << BN_X1) | (1 << BN_R);
static const uint8_t OUTPUT_SIGNAL_MASK = 
    (1 << BN_Z2) | (1 << BN_Z1) | (1 << BN_W) | (1 << BN_ZX) | (1 << BN_X2) | (1 << BN_X1) | (1 << BN_X3);
