#pragma once
#include "PcbSignalDefinitions.h"

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
};

extern ThermostatCommon *hvac;
extern const int HVAC_EEPROM_START;