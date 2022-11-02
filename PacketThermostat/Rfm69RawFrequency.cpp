#include <RFM69.h>
#include "Rfm69RawFrequency.h"
#include <RFM69registers.h>

RFM69rawFrequency::RFM69rawFrequency(int spiPin, int intPin) : RFM69(spiPin, intPin) {}
// return the frequency 
uint32_t RFM69rawFrequency::getFrequencyRaw()
{
    return (((uint32_t)readReg(REG_FRFMSB) << 16) + ((uint16_t)readReg(REG_FRFMID) << 8) + readReg(REG_FRFLSB));
}

// set the frequency 
void RFM69rawFrequency::setFrequencyRaw(uint32_t freqHz)
{
    writeReg(REG_FRFMSB, freqHz >> 16);
    writeReg(REG_FRFMID, freqHz >> 8);
    writeReg(REG_FRFLSB, freqHz);
}
