#pragma once
class RFM69rawFrequency : public RFM69
{
    // The baseclass getFrequency()/setFrequency methods use floating point to
    // get to precise Hz values. Those floating point routines cost a lot of
    // program memory. don't use them
public:
    RFM69rawFrequency(int spiPin, int intPin);
    // return the frequency 
    uint32_t getFrequencyRaw();

    // set the frequency 
    void setFrequencyRaw(uint32_t freqHz);
};
