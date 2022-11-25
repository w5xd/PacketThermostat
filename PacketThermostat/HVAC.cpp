/*

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
THE SOFTWARE.

*/

#include <Arduino.h>
#include <EEPROM.h>
#include "ThermostatCommon.h"


/* The various HVAC implementations are a mix of classes with subclasses, each itself being table-driven.
** There is a trivial class, PassThrough, and four (interesting) classes.
** Each of the four classes has runtime parameters that themselves are saved in EEPROM tables.
** 
** The command sequence for setting these tables up in EEPROM is:
**    (1) Set the number of EEPROM slots for the type of interest:
**              HVAC TYPE=n COUNT=m       where n goes from 1 through 4, and m is limited by EEPROM's 1024 byte size
**    (2) Set the current HVAC mode to one of the created slots:
**              HVAC TYPE=n MODE=m      where m must be less than what was set as COUNT=
**    (3) Fill in the parameters for the given TYPE. 
**          (a) HVAC NAME=xyz       for all types, including PASSTHROUGH
**          (b) HVACMAP=0x          for the MapInputToOutput (TYPE=1)
**          (c) HVAC_SETTINGS       for all the remaining classes (TYpe= 2, 3 and 4)
**          (d) HUM_SETTINGS              needed an addition for the COOL setting for the AUTO mode
*           (e) AUTO_SETTINGS       for AUTO
**    (4)   COMMIT command.         All the above set only the current memory and are lost on PCB power down. COMMIT 
**    puts the settings to EEPROM to survive power down.
**
** The sketch .ino file allows for the above commands to be sent either by Serial or by the packet radio.
*/

class HvacCommands; // base class                   
class PassThrough; // The trivial type is PassThrough

/* MapInputToOuput honors its thermostat inputs, but not unconditionally like PassThrough.
** Each combination of inputs is mapped through EEPROM settings to an output */
class MapInputToOutput;

/* OverrideAndDriveFromSensors ignores thermostat inputs and instead
** drives the furnace signals from what it sees in incoming radio packets.*/
class OverrideAndDriveFromSensors;
class HvacHeat; // subclass of OverrideAndDriveFromSensors
class HvacCool; // subclass of OverrideAndDriveFromSensors
class HvacAuto; // subclass of HvacCool -- switches between heat/cool

namespace
{   // support these types of mappings from available inputs to furnace outputs:
    enum HvacTypes { HVAC_PASSTHROUGH, HVAC_MAPINPUTTOOUTPUT, HVAC_HEAT, HVAC_COOL, 
#if HVAC_AUTO_CLASS
        HVAC_AUTO,
#endif
        NUMBER_OF_HVAC_TYPES };
    const int NUM_INPUT_SIGNAL_COMBINATIONS = 1 << NUM_HVAC_INPUT_SIGNALS;

    const int HVAC_EEPROM_TYPE_AND_MODE_ADDR = HVAC_EEPROM_START; // .ino source tells this C++ module where to start
    const int HVAC_SAVED_TYPE_AND_MODE_SIZE = 2; // the thermostat remembers it TYPE and MODE settings across power down. 2 bytes to save that

    const int HVAC_NUMBER_OF_MODES_IN_TYPE_ADDR = HVAC_EEPROM_TYPE_AND_MODE_ADDR + HVAC_SAVED_TYPE_AND_MODE_SIZE; 
    const int NUMBER_OF_HVAC_TYPES_IN_EEPROM = NUMBER_OF_HVAC_TYPES - 1; // PASSTHROUGH only has one MODE, so don't save it

    const int HVAC_MODES_EEPROM_START_ADDR = HVAC_NUMBER_OF_MODES_IN_TYPE_ADDR + NUMBER_OF_HVAC_TYPES_IN_EEPROM;
    
    extern HvacCommands* const ThermostatModeTypes[]; // forward declar

    uint8_t NumberOfModesInType(HvacTypes t)
    {   // The EEPROM settings for the various HVAC modes allow for a variable number of each type.
        if (t == HVAC_PASSTHROUGH)
            return 1; // exactly one PassThrough as it only has a NAME and no other parameters
        // one byte per type
        uint16_t addr = static_cast<int>(t) - 1;
        addr +=  HVAC_NUMBER_OF_MODES_IN_TYPE_ADDR;
        uint8_t ret = EEPROM.read(addr);
        if (ret == static_cast<uint8_t>(0xff))
            return 0;
        return ret;
    }

    void SetNumberOfModesInType(HvacTypes t, uint8_t count)
    {   // set in EEPROM
        if (t == HVAC_PASSTHROUGH)
            return; // cannot set PassThrough count
        uint16_t addr = static_cast<int>(t) - 1;
        addr +=  HVAC_NUMBER_OF_MODES_IN_TYPE_ADDR;
        EEPROM.write(addr, count);
    }

    uint16_t AddressOfModeTypeSettings(HvacTypes t, uint8_t which);
    const int NAME_LENGTH = 5; // without trailing null
}

const char HVAC_SETTINGS[] = "HVAC_SETTINGS ";
#if HVAC_AUTO_CLASS
const char AUTO_SETTINGS[] = "AUTO_SETTINGS"; // This is the AUTO heat setting only
#endif

/* These classes have a lot of member variables that are static.
** This is an optimization to keep memory usage down by taking
** advantage of the detail that only one of the instances is active at a time.
** The "static" attribute overlays much of the data in memory which,
** in C++ best practices, is generally a bad idea. Its like having global
** variables. */

class HvacCommands : public ThermostatCommon
{
public:
    struct Settings {
        char ModeName[NAME_LENGTH + 1]; // space for trailing null in memory, but not in EEPROM
    };
    void InitFromEeprom(uint8_t mode)
    {
        MyModeNumber = mode;
        ReadSettings();
    }
    virtual void TurnFurnaceOff() {  Furnace::UpdateOutputs(0); }
protected:
    // implement some of the pure virtuals from interface class
    bool ProcessCommand(const char* cmd, uint8_t len, uint8_t senderid, bool toMe) override;
    const char* ModeNameString() override {  return settingsFromEeprom.ModeName; }
    bool GetTargetAndActual(int16_t& targetCx10, int16_t& actualCx10) override { return false; }
    void loop(msec_time_stamp_t) override {}

     // add pure virtuals for subclasses
    virtual void CommitSettings() = 0; 
    virtual void ReadSettings() = 0;
    virtual void InitializeState() {}; // subclasses need not do this one

    uint16_t WriteEprom(uint16_t addr)
    {
 #if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("HvacCommands::WriteEprom a=0X"));
        Serial.println(addr, HEX);
#endif
        EEPROM.put(addr, settingsFromEeprom);
        auto ret = addr + sizeof(settingsFromEeprom);
#if USE_SERIAL >= SERIAL_PORT_VERBOSE
        int remaining = EEPROM.length();
        remaining -= ret;
        if (remaining < 0)
            Serial.println(F("ERROR: WriteEprom beyond capacity"));
        else
        {
            Serial.print(F("EEPROM remaining:"));
            Serial.println(remaining);
        }
#endif
        return ret;
    }
    uint16_t ReadEprom(uint16_t addr)
    {
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("HvacCommands::ReadEprom a=0X"));
        Serial.println(addr, HEX);
#endif
        if (EEPROM.read(addr) != (byte)0xff)
            EEPROM.get(addr, settingsFromEeprom); // overwrites in-memory settings
        return addr + sizeof(settingsFromEeprom);
    }
    static Settings settingsFromEeprom;
};

class PassThrough : public HvacCommands
{
public:
    void CommitSettings() override {
        WriteEprom(AddressOfModeTypeSettings(HVAC_PASSTHROUGH, 0));
        return;
    }
    void ReadSettings() override {
        ReadEprom(AddressOfModeTypeSettings(HVAC_PASSTHROUGH, 0));
    }

    void OnInputsChanged(uint8_t inputs, uint8_t previous) override
    {
        uint8_t mask = INPUT_SIGNAL_MASK & inputs;
        Furnace::UpdateOutputs(mask);        // pass the input signals through to the output
    }
};

class MapInputToOutput : public HvacCommands
{   // use a table to store all the possible outputs for a given input
public:
    struct Settings { 
        uint8_t inputToOutput[NUM_INPUT_SIGNAL_COMBINATIONS];
    };
protected:

    void OnInputsChanged(uint8_t inputs, uint8_t previous) override
    {
        inputs &= INPUT_SIGNAL_MASK; // inputs constrained to INPUT_SIGNAL_MASK
        auto value = settingsFromEeprom.inputToOutput[inputs >> BN_FIRST_SIGNAL]; /* shift inputs so they run from 0 through 63 */
        // outputs are constrainted to OUTPUT_SIGNAL_MASK
        if (value == static_cast<uint8_t>(0xff))
            value = inputs;
        Furnace::UpdateOutputs(value);
    }

    bool ProcessCommand(const char* cmd, uint8_t len, uint8_t senderid, bool toMe) override
    {
        if (HvacCommands::ProcessCommand(cmd, len, senderid, toMe))
            return true; // give base class a chance
        if (!toMe) // radio packets are sniffed from thermometers reporting to the gateway
            return false;
        static const char MAP[] = "HVACMAP=0x"; // command to overwrite mapping

        if (strncmp(cmd, MAP, sizeof(MAP) - 1) == 0)
        {   // fill in the map. All numbers in hex
            const char* q = cmd + sizeof(MAP) - 1;
            uint8_t addr = aHexToInt(q); // first number in command is the address
            for (;;)
            {
                if (!*q)
                    break;
                if (addr >= NUM_INPUT_SIGNAL_COMBINATIONS)
                    return false; // mal-formed command tried to write past end of array

                uint8_t v = static_cast<uint8_t>(aHexToInt(q));
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
                Serial.print(F("Map: "));
                Serial.print((int) addr, DEC);
                Serial.print(F(" v=0x"));
                Serial.println((int) v, HEX);
#endif
                settingsFromEeprom.inputToOutput[addr++] = v;
            }
            return true;
        }
        return false;
    }

    void CommitSettings() override    {
        WriteEprom(AddressOfModeTypeSettings(HVAC_MAPINPUTTOOUTPUT, MyModeNumber));
    }
    void ReadSettings() override {
        ReadEprom(AddressOfModeTypeSettings(HVAC_MAPINPUTTOOUTPUT, MyModeNumber));
    }

    uint16_t WriteEprom(uint16_t addr)
    {
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("MapInputToOutput::WriteEprom "));
        for (uint8_t i = 0; i < 16; i++)
        {
            Serial.print((int)settingsFromEeprom.inputToOutput[i], HEX);
            Serial.print(F(" "));
        }
        Serial.println();
#endif
        addr = HvacCommands::WriteEprom(addr);
        EEPROM.put(addr, settingsFromEeprom);
        return addr + sizeof(settingsFromEeprom);
    }
    uint16_t ReadEprom(uint16_t addr)
    {
        addr = HvacCommands::ReadEprom(addr);
        EEPROM.get(addr, settingsFromEeprom);
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("MapInputToOutput: "));
        for (uint8_t i = 0; i < 16; i++)
        {
            Serial.print((int)settingsFromEeprom.inputToOutput[i], HEX);
            Serial.print(F(" "));
        }
        Serial.println();
#endif
        return addr + sizeof(settingsFromEeprom);
    }

    static Settings settingsFromEeprom;
};

static const msec_time_stamp_t SENSOR_TIMEOUT_MSEC = 1000L * 60L * 15L; // 15 minutes

class OverrideAndDriveFromSensors : public HvacCommands
{
public:
    struct Settings { // These are used for both HEAT and COOL
        int16_t TemperatureTargetDegreesCx10; // 10x degrees c. e.g. 37C is 370
        int16_t TemperatureActivateDegreesCx10; // Turn on HVAC at this T. Turn off at Target T
        uint32_t SensorMask;    // sensor ID's we'll honor, up to 32
        uint8_t MaskFanOnly;    // how to turn on the fan
        uint8_t AlwaysOnMask;   // These outputs are set ON continuously in this mode
        uint8_t OutputStage1; // how to turn on Stage 1 
        uint8_t OutputStage2; // and Stage 2
        uint8_t OutputStage3; // and Stage 3
        uint16_t SecondsToSecondStage; // let run with actual beyond Target for this long before going to OutputStage2
        uint16_t SecondsToThirdStage;
    };

    static bool fanContinuous() { return fanIsOn; }

protected:
    void OnInputsChanged(uint8_t inputs, uint8_t previous) override  { return;} // ignore inputs
    void TurnFurnaceOff() override { Furnace::UpdateOutputs(settingsFromEeprom.AlwaysOnMask); }
    struct OffOnExit {
        OffOnExit() : off(0) {}
        ~OffOnExit()
        {
            Furnace::UpdateOutputs(off);
        }
        uint8_t off;
    };
    bool ProcessCommand(const char* cmd, uint8_t len, uint8_t senderid, bool toMe) override
    {
        if (HvacCommands::ProcessCommand(cmd, len, senderid, toMe))
            return true;

        if (toMe)
        {   // Fan on/off commands
            static const char FAN_CMD[] = "HVAC FAN=O";
            const char* p = cmd;
            const char* q = FAN_CMD;
            while (*q)
                if (toupper(*p++) != *q)
                    break;
                else q++;
                
            if (!*q) // matched
            {
                fanIsOn = toupper(*p) == 'N';
                if (fanIsOn)
                    Furnace::SetOutputBits(settingsFromEeprom.MaskFanOnly);
                else if (fancoilState == STATE_OFF)
                    Furnace::ClearOutputBits(settingsFromEeprom.MaskFanOnly);
                return true;
            }

            q = strstr(cmd, HVAC_SETTINGS);
            if (q)
            {   // fill in the thermostat parameters
                // Command looks like this:
                // HVAC_SETTINGS <target temperature C> <activate temperature C> <sensor id mask> <Stage 1 Output> <Stage 2 Output> <Stage 3 Output> <Fan Mask> <Seconds to Stage 2> <seconds to Stage 3>
                // For EXAMPLE, to set the thermostat to COOL with typical mapping of PCB to thermostat wires:
                //             206     to  69F (20.6C)  
                //             211     activate at 70F (21.1), 
                //             300     use sensors 8 and 9
                //              10     The Fan is the G wire, mapped to Z1
                //              04     Keep the O output, mapped to X1, always ON
                //              08     Stages 1, 2 and 3 are all Y output, mapped to X2
                //               1     Seconds to stages 2 and 3 are unimportant, 1 second each
                // HVAC_SETTINGS 206 211 300 10 04 08 08 08 1 1

                OffOnExit offOnExit;
                offOnExit.off = settingsFromEeprom.AlwaysOnMask;
                if (fanIsOn)
                    offOnExit.off |= settingsFromEeprom.MaskFanOnly;
                fancoilState = STATE_OFF;

                q += sizeof(HVAC_SETTINGS) - 1;
                settingsFromEeprom.TemperatureTargetDegreesCx10 = aDecimalToInt(q);
                // default activate temperature if not given
                settingsFromEeprom.TemperatureActivateDegreesCx10 = ActivateTemperatureFromTarget(settingsFromEeprom.TemperatureTargetDegreesCx10);
                if (!*q) return true;
                settingsFromEeprom.TemperatureActivateDegreesCx10 = aDecimalToInt(q);
                if (!*q) return true;
                settingsFromEeprom.SensorMask = aHexToInt(q);
                if (!*q) return true;
                settingsFromEeprom.MaskFanOnly = aHexToInt(q);
                if (!*q) return true;
                offOnExit.off = 
                settingsFromEeprom.AlwaysOnMask = aHexToInt(q);
                if (!*q) return true;
                settingsFromEeprom.OutputStage1 = aHexToInt(q);
                if (!*q) return true;
                settingsFromEeprom.OutputStage2 = aHexToInt(q);
                if (!*q) return true;
                settingsFromEeprom.OutputStage3 = aHexToInt(q);
                if (!*q) return true;
                settingsFromEeprom.SecondsToSecondStage = aDecimalToInt(q);
                if (!*q) return true;
                settingsFromEeprom.SecondsToThirdStage = aDecimalToInt(q);
                return true;
            }
            return false;
        }

        uint32_t mask = 1L << senderid;
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("C command. mask=0x"));
        Serial.print((int)mask, HEX);
        Serial.print(F(" SensorMask=0x"));
        Serial.println((int)settingsFromEeprom.SensorMask, HEX);
#endif
        if (settingsFromEeprom.SensorMask & mask)
        {   // if we're configured to use this sensor
            const auto now = millis();
            if (lastHeardSensorId > 0 &&
                (senderid > lastHeardSensorId) &&
                (static_cast<msec_time_stamp_t>(now - lastHeardFromSensor) < SENSOR_TIMEOUT_MSEC))
                return true; // ignore lower priority sensor if higher one has checked in recently
            lastHeardFromSensor = now;
            lastHeardSensorId = senderid;

            // Example thermometers:
            //      C:49433, B:244, T:+20.37
            //      C:1769, B:198, T:+20.58 R:45.46
            int16_t tCx10 = parseForColon('T', cmd, len);
            if (tCx10 == -1)
                return false;
            int16_t rhx10 = parseForColon('R', cmd, len);
            auto prevState = fancoilState;
            uint8_t output = settingsFromEeprom.AlwaysOnMask;
            bool needToBeOn = OnReceivedTemperatureInput(tCx10);
            previousActual = tCx10;
            if (!needToBeOn)
            {
                fancoilState = STATE_OFF;
                // give subclass a second chance to set the output (HvacAuto)
                output = OnReceivedTemperatureInput2(tCx10, output);
            }
            else
            {
                if (fancoilState == STATE_OFF)
                {
                    fancoilState = STATE_STAGE1;
                    output = settingsFromEeprom.OutputStage1;
                    timeEnteredStage1 = millis();
                }
                else
                {
                    int32_t sinceStage1 = millis() - timeEnteredStage1;
                    if (sinceStage1 >= settingsFromEeprom.SecondsToThirdStage * 1000l)
                        output = settingsFromEeprom.OutputStage3;
                    else if (sinceStage1 >= settingsFromEeprom.SecondsToSecondStage * 1000l)
                        output = settingsFromEeprom.OutputStage2;
                    else
                        output = settingsFromEeprom.OutputStage1;
                }
            }
            if (rhx10 > 0)
                output = OnReceivedHumidityInput(rhx10, tCx10, output);
            if (fanIsOn)
                output |= settingsFromEeprom.MaskFanOnly;
            Furnace::UpdateOutputs(output);
            return true;
        }
        return false;
    }
    
    bool isSensorTimedOut(msec_time_stamp_t now)
    {
        long interval = now - lastHeardFromSensor; // can (and will!) be negative first time through
        bool ret = interval > (settingsFromEeprom.SecondsToThirdStage * 2000l);
        if (ret) {
#if USE_SERIAL >= SERIAL_PORT_VERBOSE
            Serial.print("Sensor timed out! ");
            Serial.println(interval);
#endif
            previousActual = 0;
            TurnFurnaceOff();
            }
        return ret;
     }

    void loop(msec_time_stamp_t now) override
    { 
        // is it time to move to a later stage?
        if (fancoilState != STATE_OFF)
        {
            if (isSensorTimedOut(now))
            {
                fancoilState = STATE_OFF;
                return;
            }
            int32_t sinceStage1 = now - timeEnteredStage1;
            if (sinceStage1 >= settingsFromEeprom.SecondsToThirdStage * 1000l)
            {
                if (fancoilState != STATE_STAGE3)
                {
                    fancoilState = STATE_STAGE3;
                    Furnace::UpdateOutputs(settingsFromEeprom.OutputStage3);
                }
            }
            else if (sinceStage1 >= settingsFromEeprom.SecondsToSecondStage * 1000l)
            {
                if (fancoilState != STATE_STAGE2)
                {
                    fancoilState = STATE_STAGE2;
                    Furnace::UpdateOutputs(settingsFromEeprom.OutputStage2);
                }
            }
        }
    }

    static int16_t parseForColon(char flag, const char* p, uint8_t len)
    {   // help parse the Wireless Thermometer packet
        int16_t ret(0);
        uint8_t c = len;
        for (;;)
        {
            if (!*p)
                return -1;
            if (c == 0)
                return -1;
            if (p[0] == flag && p[1] == ':')
            {
                p += 2;  c -= 2;
                bool neg = false;
                if (*p == '-')
                {
                    neg = true;
                    p += 1;
                    c -= 1;
                } else if (*p == '+')
                {
                    neg = false;
                    p += 1;
                    c -= 1;
                }
                ret = aDecimalToInt(p) * 10;
                if (isdigit(*p))
                    ret += *p - '0';
                if (neg)
                    ret = -ret;
                break;
            } else
            {
                p += 1;
                c -= 1;
            }
        }
        return ret;
    }

    virtual bool OnReceivedTemperatureInput(int16_t degCx10) = 0; // false return means turn off the HVAC
    virtual int16_t ActivateTemperatureFromTarget(int16_t  target) = 0; // 
    virtual uint8_t OnReceivedTemperatureInput2(int16_t degCx10, uint8_t output) { return output;}
    virtual uint8_t OnReceivedHumidityInput(int16_t rhX10, int16_t degCx10, uint8_t mask) { return mask;}

    uint16_t WriteEprom(uint16_t addr)
    {
        addr = HvacCommands::WriteEprom(addr);
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("OverrideAndDriveFromSensors::WriteEprom a=0X"));
        Serial.print(addr, HEX);
        Serial.print(F(" t="));
        Serial.println(settingsFromEeprom.TemperatureTargetDegreesCx10);
#endif
        EEPROM.put(addr, settingsFromEeprom);
        return addr + sizeof(settingsFromEeprom);
    }
    uint16_t ReadEprom(uint16_t addr)
    {
        addr = HvacCommands::ReadEprom(addr);
        EEPROM.get(addr, settingsFromEeprom);
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("OverrideAndDriveFromSensors::ReadEprom a="));
        Serial.print(addr, HEX);
        Serial.print(F(" t="));
        Serial.print(settingsFromEeprom.TemperatureTargetDegreesCx10);
        Serial.print(F(" SensorMask=0x"));
        Serial.println((int)settingsFromEeprom.SensorMask, HEX);
        Serial.print(F(" settingsFromEeprom.SecondsSettingToSecondStage="));
        Serial.println((int)settingsFromEeprom.SecondsToSecondStage, DEC);
#endif
        return addr + sizeof(settingsFromEeprom);
    }

    void InitializeState() override 
    {
        timeEnteredStage1 = 
        lastHeardFromSensor = millis();
        lastHeardSensorId = 0;
        fancoilState = STATE_OFF;
        fanIsOn = false;
        previousActual = 0;
    }

    bool GetTargetAndActual(int16_t& targetCx10, int16_t& actualCx10) override 
    { 
        targetCx10 = settingsFromEeprom.TemperatureTargetDegreesCx10;
        actualCx10 = previousActual;
        return true; 
    }

    static msec_time_stamp_t lastHeardFromSensor; 
    static uint8_t lastHeardSensorId;
    static msec_time_stamp_t timeEnteredStage1; 
    static enum FurnaceState {STATE_OFF, STATE_STAGE1, STATE_STAGE2, STATE_STAGE3} fancoilState;
    static bool fanIsOn;
    static Settings settingsFromEeprom;
    static uint16_t previousActual;
};

class HvacHeat : public OverrideAndDriveFromSensors
{
protected:
    bool OnReceivedTemperatureInput(int16_t degCx10) override
    {
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("HvacHeat::OnReceivedTemperatureInput t="));
        Serial.println(degCx10);
#endif
        if (fancoilState == STATE_OFF)
            return degCx10 <= settingsFromEeprom.TemperatureActivateDegreesCx10;
        else
            return degCx10 < settingsFromEeprom.TemperatureTargetDegreesCx10;
    }
    int16_t ActivateTemperatureFromTarget(int16_t target) override { return target - 6; } // .6 degree C
    void CommitSettings() override    {
        WriteEprom(AddressOfModeTypeSettings(HVAC_HEAT, MyModeNumber));
    }
    void ReadSettings() override {
        ReadEprom(AddressOfModeTypeSettings(HVAC_HEAT, MyModeNumber));
    }
};

class HvacCool : public OverrideAndDriveFromSensors
{
public:
    struct Settings {   // The dehumidfy capability is implemented as a modification of the HvacCool settings.
        uint8_t MaskDehumidifyBitsOn;   // ...so we have "mask" state and not "output"
        uint8_t MaskDehumidifyBitsOff;
        uint16_t HumiditySettingX10; // RH in % x 10. That is, 50% RH is 500.
    };
protected:
    bool OnReceivedTemperatureInput(int16_t degCx10) override
    {
        bool ret = (fancoilState == STATE_OFF) 
          ? degCx10 >= OverrideAndDriveFromSensors::settingsFromEeprom.TemperatureActivateDegreesCx10
          : degCx10 > OverrideAndDriveFromSensors::settingsFromEeprom.TemperatureTargetDegreesCx10;
        return ret;
    }

    int16_t ActivateTemperatureFromTarget(int16_t target) override { return target + 6; } // .6 degree C

    static const int DEHUMIDIFY_HYSTERESIS = 15; // Turn ON at set point + 1.5% and turn off at set point - 1.5%

    uint8_t OnReceivedHumidityInput(int16_t rhX10, int16_t degCx10, uint8_t mask) override
    {
        if (settingsFromEeprom.HumiditySettingX10 == 0xffffu)
            return mask;
        bool needDehumd = 
            dehumidifyState == DEHUMDIFY_OFF 
            ?    rhX10 > settingsFromEeprom.HumiditySettingX10 + DEHUMIDIFY_HYSTERESIS  
            :   rhX10 > settingsFromEeprom.HumiditySettingX10 - DEHUMIDIFY_HYSTERESIS;
        static const int HALF_DEGREE_C = 5;
        if (needDehumd)
        {
            if (degCx10 < OverrideAndDriveFromSensors::settingsFromEeprom.TemperatureActivateDegreesCx10 - HALF_DEGREE_C)
            {
                needDehumd = false; // dehumidifying has lowered temperature 1/2 deg C below target. abandon dehumidify
                dehumidifyState = DEHUMDIFY_OFF;
            }
            else
            {
                mask |= settingsFromEeprom.MaskDehumidifyBitsOn;
                mask &= ~settingsFromEeprom.MaskDehumidifyBitsOff;
                dehumidifyState = DEHUMIDIFY_ACTIVE;
            }
        }
        return mask;
    }

    bool ProcessCommand(const char* cmd, uint8_t len, uint8_t senderid, bool toMe) override
    {
        if (OverrideAndDriveFromSensors::ProcessCommand(cmd, len, senderid, toMe))
            return true;
        if (toMe)
        {
            static const char HUMIDIFY_SETTINGS[] = "HUM_SETTINGS";
            const char *q = strstr(cmd, HUMIDIFY_SETTINGS);
            if (q)
            {
                settingsFromEeprom.HumiditySettingX10 = 0xffffu; // turn it off
                q += sizeof(HUMIDIFY_SETTINGS) - 1;
                if (!*(q++)) return true;
                settingsFromEeprom.HumiditySettingX10 = aDecimalToInt(q); 
                if (!*q) return true;
                settingsFromEeprom.MaskDehumidifyBitsOn = aHexToInt(q);
                if (!*q) return true;
                settingsFromEeprom.MaskDehumidifyBitsOff = aHexToInt(q);
                return true;
            }
        }
        return false;
    }
    void CommitSettings() override    {
        WriteEprom(AddressOfModeTypeSettings(HVAC_COOL, MyModeNumber));
    }
    void ReadSettings() override {
        ReadEprom(AddressOfModeTypeSettings(HVAC_COOL, MyModeNumber));
    }
    uint16_t WriteEprom(uint16_t addr)    {
        addr = OverrideAndDriveFromSensors::WriteEprom(addr);
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("OverrideAndDriveFromSensors::WriteEprom a=0x"));
        Serial.println(addr, HEX);
#endif
        EEPROM.put(addr, settingsFromEeprom);
        return addr + sizeof(settingsFromEeprom);
    }
    uint16_t ReadEprom(uint16_t addr)    {
        addr = OverrideAndDriveFromSensors::ReadEprom(addr);
        EEPROM.get(addr, settingsFromEeprom);
        return addr + sizeof(settingsFromEeprom);
    }
    void InitializeState() override { 
        OverrideAndDriveFromSensors::InitializeState();
        dehumidifyState = DEHUMDIFY_OFF; }
    enum {DEHUMDIFY_OFF, DEHUMIDIFY_ACTIVE} dehumidifyState;
    static Settings settingsFromEeprom;
};

#if HVAC_AUTO_CLASS  
class HvacAuto : public HvacCool
{
public:
    struct Settings {
        int16_t TemperatureTargetHeatDegreesCx10; // 10x degrees c. e.g. 37C is 370
        int16_t TemperatureActivateHeatDegreesCx10; // 10x degrees c. e.g. 37C is 370
        uint8_t HeatMaskStage1; // how to turn on Stage 1 HEAT
        uint8_t HeatMaskStage2; // and Stage 2
        uint8_t HeatMaskStage3; // and Stage 3
    };
protected:
    bool OnReceivedTemperatureInput(int16_t degCx10) override
    {
        auto ret = HvacCool::OnReceivedTemperatureInput(degCx10);
        if (ret)
            autoTarget = OverrideAndDriveFromSensors::settingsFromEeprom.TemperatureTargetDegreesCx10;
        return ret;
    }
    uint8_t OnReceivedTemperatureInput2(int16_t degCx10, uint8_t mask) override
    {
        bool needHeat = 
            (heatState == HEAT_OFF)
                 ? degCx10 <= settingsFromEeprom.TemperatureActivateHeatDegreesCx10
                 : degCx10 < settingsFromEeprom.TemperatureTargetHeatDegreesCx10;
        if (!needHeat)
            heatState = HEAT_OFF;
        else 
        {
            autoTarget = settingsFromEeprom.TemperatureTargetHeatDegreesCx10;
            if (heatState == HEAT_OFF)
            {
                heatState = HEAT_STAGE1;
                mask = settingsFromEeprom.HeatMaskStage1;
                timeEnteredStage1 = millis();
            } else
            {
                int32_t sinceStage1 = millis() - timeEnteredStage1;
                if (sinceStage1 >= OverrideAndDriveFromSensors::settingsFromEeprom.SecondsToThirdStage * 1000l)
                    mask = settingsFromEeprom.HeatMaskStage3;
                else if (sinceStage1 >= OverrideAndDriveFromSensors::settingsFromEeprom.SecondsToSecondStage * 1000l)
                    mask = settingsFromEeprom.HeatMaskStage2;
                else
                    mask = settingsFromEeprom.HeatMaskStage1;
            }
        }
        return mask;
    }
    uint8_t OnReceivedHumidityInput(int16_t rhX10, int16_t degCx10, uint8_t mask) override
    {
        if (heatState == HEAT_OFF)
            return HvacCool::OnReceivedHumidityInput(rhX10, degCx10, mask);
        else
            return mask;
    }
    void loop(msec_time_stamp_t now) override
    {
        if (heatState != HEAT_OFF)
        {
            if (isSensorTimedOut(now))
            {
                heatState = HEAT_OFF;
                return;
            }
            int32_t sinceStage1 = now - timeEnteredStage1;
            if (sinceStage1 >= OverrideAndDriveFromSensors::settingsFromEeprom.SecondsToThirdStage * 1000l)
            {
                if (heatState != HEAT_STAGE3)
                {
                    heatState = HEAT_STAGE3;
                    Furnace::UpdateOutputs(settingsFromEeprom.HeatMaskStage3);
                }
            }
            else if (sinceStage1 >= OverrideAndDriveFromSensors::settingsFromEeprom.SecondsToSecondStage * 1000l)
            {
                if (heatState != HEAT_STAGE2)
                {
                    heatState = HEAT_STAGE2;
                    Furnace::UpdateOutputs(settingsFromEeprom.HeatMaskStage2);
                }
            }
        }
        else 
            HvacCool::loop(now);
    }    
    bool GetTargetAndActual(int16_t& targetCx10, int16_t& actualCx10) override
    {
        targetCx10 = autoTarget;
        actualCx10 = previousActual;
        return true;
    }
    bool ProcessCommand(const char* cmd, uint8_t len, uint8_t senderid, bool toMe) override
    {
        if (HvacCool::ProcessCommand(cmd, len, senderid, toMe))
            return true;
        const char* q = strstr(cmd, AUTO_SETTINGS);
        if (q)
        {
            q += sizeof(AUTO_SETTINGS) - 1;
            if (!*(q++)) return true;
            settingsFromEeprom.TemperatureTargetHeatDegreesCx10 = aDecimalToInt(q);
            settingsFromEeprom.TemperatureActivateHeatDegreesCx10 = 
                settingsFromEeprom.TemperatureTargetHeatDegreesCx10 - 6;
            if (!*q)
                return true;
            settingsFromEeprom.TemperatureActivateHeatDegreesCx10 = aDecimalToInt(q);
            if (!*q)
                return true;
            settingsFromEeprom.HeatMaskStage1 = settingsFromEeprom.HeatMaskStage2 = settingsFromEeprom.HeatMaskStage3 = aHexToInt(q);
            if (!*q) return true;
            settingsFromEeprom.HeatMaskStage2 = aHexToInt(q);
            if (!*q) return true;
            settingsFromEeprom.HeatMaskStage3 = aHexToInt(q);
            return true;
        }
        return false;
    }
    void CommitSettings() override    {
        WriteEprom(AddressOfModeTypeSettings(HVAC_AUTO, MyModeNumber));
    }
    void ReadSettings() override {
        ReadEprom(AddressOfModeTypeSettings(HVAC_AUTO, MyModeNumber));
    }
    uint16_t WriteEprom(uint16_t addr)
    {
        addr = HvacCool::WriteEprom(addr);
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("HvacAuto::WriteEprom a=0x"));
        Serial.println(addr, HEX);
#endif
        EEPROM.put(addr, settingsFromEeprom);
        return addr + sizeof(settingsFromEeprom);
    }
    uint16_t ReadEprom(uint16_t addr)
    {
        addr = HvacCool::ReadEprom(addr);
        EEPROM.get(addr, settingsFromEeprom);
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("HvacAuto::ReadEprom t="));
        Serial.print(settingsFromEeprom.TemperatureTargetHeatDegreesCx10);
        Serial.print(" a=0x");
        Serial.println(addr, HEX);
#endif
        return addr + sizeof(settingsFromEeprom);
    }

    void InitializeState() override {
        HvacCool::InitializeState();
        heatState = HEAT_OFF;
        autoTarget = settingsFromEeprom.TemperatureTargetHeatDegreesCx10;
    }
    enum {HEAT_OFF, HEAT_STAGE1, HEAT_STAGE2, HEAT_STAGE3} heatState;
    static Settings settingsFromEeprom;
    static int16_t autoTarget;
};
#endif
namespace
{
    //  need exactly one instance of each thermostat type
    PassThrough passThrough;
    MapInputToOutput mapInputToOutput;
    HvacHeat hvacHeat;
    HvacCool hvacCool;
#if HVAC_AUTO_CLASS
    HvacAuto hvacAuto;
#endif

    HvacCommands* const ThermostatModeTypes[NUMBER_OF_HVAC_TYPES] =
    {   // Order must match enum HvacTypes
        &passThrough,
        &mapInputToOutput,
        &hvacHeat,
        &hvacCool,
#if HVAC_AUTO_CLASS
        &hvacAuto
#endif
    };

    uint16_t AddressOfModeTypeSettings(HvacTypes t, uint8_t which)
    {
        if (which > NumberOfModesInType(t)) // allow asking for address of one past the last
            return -1;
        // calculate the size of the Settings, taking into account inheritance.
        size_t sze = sizeof(HvacCommands::Settings); // all inherit from HvacCommands
        switch (t)
        {
        case HVAC_MAPINPUTTOOUTPUT:
            sze += sizeof(MapInputToOutput::Settings);
            break; // remainder do not inherit from MapInputToOutput

#if HVAC_AUTO_CLASS
        case HVAC_AUTO:
            sze += sizeof(HvacAuto::Settings); // inherits from those below
            // fall through cuz inherits from
#endif
        case HVAC_COOL:
            sze += sizeof(HvacCool::Settings);
            // fall through cuz inherits from
        case HVAC_HEAT:
            sze += sizeof(OverrideAndDriveFromSensors::Settings);
            break;
        default:
            break;
        }

        uint16_t ret = -1;

        switch (t)
        {
        case HVAC_PASSTHROUGH:
            ret = HVAC_MODES_EEPROM_START_ADDR + which * sze;
            break;
        case HVAC_MAPINPUTTOOUTPUT:
            ret = AddressOfModeTypeSettings(HVAC_PASSTHROUGH, NumberOfModesInType(HVAC_PASSTHROUGH)) + which * sze;
            break;
        case HVAC_HEAT:
            ret = AddressOfModeTypeSettings(HVAC_MAPINPUTTOOUTPUT, NumberOfModesInType(HVAC_MAPINPUTTOOUTPUT)) + which * sze;
            break;
        case HVAC_COOL:
            ret = AddressOfModeTypeSettings(HVAC_HEAT, NumberOfModesInType(HVAC_HEAT)) + which * sze;
            break;
#if HVAC_AUTO_CLASS
        case HVAC_AUTO:
            ret = AddressOfModeTypeSettings(HVAC_COOL, NumberOfModesInType(HVAC_COOL)) + which * sze;
            break;
#endif
        default:
            return -1;
        }
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print("AddressOfModeTypeSettings type=");
        Serial.print(static_cast<int>(t));
        Serial.print(" which=");
        Serial.print((int)which);
        Serial.print(" sze=");
        Serial.print(sze);
        Serial.print(" ret=");
        Serial.print(ret);
        Serial.print(" start=");
        Serial.println(HVAC_EEPROM_START);
#endif
        return ret;
    }
}

ThermostatCommon* hvac = &passThrough;

void ThermostatCommon::setup()
{
    uint8_t thermoType = EEPROM.read(HVAC_EEPROM_TYPE_AND_MODE_ADDR);
    uint8_t thermoMode = EEPROM.read(HVAC_EEPROM_TYPE_AND_MODE_ADDR+1);
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
    Serial.print("ThermostatCommon::setup() type=");
    Serial.print((int)thermoType);
    Serial.print(" mode=");
    Serial.println((int)thermoMode);
#endif
    if (thermoType < NUMBER_OF_HVAC_TYPES && 
        thermoMode < NumberOfModesInType(static_cast<HvacTypes>(thermoType)))
    {
        HvacCommands *temp;
        hvac = temp = ThermostatModeTypes[thermoType];
        HvacCommands::MyModeNumber = thermoMode;
        HvacCommands::MyTypeNumber = thermoType;
        temp->InitFromEeprom(thermoMode);
        temp->TurnFurnaceOff();
    }
}

bool HvacCommands::ProcessCommand(const char* cmd, uint8_t len, uint8_t senderid, bool toMe)
{
    if (!toMe)
        return false;

    static const char HVAC_COMMAND[] = "HVAC ";
    static const char TYPE_COMMAND[] = "TYPE=";
    static const char MODE_COMMAND[] = "MODE=";
    static const char COUNT_COMMAND[] = "COUNT="; // WARNING. This command invalidates all previously saved eepromSettings!!!!
    static const char COMMIT_COMMAND[] = " COMMIT";
    static const char NAME_COMMAND[] = "NAME=";

    const char* p = cmd;
    const char* q = HVAC_COMMAND;
    while (*q)
    {
        if (toupper(*p++) != *q++)
            return false;
    }

    uint16_t hvacType(-1);
    q = strstr(cmd, TYPE_COMMAND);
    if (q)
    {
        q += sizeof(TYPE_COMMAND) - 1;
        hvacType = aDecimalToInt(q);
        if (hvacType >= NUMBER_OF_HVAC_TYPES)
            return false;
    }

    q = strstr(cmd, NAME_COMMAND);
    if (q)
    {
        q += sizeof(NAME_COMMAND) - 1;
        char* name = settingsFromEeprom.ModeName;
        uint8_t count(0);
        while (*q && !isspace(*q) && count < NAME_LENGTH)
        {
            *name++ = *q++;
            count += 1;
        }
        *name++ = 0;
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("HVAC ModeName=\""));
        Serial.print(settingsFromEeprom.ModeName);
        Serial.println("\"");
#endif
        return true;
    }

    q = strstr(cmd, COMMIT_COMMAND);
    if (q)
    {
        q += sizeof(COMMIT_COMMAND) - 1;
        if (*q && !isspace(*q))
            return false;
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("Commit MODE="));
        Serial.println(MyModeNumber);
#endif
        CommitSettings();
        return true;
    }

#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
    Serial.print(F("hvacType="));
    Serial.println(hvacType);
#endif
    // remainder of the commands depend on TYPE
    if (hvacType >= NUMBER_OF_HVAC_TYPES)
        return false;

    auto tp = static_cast<HvacTypes>(hvacType);

    q = strstr(cmd, MODE_COMMAND);
    if (q)
    {
        q += sizeof(MODE_COMMAND) - 1;
        auto mode = aDecimalToInt(q);
        if (mode >= NumberOfModesInType(static_cast<HvacTypes>(hvacType)))
            return false;

        if (MyModeNumber != mode || MyTypeNumber != static_cast<uint8_t>(hvacType))
        {
            MyModeNumber = mode;
            MyTypeNumber = static_cast<uint8_t>(hvacType);
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
            uint16_t addr = AddressOfModeTypeSettings(tp, mode);
            Serial.print(F("HvacCommands::ProcessCommand addr="));
            Serial.print(addr);
            Serial.print(F(" mode:"));
            Serial.println(mode);
#endif
            HvacCommands *temp;
            hvac = temp = ThermostatModeTypes[hvacType];
            temp->InitializeState();
            temp->ReadSettings();
            EEPROM.write(HVAC_EEPROM_TYPE_AND_MODE_ADDR, hvacType);
            EEPROM.write(HVAC_EEPROM_TYPE_AND_MODE_ADDR+1, MyModeNumber);
            temp->TurnFurnaceOff(); // turn furnace off now
        }
        return true;
    }

    q = strstr(cmd, COUNT_COMMAND);
    if (q)
    {
        q += sizeof(COUNT_COMMAND) - 1;
        auto count = aDecimalToInt(q);
        SetNumberOfModesInType(tp, count);
#if USE_SERIAL >= SERIAL_PORT_SETME_DEBUG_TO_SEE
        Serial.print(F("SetNumberOfModesInType tp="));
        Serial.print(hvacType);
        Serial.print(" c=");
        Serial.println(count);
#endif
        return true;
    }
    return false;
}

// C++ static variables must be instanced
HvacCommands::Settings HvacCommands::settingsFromEeprom = {
    {'P', 'A', 'S', 'S'}
};

uint8_t ThermostatCommon::MyModeNumber;
uint8_t ThermostatCommon::MyTypeNumber;
char ThermostatCommon::fanContinuous() { return MyTypeNumber >= static_cast<int>(HVAC_HEAT) ? (OverrideAndDriveFromSensors::fanContinuous() ? '1' : '0') : '-' ; }
MapInputToOutput::Settings MapInputToOutput::settingsFromEeprom;

OverrideAndDriveFromSensors::Settings OverrideAndDriveFromSensors::settingsFromEeprom;
msec_time_stamp_t OverrideAndDriveFromSensors::lastHeardFromSensor; 
uint8_t OverrideAndDriveFromSensors::lastHeardSensorId;
msec_time_stamp_t OverrideAndDriveFromSensors::timeEnteredStage1; 
OverrideAndDriveFromSensors::FurnaceState OverrideAndDriveFromSensors::fancoilState;
bool OverrideAndDriveFromSensors::fanIsOn;
uint16_t OverrideAndDriveFromSensors::previousActual = 0;

HvacCool::Settings HvacCool::settingsFromEeprom;

#if HVAC_AUTO_CLASS
HvacAuto::Settings HvacAuto::settingsFromEeprom;
int16_t HvacAuto::autoTarget;
#endif
