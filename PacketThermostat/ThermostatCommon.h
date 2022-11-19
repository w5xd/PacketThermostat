#pragma once
#include "PcbSignalDefinitions.h"
// values for USE_SERIAL. each one uses a bit more program memory
#define SERIAL_PORT_OFF 0 // If you use this, you can't set the radio parameters on the serial port.
#define SERIAL_PORT_PROMPT_ONLY 1
#define SERIAL_PORT_SETUP 2
#define SERIAL_PORT_VERBOSE 3
#define SERIAL_PORT_DEBUG 4
#define SERIAL_PORT_SETME_DEBUG_TO_SEE 5

#define USE_SERIAL SERIAL_PORT_VERBOSE   
#define HVAC_AUTO_CLASS 1 // not enough program memory for all features? Turn this off.

namespace Furnace {
    void UpdateOutputs(uint8_t mask);
    void ClearOutputBits(uint8_t mask);
    void SetOutputBits(uint8_t mask=0);
}
extern uint16_t aDecimalToInt(const char*&);
extern uint32_t aHexToInt(const char*&);

typedef unsigned long msec_time_stamp_t;
struct ThermostatCommon
{
public:
    static void setup();
    virtual void OnInputsChanged(uint8_t inputs, uint8_t previous)=0;
    virtual bool ProcessCommand(const char *cmd, uint8_t len, uint8_t senderid, bool toMe)= 0;
    virtual const char *ModeNameString() = 0;
    virtual bool GetTargetAndActual(int16_t &targetCx10, int16_t &actualCx10) = 0;
    virtual void loop(msec_time_stamp_t now) = 0;
    uint8_t TypeNumber() const { return MyTypeNumber; }
    uint8_t ModeNumber() const { return MyModeNumber; }
protected:
    static uint8_t MyTypeNumber;
    static uint8_t MyModeNumber;
};

extern ThermostatCommon *hvac;
extern const int HVAC_EEPROM_START;
extern const char HVAC_SETTINGS[];
extern const char AUTO_SETTINGS[];
