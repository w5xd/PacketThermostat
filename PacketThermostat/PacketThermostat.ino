/* (c) 2022 by Wayne E. Wright, Round Rock, Texas, USA

Permission is hereby granted, free of charge, to any person obtaining a copy
of this packet thermostat software and associated documentation files 
(the "Software"), to deal in the Software without restriction, including 
without limitation the rights to use, copy, modify, merge, publish, 
distribute, sublicense, and/or sell copies of the Software, and to permit 
persons to whom the Software is furnished to do so, subject to the following 
conditions:


The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE. */

/*
** 
** Sketch for the Pro Micro in the packet thermostat with its associated printed circuit board.
**
** The packet thermostat is designed to intervene between an existing thermostat
** and furnace. In bypass operation it passes all thermostat control signals unchanged
** through to the furnace.
**
** It accepts commands over its RFM69 packet radio that it uses to set into modes
** that modify the 24VAC commands to the furnace. And it sends reports over the RFM69.
**
** The PCB and sketch designs handle up to 6 wires of 24VAC controls. 
** These 6 wires, for example, usually include W for heat, G for fan, Y for a compressor (either AC or heat pump)
** O or B for a reversing valve. A multi-stage furnace, heat pump, and/or air conditioners each might add another wire.
**
** "Communicating" thermostats are not supported.
**
** Some furnaces have a single 24VAC supply for both heat and cool, which is supplied to the thermostat
** in the R wire, but others might have separate 24VAC for the two, which would usually be labeled Rc and Rh.
** The PCB supports three outputs on one of the R supplies (labeled Rx) and two on the other (labeled Rz). Then
** there are two more outputs with on-board jumpers to be placed on either Rx or Rz. The 24V power arrangement
** affects only the necessary wiring of the PCB connectors and jumpers. This sketch is not affected by the
** R wire 24VAC supply wiring.
**
** This PCB design requires a dedicated 5VDC power supply through its barrel connector.
**
** A Serial LCD display is supported via a qwiic connector. A calendar/clock (RTC) is also supported
** on qwiic. This board does not support any user input except via the packet radio.
** It has a micro-USB connector that is used to program the Arduino Pro Micro and help with debugging,
** and to configure the EEPROM.
**
** The mapping between the generic signal names (X1, X2, Z1, Z2, ZX) and typical thermostat wire names
** (O, B, Y, Y2, G) does not appear in this sketch. The only exceptions are these two wires:
** The W wire is assumed to be the furnace wire and R is assumed to be 24VAC when power is on to the furnace.
**
** W is special. It is the one wire that the PCB passes through to the furnce when this PCB is turned off.
** That is, the heat function on the furnace still works, but no compressor.
** There is nothing in the PCB nor sketch that demands that W be the standard furnace W function. W is simply
** the signal wire that passes through the PCB when its power is off.
**
** This sketch supports multiple HVAC "types" (see HVAC.cpp). The types are numbered starting at zero,
** and type zero is the trivial type named PassThrough. The PassThrough type copies each if the 6 input
** signals to its corresponding output.
** 
** The remaining types are documented in HVAC.cpp. Each of those types has settings in EEPROM that 
** determine the behavior of this sketch/PCB when set to that type.
*/

// libraries
#include <RFM69.h>
#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include <SerLCD.h>
#include <SparkFun_RV8803.h>
#include <avr/wdt.h>

// custom library
#include <RadioConfiguration.h>
#include "ThermostatCommon.h"
#include "Rfm69RawFrequency.h"

#define ENABLE_OUTPUT_RELAYS 1  // for testing, the sketch can be built with outputs disabled.

#define SCHEDULE_ENTRIES 1 // set to zero to remove this feature

namespace LCD {
    const byte MODE_COLUMN = 0;
    const byte MODE_ROW = 0;
    const byte TIME_COLUMN = 0;
    const byte TIME_ROW = 1;
    const byte HVAC_COLUMN=6;
    const byte HVAC_ROW = 0;
    const byte HVAC_TEMPERATURES_ROW = 1;
    const byte HVAC_TEMPERATURES_COLUMN = 6;
    const byte HVAC_COMPRESSORHOLD_ROW = 0;
    const byte HVAC_COMPRESSORHOLD_COLUMN = 15;

    SerLCD lcd;

    const long WHEN_TO_REINIT_INTERVAL_MSEC = 60000 * 3; // 3 minutes
    unsigned long whenToReinit;

    void backlightOK() { lcd.setBacklight(64, 64, 64);  }

    void init()
    {
        lcd.begin(Wire);
        backlightOK();
        lcd.setContrast(5);
        lcd.clear();
        lcd.noCursor();
        lcd.noBlink();
        lcd.noAutoscroll();
        whenToReinit = millis() + WHEN_TO_REINIT_INTERVAL_MSEC;
    }

    void printMode(const char *m)
    {
        lcd.setCursor(MODE_COLUMN, MODE_ROW);
        lcd.write(m);
        lcd.write(' ');
    }

    void printTime(const char *t)
    {
        lcd.setCursor(TIME_COLUMN, TIME_ROW);
        lcd.write(t);
    }

    void printTemperatures(const char *t)
    {
        lcd.setCursor(HVAC_TEMPERATURES_COLUMN, HVAC_TEMPERATURES_ROW);
        lcd.write(t);
    }

    void printOutputs(const char *p)
    {
        lcd.setCursor(HVAC_COLUMN, HVAC_ROW);
        lcd.write(p);
    }

    void printCompressorHold(const char *p)
    {
        lcd.setCursor(HVAC_COMPRESSORHOLD_COLUMN, HVAC_COMPRESSORHOLD_ROW);
        lcd.write(p);
    }

    enum {BACKLIGHT_UNKNOWN, BACKLIGHT_OFF, BACKLIGHT_ON} backlightShowMissing24V;

    bool reinit;

    void backLight(bool good)
    {   // change LCD backlight if R wire is not live.
        if (good) {
            if (backlightShowMissing24V != BACKLIGHT_ON)
            {
                backlightOK();
                lcd.clear();
                reinit = true;
            }
            backlightShowMissing24V = BACKLIGHT_ON;
        } else {
            if (backlightShowMissing24V != BACKLIGHT_OFF)
            {
                lcd.setBacklight(255, 0, 0);
                lcd.clear();
                reinit = true;
            }
            backlightShowMissing24V = BACKLIGHT_OFF;
        }
    }

    const int BANNER_TIME_MSEC = 2000;
    void printBanner(const char *p)
    {
        lcd.clear();
        lcd.write(p);
        delay(BANNER_TIME_MSEC);
    }

    void loop(unsigned long now)
    {
        if (now - whenToReinit > WHEN_TO_REINIT_INTERVAL_MSEC)
        {
            whenToReinit = now;
            lcd.clear();
            reinit = true;
            backlightShowMissing24V = BACKLIGHT_UNKNOWN;
        }
    }
}

namespace
{
    const int MAX_WIRE_NAME_LEN = 2;
    struct HeatSafetyMask_t {
        uint8_t dontCareMask;
        uint8_t mustMatchMask;
        uint8_t toClear;
        HeatSafetyMask_t()  { memset(this, 0, sizeof(*this));  } // memset generates smaller code than explicit initializers
    };
    static_assert(sizeof(HeatSafetyMask_t) == 3, "no padding");
    const int NUM_HEAT_SAFETY_ENTRIES = 3;
#if SCHEDULE_ENTRIES
    struct ScheduleEntry_t {
        uint8_t degreesCx5; // 255 max, 255/5 = 51 degrees C (way too hot)
        unsigned TimeOfDayHour : 5; // 0 to 23
        unsigned TimeOfDayMinute: 6; // 0 to 60
        unsigned DaysOfWeek: 7; // bit mask, 
        unsigned AutoMode: 1;
        ScheduleEntry_t() { memset(this, 0, sizeof(*this)); }
        };
    static_assert(sizeof(ScheduleEntry_t) == 4, "EEPROM size changed!");
    const int NUM_SCHEDULE_TEMPERATURE_ENTRIES = 16;
#endif
    enum class EepromAddresses {PACKET_THERMOSTAT_START = RadioConfiguration::EepromAddresses::TOTAL_EEPROM_USED,
                SIGNAL_LABEL_ASSIGNMENT = PACKET_THERMOSTAT_START,
                DISPLAY_UNITS_ADDRESS = SIGNAL_LABEL_ASSIGNMENT + (OutregBits::NUMBER_OF_SIGNALS * MAX_WIRE_NAME_LEN),
                COMPRESSOR_MASK,
                COMPRESSOR_HOLD_SECONDS,
                HEATSAFETY_HOLD_SECONDS =  COMPRESSOR_HOLD_SECONDS + 2,
                HEATSAFETY_TRIGGER_TEMPERATURECx10 = HEATSAFETY_HOLD_SECONDS + 2,
                HEATSAFETY_MAP = HEATSAFETY_TRIGGER_TEMPERATURECx10 + 2,
                SCHEDULE_TEMPERATURE_ENTRIES = HEATSAFETY_MAP + NUM_HEAT_SAFETY_ENTRIES * sizeof(HeatSafetyMask_t),
#if SCHEDULE_ENTRIES
                TOTAL_EEPROM_USED = SCHEDULE_TEMPERATURE_ENTRIES + NUM_SCHEDULE_TEMPERATURE_ENTRIES * sizeof(ScheduleEntry_t)
#else
                TOTAL_EEPROM_USED = SCHEDULE_TEMPERATURE_ENTRIES
#endif
    };

    // Arduino pin assignments **********************************************************
    // These correspond with the PCB layout**********************************************
    const int RFM69_SPI_CS_PIN = 10; // pin number
    const int RMF69_INT_PIN = 7; // 32U4 has hardware interrupt on this pin
    const int OUTREG_SPI_CS_PIN = 9;

    const int S1_7089U_OUTSIDE_PIN = A1;
    const int T_LM235_INLET_PIN = A2;
    const int T_LM235_OUTLET_PIN = A3;

    /* Both SPI and 2Wire functions are used, and are on the Atmega-defined pin positions,
    ** therefore no pin name declarations for them are here. */

    /* the PCB silkscreen gives generic names to the six input signals
    ** The signal names used in this sketch match the silk screen on the PCB. Those
    ** names are, by design, generic names instead of using typical thermostat wiring
    ** letter assignments. The PCB and, up until the "signal assignments" comment below,
    ** this sketch, name the 6 inputs and 7 outputs without reference to how they might be used
    ** in a particular HVAC install. 
    **
    ** These 3 signals share a common R wire, Rx:
    **		X1, X2
    ** These 2 signals Share the other R wire, Rz:
    **		Z1, Z2
    ** These 2 signals each has a silk-screend jumper to use either Rx or Rz:
    **		ZX, W
    **
    **	W is also unique in that it has a normaly closed relay through connection
    **
    ** There is a seventh input signal, R_ACTIVE, which goes low if 24VAC appears on the R versus C
    ** of the thermostat output.
    ** And there is a seventh output signal, X3, which has an output relay but no corresponding input bit.
    */

    const int PCB_INPUT_X1_PIN = 1;
    const int PCB_INPUT_X2_PIN = A0;
    const int PCB_INPUT_ZX_PIN = 8;
    const int PCB_INPUT_W_PIN = 4;
    const int PCB_INPUT_Z1_PIN = 5;
    const int PCB_INPUT_Z2_PIN = 6;
    const int PCB_INPUT_R_ACTIVE_PIN = 0;

    const int CMD_BUFLEN = 80;

    const int INPUT_AC_ACTIVE_MIN_MSEC = 100; // this long seen nothing on input, declare it OFF

    // enum OutregBits applies to both the input and output registers
    uint8_t OutputRegister = 0;
    uint8_t InputRegister = 0;
    bool InputsToHvacFlag;
    msec_time_stamp_t CompressorOffStartTime;
    bool CompressorOffTimeActive;
    msec_time_stamp_t HeatSafetyOffStartTime;
    uint8_t HeatSafetyShutoffMask;
    bool HeatSafetyOffTimeActive;
    const char * const HeatSafetyBanner = "OVER!";
    int16_t TinletTemperatureCx10; // last calculated copy of TinleADCsum

    char cmdbuf[CMD_BUFLEN];
    unsigned char charsInBuf;
    RFM69rawFrequency radio(RFM69_SPI_CS_PIN, RMF69_INT_PIN);
    RadioConfiguration radioConfiguration;
    bool radioSetupOK = false;
    const uint8_t GATEWAY_NODEID = 1;

    RV8803 rtc;

    const double ADCmaxVoltage = 3.3; // supply voltage is 3.3V
    const unsigned ADCmaxVoltageCount = 1024;         // ADC is 10 bits, which means 3.3V <-> 1024 counts 
    const int POWER2_ADC_READS_TO_AVERAGE = 6;
    const int NUMBER_TEMPERATURE_ADC_READS_TO_AVERAGE = 1 << POWER2_ADC_READS_TO_AVERAGE; // 2**6 = 64

    const uint16_t POLL_ADC_MSEC = 1000;  // 1000 msec between reads, times 64 is (about) 64 seconds
    const uint16_t BETWEEN_REPORTING_TEMPERTURE_MSEC = 60 * 1000 * 3; // 3 minutes
    uint16_t TinletADCsum; // 10 bit ADC summed 64 times just fits here
    uint16_t ToutletADCsum;
    uint16_t TexternalADCsum;
    uint8_t numberReadsInSum;
    bool displayLcdFarenheit;

    // The Honeywell C7089U temperature dependent resistor is supported
    const unsigned NUM_C7089_COEFFICIENTS = 75;
    struct C7089UCoefficient_t {
        int16_t TCelsiusX10;
        uint16_t ADCx64; // corresponding ADC value, multiplied by 64
        uint16_t slope;
    };
    extern const C7089UCoefficient_t C7089UCoefficients[NUM_C7089_COEFFICIENTS]  PROGMEM;
    const int POWER2_C7089SlopeScale = 18;
    static char reportbuf[sizeof(radio.DATA) + 1];

    void radioPrintInfo()
    {
#if USE_SERIAL >= SERIAL_PORT_SETUP
        Serial.print(F("Node "));
        Serial.print(radioConfiguration.NodeId(), DEC);
        Serial.print(F(" on network "));
        Serial.print(radioConfiguration.NetworkId(), DEC);
        Serial.print(F(" band "));
        Serial.print(radioConfiguration.FrequencyBandId(), DEC);
        Serial.print(F(" key "));
        radioConfiguration.printEncryptionKey(Serial);
        Serial.println();
        Serial.print(F("FreqRaw=")); Serial.println(radio.getFrequencyRaw() );
#endif
    }

    char *tempToAscii(char *p, int16_t temperatureX10)
    {
        itoa(temperatureX10, p, 10);
        p += strlen(p);
        *p = p[-1];  // its times ten, so append . and tenths
        p[-1] = '.';
        return p + 1;
    }

    char *formatTemperature(char *p, int16_t temperatureX10, char tag)
    { 
        *p++ = 'T'; *p++ = tag; *p++ = ':';
        return tempToAscii(p, temperatureX10);
    }

    const char *hvacWireName(uint8_t i)
    {
        static char wireName[MAX_WIRE_NAME_LEN+1];
        wireName[0] = 0;
        if (i < OutregBits::NUMBER_OF_SIGNALS)
        {
            int addr = static_cast<uint16_t>(EepromAddresses::SIGNAL_LABEL_ASSIGNMENT) + (i * MAX_WIRE_NAME_LEN);
            wireName[0] = EEPROM.read(addr++);
            wireName[1] = EEPROM.read(addr++);
            wireName[2] = 0;
            if (wireName[0] == static_cast<char>(0xff))
            {
                wireName[0] = '?';
                wireName[1] = 0;
            }
        }
        return &wireName[0];
    }

    uint8_t getCompressorMask()
    {
            int addr = static_cast<uint16_t>(EepromAddresses::COMPRESSOR_MASK);
            uint8_t ret = EEPROM.read(addr);
            if (ret == static_cast<uint8_t>(0xff))
                return 0;
            return ret;
    }

    uint16_t getCompressorHoldSeconds()
    {
            int addr = static_cast<uint16_t>(EepromAddresses::COMPRESSOR_HOLD_SECONDS);
            uint16_t ret;
            EEPROM.get(addr, ret);
            return ret;
    }

    uint16_t getHeatSafetyHoldSeconds()
    {
        int addr = static_cast<uint16_t>(EepromAddresses::HEATSAFETY_HOLD_SECONDS);
        uint16_t ret;
        EEPROM.get(addr, ret);
        return ret;
    }

    int16_t getHeatSafetyTemperatureCx10()
    {
        int addr = static_cast<uint16_t>(EepromAddresses::HEATSAFETY_TRIGGER_TEMPERATURECx10);
        int16_t ret;
        EEPROM.get(addr, ret);
        return ret;
    }

    HeatSafetyMask_t getHeatSafetyMask(uint8_t which)
    {
        HeatSafetyMask_t ret;
        if (which < NUM_HEAT_SAFETY_ENTRIES)
        {
            int addr = static_cast<uint16_t>(EepromAddresses::HEATSAFETY_MAP);
            addr += which * sizeof(ret);
            EEPROM.get(addr, ret);
        }
        return ret;
    }

    void setCompressorMask(uint8_t mask)
    {
            int addr = static_cast<uint16_t>(EepromAddresses::COMPRESSOR_MASK);
            EEPROM.write(addr, mask);
    }

    void setCompressorHoldSeconds(uint16_t s)
    {
        int addr = static_cast<uint16_t>(EepromAddresses::COMPRESSOR_HOLD_SECONDS);
        EEPROM.put(addr, s);
    }

    void setHeatSafetyHoldSeconds(uint16_t s)
    {
        int addr = static_cast<uint16_t>(EepromAddresses::HEATSAFETY_HOLD_SECONDS);
        EEPROM.put(addr, s);
    }

    void setHeatSafetyTemperatureX10(int16_t s)
    {
        int addr = static_cast<uint16_t>(EepromAddresses::HEATSAFETY_TRIGGER_TEMPERATURECx10);
        EEPROM.put(addr, s);
    }

    void setHeatSafetyMask(uint8_t which, const HeatSafetyMask_t &m)
    {
        if (which < NUM_HEAT_SAFETY_ENTRIES)
        {
            int addr = static_cast<uint16_t>(EepromAddresses::HEATSAFETY_MAP)  + which * sizeof(m);
            EEPROM.put(addr, m);
        }
    }

#if SCHEDULE_ENTRIES
    void setScheduleEntry(uint8_t which, const ScheduleEntry_t& se)
    {
        if (which < NUM_SCHEDULE_TEMPERATURE_ENTRIES)
        {
            int addr = static_cast<uint16_t>(EepromAddresses::SCHEDULE_TEMPERATURE_ENTRIES) + which * sizeof(se);
            EEPROM.put(addr, se);
        }
    }

    ScheduleEntry_t getScheduleEntry(uint8_t which)
    {
        ScheduleEntry_t ret;
        if (which < NUM_SCHEDULE_TEMPERATURE_ENTRIES)
        {
            int addr = static_cast<uint16_t>(EepromAddresses::SCHEDULE_TEMPERATURE_ENTRIES);
            addr += which * sizeof(ret);
            EEPROM.get(addr, ret);
        }
        return ret;
    }
#endif

    char *reportHvac(char *p, uint8_t mask, char t)
    {
        *p++ = 'H'; *p++ = 'V'; *p++ = t; *p++ = '=';
        uint8_t wireMask = 1;
        for (uint8_t i = 0; i < NUMBER_OF_SIGNALS; i++, wireMask <<= 1)
        {
            const char *q = hvacWireName(i);
            if (mask & wireMask)
            {
                while (*q)
                    *p++ = *q++;
            }
            else
            {
                uint8_t nameLen = strlen(q);
                for (uint8_t k = 0; k < nameLen; k++)
                    *p++ = '-';
            }
        }
        *p = 0;
        return p;
    }

    char *reportHvacOut(char *p, uint8_t mask)
    {  return reportHvac(p, mask, 'o');    }

    char *reportHvacIn(char *p, uint8_t mask)
    {  return reportHvac(p, mask, 'i');  }

    int16_t degreesCx10toDegreesFx10(int16_t cX10)
    {        return ((cX10 * 9) + 1600L) / 5L;    }

    void setHvacWireName(uint8_t i, const char *p)
    {
        if (i < NUMBER_OF_SIGNALS) {
            int addr = static_cast<int>(EepromAddresses::SIGNAL_LABEL_ASSIGNMENT) + (i * MAX_WIRE_NAME_LEN);
#if USE_SERIAL >= SERIAL_PORT_VERBOSE
            Serial.print(F("setHvacWireName("));
            Serial.print((int)i); Serial.print(F(","));
            auto q = p;
            Serial.print(*q++);
            if (*q)
                Serial.print(*q);
            Serial.println(F(")"));
#endif
            EEPROM.write(addr++,*p++);
            EEPROM.write(addr++,*p++);
        }
    }

    // convert the LM235 ADC read value (64 of them summed) to degrees C times 10
    int16_t degreesCx10fromLM235ADCx64(uint16_t bits10x64)
    {
        static const double LM235Coeficient = .01;         // LM235 is 10mV / K  (AKA 0.01 Volts/K)
        static const double ADCcountPerDegreeC = LM235Coeficient * ADCmaxVoltageCount / ADCmaxVoltage; // 3.103 counts per C or K

        // Shift the incoming, measured counts from Kelvins to Celsius, which means we can use fewer bits to represent it
        // 273.15 K is 0 degrees C
        // The maximum ADC count is 1024, corresponding to 3.3V. That temperature is 330K, which is 57C, or 134.6F
        static const uint16_t CountForWaterFreezeTempX64 = 
            static_cast<uint16_t>(0.5 + (NUMBER_TEMPERATURE_ADC_READS_TO_AVERAGE * 273.15 * ADCcountPerDegreeC )); // 54246

        int32_t countsFromFreezeX64 = static_cast<int32_t>(bits10x64); // convert from 16 bit unsigned to 32 bit signed
        // ... need those extra bits for multiply, below
        countsFromFreezeX64 -= CountForWaterFreezeTempX64; // offset from water freeze rather than absolute zero. signed 16 bits signficant

        // multiply the counts by degrees per count.
        // optimization: will use only integer arithmetic by arranging for the result to be multiplied by power of two of desired result.
        // Desired result is degrees C times 10 (so can be conveniently printed using integer arithmetic as nn.n degrees C
        static const double degreeCPerCountX64x10 = 10.0 / ( NUMBER_TEMPERATURE_ADC_READS_TO_AVERAGE * ADCcountPerDegreeC); // about .05
        static const int POWER2_FITS_IN_16 = 19; // shift left to increase precision, but stop at a value that fits in 16 bits signed
        static const int32_t degreeCPerCountShift16 = static_cast<int32_t>(0.5 + (1L << POWER2_FITS_IN_16) * degreeCPerCountX64x10); //about 26400
        return static_cast<int16_t>((countsFromFreezeX64 * degreeCPerCountShift16) >> POWER2_FITS_IN_16); 
    }

    // convert the C7089 ADC read value (64 of them summed) to degrees C times 10
    int16_t degreesCx10FromC7089ADC(uint16_t ADCx64)
    {
        uint16_t lowestADC = pgm_read_word_near(&C7089UCoefficients[0].ADCx64);
        if (ADCx64 >= lowestADC)
            return pgm_read_word_near(&C7089UCoefficients[0].TCelsiusX10);

        uint16_t highestADC = pgm_read_word_near(&C7089UCoefficients[NUM_C7089_COEFFICIENTS-1].ADCx64);
        if (ADCx64 <= highestADC)
            return pgm_read_word_near(&C7089UCoefficients[NUM_C7089_COEFFICIENTS-1].TCelsiusX10);

        uint8_t idx = NUM_C7089_COEFFICIENTS/2;
        uint8_t minIdx = 0;
        uint8_t maxIdx = NUM_C7089_COEFFICIENTS - 1;
        // binary search
        for (;;)
        {
            auto tbl = static_cast<uint16_t>(pgm_read_word_near(&C7089UCoefficients[idx].ADCx64));
            if (ADCx64 == tbl)
                return pgm_read_word_near(&C7089UCoefficients[idx].TCelsiusX10);
            else if (ADCx64 > tbl)
                maxIdx = idx;
            else
                minIdx = idx;

            if (maxIdx - minIdx == 1)
                break;

            idx = (minIdx + maxIdx) >> 1;
        }
        // linear interpolate between the nearest two table entries.
        // The "slope" is calculated by the compiler and appears in the table in PROGMEM.
        // This compile-time calculation optimizes away a run time long integer divide operation.
        uint16_t slope = static_cast<uint16_t>(pgm_read_word_near(&C7089UCoefficients[maxIdx].slope));
        int16_t b = pgm_read_word_near(&C7089UCoefficients[minIdx].TCelsiusX10);
        uint16_t dx = pgm_read_word_near(&C7089UCoefficients[minIdx].ADCx64) - ADCx64;

        int32_t mx = slope;
        mx *= dx;
        mx >>= POWER2_C7089SlopeScale;
        auto y = static_cast<int16_t>(b + mx);
        return y;
    }
    
    void radioTemperatureReport(int16_t TinletCx10, int16_t tOutletCx10, int16_t tOutsideCx10)
    {
        char *p = &reportbuf[0];
        p = formatTemperature(p, degreesCx10fromLM235ADCx64(TinletCx10), 'i'); // 7 characters
        *p++ = ' ';                                                             // 8
        p = formatTemperature(p, degreesCx10fromLM235ADCx64(tOutletCx10), 'o'); // 15
        *p++ = ' ';                                                             // 16
        p = formatTemperature(p, degreesCx10FromC7089ADC(tOutsideCx10), 's');   // 23
        *p++ = ' ';                                                             // 24
        int16_t t; int16_t a;
        if (hvac->GetTargetAndActual(t, a))
        {
            p = formatTemperature(p, t, 't');                                  // 31
            *p++ = ' ';                                                       // 32
            p = formatTemperature(p, a, 'a');                                 // 39
            *p++ = ' ';                                                       // 40
        }
        else 
        {
            strcpy(p, "0 0 ");
            p += 4;
        }

        *p++ = 'S';  *p++ = ':'; *p++ = '0' + hvac->TypeNumber();
        *p++ = ':';              *p++ = '0' + hvac->ModeNumber();
        *p++ = 0;

        if (radioSetupOK)
            radio.sendWithRetry(GATEWAY_NODEID, reportbuf, strlen(reportbuf));
#if USE_SERIAL >= SERIAL_PORT_VERBOSE
        Serial.print(radioSetupOK ? "Radio: " : "Not sent ");
        Serial.println(reportbuf);
#endif
    }
    
    void lcdHvacReport(uint8_t outputs)
    {
        reportHvacOut(reportbuf, outputs);
        strcat(reportbuf, ((1 << BN_W_FAILSAFE) & OutputRegister) != 0 ? "w" : "p");
        LCD::printOutputs(reportbuf+5);
    }

    void radioHvacReport(uint8_t in, uint8_t out)
    {
        auto p = reportHvacIn(reportbuf, in & INPUT_SIGNAL_MASK);
        *p++ = ' ';
        uint8_t outputs = out & OUTPUT_SIGNAL_MASK;
        p = reportHvacOut(p,  outputs);
        *p++ = ' ';
        auto q = rtc.stringTime8601();
        while (*p++ = *q++);
        if (radioSetupOK)
            radio.sendWithRetry(GATEWAY_NODEID, reportbuf, strlen(reportbuf));
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(radioSetupOK ? "Radio: " : "Not sent ");
        Serial.println(reportbuf);
#endif
        lcdHvacReport(outputs);
    }

    bool ProcessCommand(const char* cmd, unsigned char len)
    {
        static const char COMPRESSOR[] = "COMPRESSOR=0x";
        const char *q;
        if (cmd[0] == 'T' && cmd[1] == '=')
        {   // set the RTC time
            // T=YYYY MM DD HH MM SS DOW
            uint16_t year; uint8_t month; uint8_t dow;
            uint8_t day; uint8_t hour; uint8_t minute; uint8_t sec;
            const char* p = cmd + 2;
            year = aDecimalToInt(p);
            month = aDecimalToInt(p);
            day = aDecimalToInt(p);
            hour = aDecimalToInt(p);
            minute = aDecimalToInt(p);
            sec = aDecimalToInt(p);
            dow = aDecimalToInt(p);
            rtc.setTime(sec, minute, hour, dow, day, month, year);
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
            Serial.print(F("Setting clock to year:"));
            Serial.print(year);
            Serial.print(F(" mon:"));
            Serial.print(month);
            Serial.print(F(" day:"));
            Serial.println(day);
#endif
            return true;
        } 
        else if (toupper(cmd[0]) == 'I')
        {
            radioPrintInfo();
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
            Serial.print(F("Compressor Off active ="));
            Serial.println(CompressorOffTimeActive ? "active" : "off");
#endif
            return true;
        } 
        else if (toupper(cmd[0]) == 'H' && toupper(cmd[1]) == 'V' && isspace(cmd[2]))
        {   // set wire names in EEPROM
            // HV <R> <Z2> <Z1> <W> <ZX> <X2> <X1>
            const char *p = cmd + 2;
            for (uint8_t i = 0; i < NUMBER_OF_SIGNALS; i++)
            {
                char nameBuf[MAX_WIRE_NAME_LEN];
                memset(nameBuf, 0, sizeof(nameBuf));
                char *q = nameBuf;
                bool haveName = false;
                uint8_t count(0);
                for (;;)
                {
                    if (isspace(*p))
                    {
                        if (!haveName)
                        {
                            p += 1;
                            continue;
                        }
                        else
                            break;
                    }
                    else if (!*p)
                        break;
                    else
                    {
                        *q++ = *p++;
                        haveName = true;
                        if (++count >= MAX_WIRE_NAME_LEN)
                            break;
                    }
                }
                if (haveName)
                    setHvacWireName(i, nameBuf);
            }            
            return true;
        } 
        else if (toupper(cmd[0]) == 'D' && toupper(cmd[1]) == 'U' && cmd[2] == '=')
        {   // DU=F and DU=C  for farenheit and celsius. Only affects LCD
            displayLcdFarenheit = cmd[3] == 'F';
            EEPROM.write(static_cast<int>(EepromAddresses::DISPLAY_UNITS_ADDRESS), displayLcdFarenheit ? 1 : 0);
            return true;
        } 
        else if (0 != (q = strstr(cmd, COMPRESSOR)))
        {
            q += sizeof(COMPRESSOR) - 1;
            uint8_t mask = aHexToInt(q);
            if (!*q)
                return false;
            uint16_t seconds = aDecimalToInt(q);
            setCompressorMask(mask);
            setCompressorHoldSeconds(seconds);
            return true;
        } 
#if USE_SERIAL >= SERIAL_PORT_VERBOSE
        else if (0 == strcmp(cmd, "RH"))
        {
            radioHvacReport(InputRegister, OutputRegister);
            return true;
        }
#endif
        else if (toupper(cmd[0]) == 'H' && toupper(cmd[1]) == 'S')
        {
            q = cmd + 2;
            while (isspace(*q)) q += 1;
            auto c = *q++;
            if (c != 0) {
                while (isspace(*q)) q += 1;
                switch (c)
                {
                case 'C': // set temperature in Celsius
                    setHeatSafetyTemperatureX10(aDecimalToInt(q));
                    return true;

                case 'T': // set timer in  seconds
                    setHeatSafetyHoldSeconds(aDecimalToInt(q));
                    return true;

                case '1':
                case '2':
                case '3':
                {
                    int which = c - '1';
                    HeatSafetyMask_t m;
                    m.dontCareMask = aHexToInt(q);
                    if (*q)
                        m.mustMatchMask = aHexToInt(q);
                    if (*q)
                        m.toClear = aHexToInt(q);
                    setHeatSafetyMask(which, m);
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
                    Serial.print("Set Safety mask i=");
                    Serial.print(which);
                    Serial.print(" dc=0x"); Serial.print(m.dontCareMask, HEX);
                    Serial.print(" mm=0x"); Serial.print(m.mustMatchMask, HEX);
                    Serial.print(" tc=0x"); Serial.println(m.toClear, HEX);
#endif
                }
                return true;
                }
            }
        }
#if SCHEDULE_ENTRIES
        else if (toupper(cmd[0]) == 'S' && toupper(cmd[1]) == 'E')
        {   // SE [which] [Celsiusx10] [HOUR] [MINUTE] [DAY-OF-WEEK-MASK]
            q = cmd + 2;
            while (isspace(*q)) q += 1;
            uint8_t which = aDecimalToInt(q);
            if (which >= NUM_SCHEDULE_TEMPERATURE_ENTRIES) return false;
            ScheduleEntry_t se;
            se.degreesCx5 = aDecimalToInt(q) >> 1; // x10 in command, saved as X5
            se.TimeOfDayHour = aDecimalToInt(q);
            se.TimeOfDayMinute = aDecimalToInt(q);
            se.DaysOfWeek = aHexToInt(q);
            se.AutoMode = *q == '1';
            setScheduleEntry(which, se);
            return true;
        }
#endif
#if USE_SERIAL >= SERIAL_PORT_DEBUG
        else if (strcmp(cmd, "CRASH") == 0)
        {
            ProcessCommand(cmd, len);
        }
        else if (toupper(cmd[0]) == 'U' && toupper(cmd[1]) == 'O' && cmd[2] == '=' && cmd[3] == '0' && cmd[4]=='x')
        {
            q = cmd+5;
            uint8_t mask = aHexToInt(q);
            Furnace::UpdateOutputs(mask);
        }
#endif
        return false;
    }

    void setTemperatureCx10(int16_t t, bool autoMode = false);

    void routeCommand(char* cmd, unsigned char len, uint8_t senderid = -1, bool toMe = true)
    {
#if USE_SERIAL >= SERIAL_PORT_VERBOSE
        Serial.print(F("Command: ")); Serial.print(cmd); 
        if (senderid != static_cast<uint8_t>(-1))
        {
            Serial.print(F(" Sender: ")); Serial.println((int)senderid);
        }
        else
            Serial.println();
#endif
        if (toMe && radioConfiguration.ApplyCommand(cmd))
        {
#if USE_SERIAL >= SERIAL_PORT_VERBOSE
            Serial.println(F("Command accepted for radio"));
#endif
        }
        else if (toMe && ProcessCommand(cmd, len))
        {
#if USE_SERIAL >= SERIAL_PORT_VERBOSE
            Serial.println(F("Command accepted for main"));
#endif
        }
        else
        {
            int16_t targetCx10; int16_t actualCx10;
            bool tempOK = hvac->GetTargetAndActual( targetCx10, actualCx10);
            auto tempType = hvac->TypeNumber();
            auto tempMode = hvac->ModeNumber();
            if (hvac->ProcessCommand(cmd, len, senderid, toMe))
            {
#if USE_SERIAL >= SERIAL_PORT_VERBOSE
                Serial.println(F("Command accepted for HVAC"));
#endif
                LCD::printMode(hvac->ModeNameString());
                if (tempOK && hvac->TypeNumber() == tempType && hvac->ModeNumber() != tempMode)
                    setTemperatureCx10(targetCx10);
                InputsToHvacFlag = true;
            }
        }
    }

    void printHvacTemperatures()
    {
        int16_t t; int16_t a;
        if (!hvac->GetTargetAndActual(t, a))
            return;
        if (displayLcdFarenheit)
        {
            t = degreesCx10toDegreesFx10(t);
            a = degreesCx10toDegreesFx10(a);
        }
        char *p = tempToAscii(reportbuf, t);
        *p++ = '/';
        p = tempToAscii(p, a);
        *p++ = 0;
        LCD::printTemperatures(reportbuf);
    }

    void setTemperatureCx10(int16_t t, bool autoMode)
    {
        static char buf[20];
        strcpy(buf, HVAC_SETTINGS);
#if        HVAC_AUTO_CLASS
        if (autoMode)
            strcpy(buf, AUTO_SETTINGS);
#endif
        itoa(t, buf + strlen(buf), 10);
        routeCommand(buf, strlen(buf));
    }
}

//  compile-time reference to last EEPROM address used in this ino file
const int HVAC_EEPROM_START = static_cast<int>(EepromAddresses::TOTAL_EEPROM_USED);

namespace Furnace {
    uint8_t LastOutputWrite;

    void UpdateOutputs(uint8_t mask)
    {
        mask &= OUTPUT_SIGNAL_MASK;
        LastOutputWrite = mask;

        if (HeatSafetyOffTimeActive)
            mask &= HeatSafetyShutoffMask;
        const auto now = millis();
        
        // check compressor short cycling logic
        uint8_t compressorMask = getCompressorMask();
        if (!CompressorOffTimeActive && compressorMask != 0xff)
        {
            bool compressorWasOn = 0 != (OutputRegister & compressorMask);
            bool compressorToBeOff = 0 == (mask & compressorMask);
            if (compressorWasOn && compressorToBeOff)
            {   // this command is turning the compressor off
                CompressorOffTimeActive = true;
                CompressorOffStartTime = now;
            }
        }
        if (CompressorOffTimeActive)
        {   // updating output while off time active
            mask &= ~compressorMask; // ensure compressors not turned on
        }

        // Activate hardware relay if input W doesn't match output W
        //mask &= ~(1 << BN_W_FAILSAFE); // hardware relay already off
        
        /* Deal with possibility that W signal is coming from furnace side. 
        ** Once W relay is pulled in, keep it in for a while to prevent chatter */
        static bool relayIsOn(false);
        static auto onAtTime(now);
        const unsigned long MINIMUM_ON_MSEC = 60000L;
        
        if ((relayIsOn && (now - onAtTime < MINIMUM_ON_MSEC)) ||
            ((((mask & (1 << BN_W)) ^ (InputRegister & (1 << BN_W))) != 0) && 
                (onAtTime = now, relayIsOn = true) //assignments
            )) 
            mask |= 1 << BN_W_FAILSAFE; /// hardware relay on

        OutputRegister = mask;
#if ENABLE_OUTPUT_RELAYS > 0
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("UpdateOutputs: 0x"));
        Serial.println(mask, HEX);
#endif
        SPI.beginTransaction(SPISettings(3000000, MSBFIRST, SPI_MODE0));
        digitalWrite(OUTREG_SPI_CS_PIN, LOW);
        SPI.transfer(mask);
        digitalWrite(OUTREG_SPI_CS_PIN, HIGH);
        SPI.endTransaction();
#endif
    }

    void SetOutputBits(uint8_t mask = 0)
    {   // change only specific bits
        mask |= LastOutputWrite;
        UpdateOutputs(mask);
    }

    void ClearOutputBits(uint8_t mask)
    {   // change only specific bits
        mask = ~mask;
        uint8_t next = LastOutputWrite;
        next &= mask;
        UpdateOutputs(next);
    }
}

uint16_t aDecimalToInt(const char*& p)
{   // p is set to character following terminating non-digit, unless null
    uint16_t ret = 0;
    for (;;)
    {
        auto c = *p;
        if (c >= '0' && c <= '9')
        {
            ret *= 10;
            ret += c - '0';
            p+=1;
        } else 
        {
            if (c != 0)
                p+=1;
            return ret;
        }
    }
}

uint32_t aHexToInt(const char*&p)
{   // p is set to character following terminating non-digit, unless null
    uint32_t ret = 0;
    for (;;)
    {
        auto c = *p;
        if (isxdigit(c))
        {
            ret *= 16;
            if (isdigit(c))
                ret += c - '0';
            else
                ret += 10 + toupper(c) - 'A';
            p+=1;
        } else
        {
            if (c!= 0)
                p+=1;
            return ret;
        }
    }
}

void setup()
{
#if USE_SERIAL > SERIAL_PORT_OFF
    Serial.begin(9600);
#endif
#if USE_SERIAL >= SERIAL_PORT_DEBUG 
    for (uint8_t i = 0; i < 1500; i++)
        if (Serial.read() >= 0)
            break;
        else
            delay(10);
    Serial.println(F("PacketThermostat DEBUG"));
#elif USE_SERIAL >= SERIAL_PORT_OFF 
    Serial.println(F("PacketThermostat Rev03"));
#endif

    digitalWrite(OUTREG_SPI_CS_PIN, HIGH);
    pinMode(OUTREG_SPI_CS_PIN, OUTPUT);
    displayLcdFarenheit = EEPROM.read(static_cast<int>(EepromAddresses::DISPLAY_UNITS_ADDRESS)) != 0;

    Wire.begin();
    SPI.begin();
    Furnace::UpdateOutputs(0);

    // setup radio
    // setup RTC
    if (rtc.begin() == false)
    {
#if USE_SERIAL >= SERIAL_PORT_VERBOSE 
        Serial.println(F("rtc begin FAILED"));
#endif
    } else {
        rtc.set24Hour();
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE 
        Serial.println(F("rtc begin OK!"));
        rtc.setToCompilerTime();
        String ct = rtc.stringTime();
        Serial.println(ct);
#endif
    }

    LCD::init();

    // Initialize the RFM69HCW:
    bool ok = false;
    if (radioConfiguration.NodeId() != 0xff &&
        radioConfiguration.NetworkId() != 0xff)
    {
        ok = radio.initialize(radioConfiguration.FrequencyBandId(),
            radioConfiguration.NodeId(), radioConfiguration.NetworkId());
        if (ok)
        {
            uint32_t freq;
            if (radioConfiguration.FrequencyRaw(freq))
                radio.setFrequencyRaw(freq);
            radio.spyMode(true);
            radioSetupOK = radio.getFrequencyRaw() != 0;
        }
#if USE_SERIAL >= SERIAL_PORT_DEBUG
        Serial.println(ok ? "Radio init OK!" : "Radio init FAILED");
        radioPrintInfo();
#endif   
    }
    else
    {
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.println("Radio EEPROM not setup");
        radioPrintInfo();
#endif   
    }

    if (radioSetupOK)
    {
        radio.setHighPower(); // Always use this for RFM69HCW
        // Turn on encryption if so configured:
        const char* key = radioConfiguration.EncryptionKey();
        if (radioConfiguration.encrypted())
            radio.encrypt(key);
    }
    delay(1000);
    LCD::printBanner(radioSetupOK ? "Radio OK" : "Radio No Good");

    analogReference(DEFAULT); // 3.3V full scale at 10 bits, which is 1023

    pinMode(PCB_INPUT_X1_PIN, INPUT_PULLUP);
    pinMode(PCB_INPUT_X2_PIN, INPUT_PULLUP);
    pinMode(PCB_INPUT_ZX_PIN, INPUT_PULLUP);
    pinMode(PCB_INPUT_W_PIN, INPUT_PULLUP);
    pinMode(PCB_INPUT_Z1_PIN, INPUT_PULLUP);
    pinMode(PCB_INPUT_Z2_PIN, INPUT_PULLUP);
    pinMode(PCB_INPUT_R_ACTIVE_PIN, INPUT_PULLUP);

    ThermostatCommon::setup();

    wdt_enable(WDTO_8S);
}

void loop()
{   wdt_reset();
    const auto now = millis();
    static_assert(sizeof(now) == sizeof(msec_time_stamp_t), "msec_time_stamp_t must match type of millis()");
    auto previousInputRegister = InputRegister;
    auto previousOutputRegister = OutputRegister;

    {   // every second (or so) update the RTC time on the LCD
        const unsigned long LCD_TIME_UPDATE_MSEC = 1000;
        static unsigned long lastLCDupdate;
        static bool firstTime=true;
        static_assert(sizeof(lastLCDupdate) == sizeof(now), "lastLCDupdate wrong size");
        int diff = now - lastLCDupdate;
        if (diff > LCD_TIME_UPDATE_MSEC)
        {
            lastLCDupdate = now;
            if (firstTime)
                firstTime = false;
            else
            {
                rtc.updateTime();
                uint8_t hrs = rtc.getHours();
                uint8_t min = rtc.getMinutes();
                char *p = reportbuf;
                if (hrs < 10)
                    *p++ = '0';
                else
                {
                    *p++ = '0' + hrs / 10;
                    hrs %= 10;
                }
                *p++ = '0' + hrs;
                *p++ = ':';
                if (min < 10)
                    *p++ = '0';
                else
                {
                    *p++ = '0' + min / 10;
                    min %= 10;
                }
                *p++ = '0' + min;
                *p++ = 0;
                LCD::printTime(&reportbuf[0]);
                printHvacTemperatures();
                LCD::printCompressorHold(CompressorOffTimeActive ? "H" : "");
            }
        }
    }

#if SCHEDULE_ENTRIES
    {   // check schedule slightly faster than once per minute
        const unsigned long SCEDULE_TIME_UPDATE_MSEC = 40000; // less than one minute
        static unsigned long lastScheduleUpdate;
        static_assert(sizeof(lastScheduleUpdate) == sizeof(now), "lastScheduleUpdate wrong size");
        int diff = now - lastScheduleUpdate;
        if (diff > SCEDULE_TIME_UPDATE_MSEC)
        {
            lastScheduleUpdate = now;
            rtc.updateTime();
            uint8_t hrs = rtc.getHours();
            uint8_t mins = rtc.getMinutes();
            int weekday = rtc.getWeekday();
            for (uint8_t i = 0; i < NUM_SCHEDULE_TEMPERATURE_ENTRIES; i++)
            {
                auto se = getScheduleEntry(i);
                if ((0 != ((int)se.DaysOfWeek & (1 << weekday))) &&
                    hrs == static_cast<uint8_t>(se.TimeOfDayHour) &&
                    mins == static_cast<uint8_t>(se.TimeOfDayMinute))
                {
                    setTemperatureCx10((int)se.degreesCx5 << 1, se.AutoMode);
                    LCD::reinit = true;
                }
            }
        }
    }
#endif

    if (CompressorOffTimeActive && (now - CompressorOffStartTime) > 1000L * getCompressorHoldSeconds())
    {   // deal with possible expiration of the compressor short cycle prevention timer
        CompressorOffTimeActive = false;
        Furnace::SetOutputBits();
    }

    if (HeatSafetyOffTimeActive)
    {
        if ((now - HeatSafetyOffStartTime) > 1000L * getHeatSafetyHoldSeconds())
        {
            HeatSafetyOffTimeActive = false;
            LCD::printBanner(hvac->ModeNameString());
            Furnace::SetOutputBits();
        }
    }
    if (!HeatSafetyOffTimeActive)
    {   // check inlet temperature in heat modes and shut down if EEPROM settings say so
        auto heatSafetySeconds = getHeatSafetyHoldSeconds();
        if (heatSafetySeconds > 0 && heatSafetySeconds != static_cast<uint16_t>(0xffff))
        {
            auto heatSafetyTempCx10 = getHeatSafetyTemperatureCx10();
            if (heatSafetyTempCx10 > 0)
            {
                if (heatSafetyTempCx10 <= TinletTemperatureCx10)
                {   // safety triggerred
                    for (uint8_t i = 0; i < NUM_HEAT_SAFETY_ENTRIES; i++)
                    {
                        HeatSafetyMask_t m = getHeatSafetyMask(i);
                        uint8_t bits = ~m.dontCareMask & Furnace::LastOutputWrite;
                        if (bits == m.mustMatchMask && m.toClear != 0)
                        { // table indicates this IS a heat mode, so shut down heat
                            HeatSafetyOffStartTime = millis();
                            HeatSafetyOffTimeActive = true;
                            LCD::printBanner(HeatSafetyBanner);
                            HeatSafetyShutoffMask = ~m.toClear;
                            Furnace::SetOutputBits();
                            break;
                        }
                    }
                }
            }
        }
    }

    uint8_t inputsAsRead = 0;
    {   // read input signals. mask definitions correspond with those in OutputRegister
        uint8_t TempInputRegister = 0;
        if (digitalRead(PCB_INPUT_X1_PIN) == LOW)
            TempInputRegister |= 1 << BN_X1;
        if (digitalRead(PCB_INPUT_X2_PIN) == LOW)
            TempInputRegister |= 1 << BN_X2;
        if (digitalRead(PCB_INPUT_Z1_PIN) == LOW)
            TempInputRegister |= 1 << BN_Z1;
        if (digitalRead(PCB_INPUT_Z2_PIN) == LOW)
            TempInputRegister |= 1 << BN_Z2;
        if (digitalRead(PCB_INPUT_W_PIN) == LOW)
            TempInputRegister |= 1 << BN_W;
        if (digitalRead(PCB_INPUT_ZX_PIN) == LOW)
            TempInputRegister |= 1 << BN_ZX;
        if (digitalRead(PCB_INPUT_R_ACTIVE_PIN) == LOW)
            TempInputRegister |= 1 << BN_R;

        // deal with 60Hz AC to input signal conversion
        static msec_time_stamp_t LastSeenActive[NUMBER_OF_SIGNALS]; 
        static msec_time_stamp_t LastChecked; 
        static msec_time_stamp_t RestartedChecking; // some functions in loop() take longer than INPUT_AC_ACTIVE_MIN_MSEC
        // when restarting checking after missing that long, don't turn off outputs until we've seen enough inputs

        if (now - LastChecked >= INPUT_AC_ACTIVE_MIN_MSEC / 4) 
            RestartedChecking = now;

        uint8_t mask = 1;
        for (uint8_t i = 0; i < NUMBER_OF_SIGNALS; i++, mask <<= 1)
        {
            if (TempInputRegister & mask)
            {
                LastSeenActive[i] = now;
                InputRegister |= mask;
            }
            else 
            {
                if (static_cast<unsigned long>(now - LastSeenActive[i]) >= INPUT_AC_ACTIVE_MIN_MSEC &&
                    static_cast<unsigned long>(now - RestartedChecking) >= INPUT_AC_ACTIVE_MIN_MSEC)
                    InputRegister &= ~mask;
            }
        }
        LCD::backLight(0 != (InputRegister & (1 << BN_R)));
        LastChecked = now;
        inputsAsRead = InputRegister;
        if (previousInputRegister != InputRegister || InputsToHvacFlag) // hvac can CHANGE inputregister
        {
            hvac->OnInputsChanged(InputRegister, previousInputRegister);
            Furnace::SetOutputBits();
        }
        InputsToHvacFlag = false;
    }

    {   // An ADC read takes a large number of cycles. Spread the 3 of them out through multiple loops
        static unsigned long lastTemperatureAcquireMsec;
        static enum {DO_T_INLET, DO_T_OUTLET, DO_OUTSIDE, DO_REPORT, WAIT_REPORT_INTERVAL} temperatureAcquireState;
        uint16_t diffMsec = now - lastTemperatureAcquireMsec;
        switch (temperatureAcquireState)
        {
            // the ADC reads are done on consecutive loop() calls
            case DO_T_INLET:
                if (diffMsec >= POLL_ADC_MSEC)
                {   // stay in D_T_INLETT for POLL_ADC_MSEC msec
                    auto vTi = analogRead(T_LM235_INLET_PIN); // Pro Micro has 10bit A/D
                    TinletADCsum += vTi;
                    temperatureAcquireState = DO_T_OUTLET;
                }
                break;
            case DO_T_OUTLET:
                {
                    auto vTi = analogRead(T_LM235_OUTLET_PIN); // Pro Micro has 10bit A/D
                    ToutletADCsum += vTi;
                    temperatureAcquireState = DO_OUTSIDE;
                }
                break;
            case DO_OUTSIDE:
                 {
                    auto vTi = analogRead(S1_7089U_OUTSIDE_PIN); // Pro Micro has 10bit A/D
                    TexternalADCsum += vTi;
                    numberReadsInSum += 1;
                    if (numberReadsInSum >= NUMBER_TEMPERATURE_ADC_READS_TO_AVERAGE)
                        temperatureAcquireState = DO_REPORT;
                    else
                        temperatureAcquireState = DO_T_INLET;
                    lastTemperatureAcquireMsec = now;
                }
                break;
            case DO_REPORT:
                {
                    radioTemperatureReport(TinletADCsum, ToutletADCsum, TexternalADCsum);
                    TinletTemperatureCx10 = degreesCx10fromLM235ADCx64(TinletADCsum);
                    lastTemperatureAcquireMsec = now;
                    temperatureAcquireState  = WAIT_REPORT_INTERVAL;
                }
                break;
            case WAIT_REPORT_INTERVAL:
                if (diffMsec >= BETWEEN_REPORTING_TEMPERTURE_MSEC)
                {
                    TexternalADCsum = 0;
                    ToutletADCsum = 0;
                    TinletADCsum = 0;
                    numberReadsInSum = 0;
                    lastTemperatureAcquireMsec = now;
                    temperatureAcquireState = DO_T_INLET;
                }
                break;
        }
    }

    // check for Serial input
#if USE_SERIAL >= SERIAL_PORT_PROMPT_ONLY
    while (Serial.available())
    {
        auto c = Serial.read();
        if (c < 0)
            break;
        auto ch = static_cast<char>(c);
        bool isRet = ch == '\n' || ch == '\r';
        if (!isRet)
            cmdbuf[charsInBuf++] = ch;
        cmdbuf[charsInBuf] = 0;
        if (isRet || charsInBuf >= CMD_BUFLEN - 1)
        {
            routeCommand(cmdbuf, charsInBuf);
            Serial.println(F("ready>"));
            charsInBuf = 0;
        }
    }
#endif

    // check radio
    if (radio.receiveDone()) // Got a packet over the radio
    {   // RFM69 ensures no trailing zero byte when buffer is full
        memset(reportbuf, 0, sizeof(reportbuf));
        memcpy(reportbuf, &radio.DATA[0], sizeof(radio.DATA));
        bool toMe = radioConfiguration.NodeId() == radio.TARGETID;
        routeCommand(reportbuf, sizeof(radio.DATA), static_cast<uint8_t>(radio.SENDERID), toMe);
        if (toMe && radio.ACKRequested())
            radio.sendACK();
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("FromRadio: \""));
        Serial.print(reportbuf);
        Serial.print(F("\" sender:"));
        Serial.print(radio.SENDERID);
        Serial.print(" target:");
        Serial.println(radio.TARGETID);
#endif
    }

    hvac->loop(now);

    if (LCD::reinit)
    {   // the LCD display seems to get out of sync. Force a full update of it occasionally
        LCD::printBanner(HeatSafetyOffTimeActive ? HeatSafetyBanner : hvac->ModeNameString());
        lcdHvacReport(OutputRegister & OUTPUT_SIGNAL_MASK);
        LCD::reinit = false;
    }

    LCD::loop(now);

    if (  ((previousInputRegister  & INPUT_SIGNAL_MASK ) != (InputRegister  & INPUT_SIGNAL_MASK)) 
        || (previousOutputRegister != OutputRegister ))
        radioHvacReport(inputsAsRead, OutputRegister);
}

/* macros to simplify compile-time generation of the C7089U coefficients that are optimized for least run-time
** arduino computation. */

#define ADCfromRESISTANCE(resistance) ((static_cast<double>(NUMBER_TEMPERATURE_ADC_READS_TO_AVERAGE) * (resistance) * ADCmaxVoltageCount * IsenseAmps / ADCmaxVoltage))
#define FtoC(farenheit) (((farenheit) - 32.0) * 5.0 / 9.0)
#define MAKEC7089U(farenheit, celsius, resistance, Rprev, Tprev) { static_cast<int16_t>(0.5 + FtoC(farenheit)*10), \
    static_cast<uint16_t>(0.5 + ADCfromRESISTANCE(resistance)), \
    static_cast<uint16_t>(0.5 + ((1L << POWER2_C7089SlopeScale) * 10.0 * (FtoC(farenheit) - FtoC(Tprev)) / (ADCfromRESISTANCE(Rprev) - ADCfromRESISTANCE(resistance))))  \
    } 

namespace
{
    // the Rset for the LM334 is 2.2K, which the data sheet says gives 67.7mV/2.2K = 30.77 uA
    const double IsenseAmps = .0677 / 2200;
    // these coefficients are copied and pasted from the Honeywell C7089U specification, with
    // the addition of two columns. These extra columns are the resistance and the temperature
    // repeated one line lower, which allows compile-time generation of the "slope" entry in
    // the table.
    const C7089UCoefficient_t C7089UCoefficients[NUM_C7089_COEFFICIENTS] PROGMEM =
    {
#if 0
    /*  With a 3.3V ADC and a 2.2K Rset (=30.7uA), the maximum R that can be digitized is 107.5K ohms */
    MAKEC7089U(-40	,	-40	,	195652	,	-1	,	-1),
    MAKEC7089U(-38.2	,	-39	,	184917	,	195652	,	-40),
    MAKEC7089U(-36.4	,	-38	,	174845	,	184917	,	-38.2),
    MAKEC7089U(-34.6	,	-37	,	165391	,	174845	,	-36.4),
    MAKEC7089U(-32.8	,	-36	,	156512	,	165391	,	-34.6),
    MAKEC7089U(-31	,	-35	,	148171	,	156512	,	-32.8),
    MAKEC7089U(-29.2	,	-34	,	140330	,	148171	,	-31),
    MAKEC7089U(-27.4	,	-33	,	132957	,	140330	,	-29.2),
    MAKEC7089U(-25.6	,	-32	,	126021	,	132957	,	-27.4),
    MAKEC7089U(-23.8	,	-31	,	119493	,	126021	,	-25.6),
    MAKEC7089U(-22	,	-30	,	113347	,	119493	,	-23.8),
#endif
    MAKEC7089U(-20	,	-28.9	,	106926	,	113347	,	-22), /* adc: 65345 */
    MAKEC7089U(-18	,	-27.8	,	100923	,	106926	,	-20), /* adc: 61671*/
    MAKEC7089U(-16	,	-26.7	,	95310	,	100923	,	-18),
    MAKEC7089U(-14	,	-25.6	,	90058	,	95310	,	-16),
    MAKEC7089U(-12	,	-24.4	,	85124	,	90058	,	-14),
    MAKEC7089U(-10	,	-23.3	,	80485	,	85124	,	-12),
    MAKEC7089U(-8	,	-22.2	,	76137	,	80485	,	-10),
    MAKEC7089U(-6	,	-21.1	,	72060	,	76137	,	-8),
    MAKEC7089U(-4	,	-20	,	68237	,	72060	,	-6),
    MAKEC7089U(-2	,	-18.9	,	64631	,	68237	,	-4),
    MAKEC7089U(0	,	-17.8	,	61246	,	64631	,	-2),
    MAKEC7089U(2	,	-16.7	,	58066	,	61246	,	0),
    MAKEC7089U(4	,	-15.6	,	55077	,	58066	,	2),
    MAKEC7089U(6	,	-14.4	,	53358	,	55077	,	4),
    MAKEC7089U(8	,	-13.3	,	49598	,	53358	,	6),
    MAKEC7089U(10	,	-12.2	,	47092	,	49598	,	8),
    MAKEC7089U(12	,	-11.1	,	44732	,	47092	,	10),
    MAKEC7089U(14	,	-10	,	42506	,	44732	,	12),
    MAKEC7089U(16	,	-8.9	,	40394	,	42506	,	14),
    MAKEC7089U(18	,	-7.8	,	38400	,	40394	,	16),
    MAKEC7089U(20	,	-6.7	,	36519	,	38400	,	18),
    MAKEC7089U(22	,	-5.6	,	34743	,	36519	,	20),
    MAKEC7089U(24	,	-4.4	,	33063	,	34743	,	22),
    MAKEC7089U(26	,	-3.3	,	31475	,	33063	,	24),
    MAKEC7089U(28	,	-2.2	,	29975	,	31475	,	26),
    MAKEC7089U(30	,	-1.1	,	28558	,	29975	,	28),
    MAKEC7089U(32	,	0	,	27219	,	28558	,	30),
    MAKEC7089U(34	,	1.1	,	25949	,	27219	,	32),
    MAKEC7089U(36	,	2.2	,	24749	,	25949	,	34),
    MAKEC7089U(38	,	3.3	,	23613	,	24749	,	36),
    MAKEC7089U(40	,	4.4	,	22537	,	23613	,	38),
    MAKEC7089U(42	,	5.6	,	21516	,	22537	,	40),
    MAKEC7089U(44	,	6.7	,	20546	,	21516	,	42),
    MAKEC7089U(46	,	7.8	,	19626	,	20546	,	44),
    MAKEC7089U(48	,	8.9	,	18754	,	19626	,	46),
    MAKEC7089U(50	,	10	,	17926	,	18754	,	48),
    MAKEC7089U(52	,	11.1	,	17136	,	17926	,	50),
    MAKEC7089U(54	,	12.2	,	16387	,	17136	,	52),
    MAKEC7089U(56	,	13.3	,	15675	,	16387	,	54),
    MAKEC7089U(58	,	14.4	,	14999	,	15675	,	56),
    MAKEC7089U(60	,	15.6	,	14356	,	14999	,	58),
    MAKEC7089U(62	,	16.7	,	13743	,	14356	,	60),
    MAKEC7089U(64	,	17.8	,	13161	,	13743	,	62),
    MAKEC7089U(66	,	18.9	,	12607	,	13161	,	64),
    MAKEC7089U(68	,	20	,	12081	,	12607	,	66),
    MAKEC7089U(70	,	21.1	,	11578	,	12081	,	68),
    MAKEC7089U(72	,	22.2	,	11100	,	11578	,	70),
    MAKEC7089U(74	,	23.3	,	10644	,	11100	,	72),
    MAKEC7089U(76	,	24.4	,	10210	,	10644	,	74),
    MAKEC7089U(78	,	25.6	,	9795	,	10210	,	76),
    MAKEC7089U(80	,	26.7	,	9398	,	9795	,	78),
    MAKEC7089U(82	,	27.8	,	9020	,	9398	,	80),
    MAKEC7089U(84	,	28.9	,	8659	,	9020	,	82),
    MAKEC7089U(86	,	30	,	8315	,	8659	,	84),
    MAKEC7089U(88	,	31.1	,	7986	,	8315	,	86),
    MAKEC7089U(90	,	32.2	,	7672	,	7986	,	88),
    MAKEC7089U(92	,	33.3	,	7372	,	7672	,	90),
    MAKEC7089U(94	,	34.4	,	7086	,	7372	,	92),
    MAKEC7089U(96	,	35.6	,	6813	,	7086	,	94),
    MAKEC7089U(98	,	36.7	,	6551	,	6813	,	96),
    MAKEC7089U(100	,	37.8	,	6301	,	6551	,	98),
    MAKEC7089U(102	,	38.9	,	6062	,	6301	,	100),
    MAKEC7089U(104	,	40	,	5834	,	6062	,	102),
    MAKEC7089U(106	,	41.1	,	5614	,	5834	,	104),
    MAKEC7089U(108	,	42.2	,	5404	,	5614	,	106),
    MAKEC7089U(110	,	43.3	,	5203	,	5404	,	108),
    MAKEC7089U(112	,	44.4	,	5010	,	5203	,	110),
    MAKEC7089U(114	,	45.6	,	4826	,	5010	,	112),
    MAKEC7089U(116	,	46.7	,	4649	,	4826	,	114),
    MAKEC7089U(118	,	47.8	,	4479	,	4649	,	116),
    MAKEC7089U(120	,	48.9	,	4317	,	4479	,	118),
    MAKEC7089U(122	,	50	,	4160	,	4317	,	120),
    MAKEC7089U(123.8	,	51	,	4026	,	4160	,	122),
    MAKEC7089U(125.6	,	52	,	3896	,	4026	,	123.8), // adc: 2375
    MAKEC7089U(127.4	,	53	,	3771	,	3896	,	125.6), // adc: 2299
    };
}
