/* Command line application to configure the packet thermostat EEPROM for a particular set of signal wires and functionality.
**

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

/*
** The Packet Thermostat sketch and PCB are generic in the sense they treat the thermostat-to-furnace wiring as a set of
** up to 6 generic 24VAC signals. This application sets the EEPROM in the Arduino to a specific mapping of those generic
** signals to ones for a typical thermosat. 
**
** A good primer on how thermostats are wired:
** https://www.epatest.com/store/resources/images/misc/how-a-thermostat-operates.pdf
*/

#include <iostream>
#include <memory>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include <PacketThermostat/PcbSignalDefinitions.h>
#ifdef WIN32
#include "SerialPortWin.h"
#else
#include "SerialPortLinux.h"
#endif

/* The mapping of PCB/sketch pins to thermostat signals here is five signals:
** X1 is  O is the compressor reversing valve, with "ON" calling for cool, OFF for heat.
** W  is  W, the furnace
** X2 is  Y, the (only or stage 1) compressor
** Z1  is G, the fan
** Z2 is  Y2, the stage 2 compressor
** ZX is DH, dehumidify
**
** R and C are 24VAC and common, respectively
*/ 

// The furnace packet radio is on this Node ID
#define FURNACE_NODEID "99"

namespace {
    const uint8_t MASK_O = 1 << BN_X1;
    const uint8_t MASK_W = 1 << BN_W;
    const uint8_t MASK_Y = 1 << BN_X2;
    const uint8_t MASK_Y2 = 1 << BN_Z2;
    const uint8_t MASK_G = 1 << BN_Z1;
    const uint8_t MASK_DH = 1 << BN_ZX;

    int doConfigure(PacketThermostat::SerialPort &, int argc, char **argv);
    int doSetMode(PacketThermostat::SerialPort &, int argc, char **argv);
}

struct WaitFailed : public std::runtime_error
{
    WaitFailed(const std::string &e) : std::runtime_error(e)
    {}
};

int main(int argc, char **argv)
{
    static const char *USAGE1 = 
        "usage: PacketThermostatSettings <COMMPORT> <[CONFIGURE] | [SETMODE [PASS | NOHP | HEAT | COOL | EHEAT] <TEMPERATURE F>]> ";
    static const char *USAGE2 = 
        "COMMPORT for CONFIGURE is the thermostat. COMMPORT for SETMODE is PacketGateway";
    if (argc < 3)
    {
        std::cerr << USAGE1 << std::endl;
        std::cerr << USAGE2 << std::endl;
        return 1;
    }
    static const int BAUD = 9600;
    PacketThermostat::SerialPort sp(argv[1], BAUD);
    if (sp.OpenCommPort() < 0)
    {
        std::cerr << "failed to open Serial Port" << std::endl;
        return 1;
    }

    std::string cmdUpper;
    const char *p = argv[2];
    while (*p)
        cmdUpper.push_back(toupper(*p++));

    try {
        if (cmdUpper == "CONFIGURE")
            return doConfigure(sp, argc, argv);
        else if (cmdUpper == "SETMODE")
            return doSetMode(sp, argc, argv);
    }
    catch (const WaitFailed &e)
    {
        std::cerr << "Serial command failed: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error " << e.what() << std::endl;
    }

    std::cerr << "Unknown command: " << argv[2] << std::endl;
    return 1;
}

namespace {

 void WaitForReady(const std::string &error, PacketThermostat::SerialPort &sp)
{   // the firmware on the packet thermostat sends "ready>" on its serial port after processing a command
    static const char SCAN_FOR_READY[] = "ready>";
    static const unsigned NUM_READ_LOOPS = 10;
    std::string scan;
    for (unsigned j = 0; j < NUM_READ_LOOPS; j++)
    {
        unsigned sizeRead;
        unsigned char buf[100];
        if (!sp.Read(&buf[0], sizeof(buf) - 1, &sizeRead)) // 100msec time out
            continue;
        for (unsigned i = 0; i < sizeRead; i++)
        {
            char c = (char)buf[i];
            scan.push_back(isalpha(c) ? tolower(c) : c);
            std::cout << c;
            if (scan.find(std::string(SCAN_FOR_READY)) != scan.npos)
            {
                std::cout << std::endl;
                return;
            }
        }
        while (scan.size() > sizeof(SCAN_FOR_READY))
            scan.erase(scan.begin());
    }
    throw WaitFailed(error);
}

 void DoCommandAndWait(const std::string &cmd, PacketThermostat::SerialPort &sp)
{
     for (int i = 0; i < 15; i++)
     {// this is just a timed delay
         unsigned char buf[1]; unsigned sze;
         for (; sp.Read(&buf[0], sizeof(buf), &sze), sze != 0;); // 100msec time out
     }
     sp.Write(cmd + '\r');
     WaitForReady(cmd, sp);
}

 int doConfigure(PacketThermostat::SerialPort &sp, int argc, char **argv)
{
    // name the wires
    DoCommandAndWait("HV R Y2 G W d Y O", sp);

    uint8_t compressorMask = MASK_Y | MASK_Y2;
    {
        static const int COMPRESSOR_HOLD_SECONDS = 5 * 60;
        std::ostringstream setCompressorMask;
        setCompressorMask << "COMPRESSOR=0x";
        setCompressorMask << std::hex << static_cast<int>(compressorMask);
        setCompressorMask << " " << std::dec << COMPRESSOR_HOLD_SECONDS;
        DoCommandAndWait(setCompressorMask.str(), sp);
    }

    // name the PassThrough mode as PasT
    DoCommandAndWait("HVAC TYPE=0 MODE=0", sp);
    DoCommandAndWait("HVAC NAME=PasT", sp);
    DoCommandAndWait("HVAC COMMIT", sp);

    // mapping mode to disable heat pump****************************************************
    DoCommandAndWait("HVAC TYPE=1 COUNT=1", sp);    // set up a mapping mode 
    DoCommandAndWait("HVAC TYPE=1 MODE=0", sp); // switch to the newly created mode
    DoCommandAndWait("HVAC NAME=NoHP", sp); // name the mode NoHP

    static const int SIGNAL_COMBINATIONS = 1 << NUM_HVAC_INPUT_SIGNALS;
    unsigned char map[SIGNAL_COMBINATIONS];
    for (unsigned i = 0; i < SIGNAL_COMBINATIONS; i++)
    {
        static const uint8_t HEATPUMP_MASK = MASK_Y | MASK_Y2;
        map[i] = i << 1; // initialize map to input to output. Shift-by-one cuz signals start one bit shifted left
        uint8_t item = map[i];
        if ((0 != (item & HEATPUMP_MASK)) && (0 == (item & MASK_O)))// Either of Y or Y2 without O?
        {
            map[i] &= ~HEATPUMP_MASK; // turn off heat pump
            map[i] |= MASK_W;   // turn on furnace
        }
    }

    unsigned i = 0;
    for (unsigned j = 0; j < 8; j++) // spread the map into 8 commands to limit buffer size to what fits
    {
        std::ostringstream oss;
        oss << "HVACMAP=0x" << std::hex << i << " ";
        for (int m = 7; m >= 0; m -= 1)
        {
            oss << std::hex << static_cast<int>(map[i++]) ;
            if (m != 0)
                oss << " ";
        }
        DoCommandAndWait(oss.str(), sp);
    }
    DoCommandAndWait("HVAC COMMIT", sp); // into EEPROM on the Packet Thermostat

    uint32_t sensorMask = 0;
    for (int i = 0; i < argc; i++)
    {   // scan command line for -s
        if (strcmp(argv[i], "-s") == 0)
        {
            i += 1;
            if (i < argc)
            {
                int sensor = atoi(argv[i]);
                sensorMask |= 1 << sensor;
            }
        }
    }

    // HEAT mode
    DoCommandAndWait("HVAC TYPE=2 COUNT=2", sp);    // set up a mapping mode 
    DoCommandAndWait("HVAC TYPE=2 MODE=0", sp); // switch to the newly created mode
    DoCommandAndWait("HVAC NAME=HEAT", sp); // name the mode 

    {
        std::ostringstream heatSettings;
        heatSettings << "HVAC_SETTINGS 1 0"; // temperatures are 0.1C off, 0.0C on
        heatSettings << " " << std::hex << sensorMask;  // thermometer mask
        heatSettings << " " << std::hex << (int)(MASK_G); // fan mask
        heatSettings << " " << std::hex << (int)MASK_DH; // always on (dehumidify--reverse logic)
        heatSettings << " " << std::hex << (int)(MASK_Y|MASK_G | MASK_DH); // heat stage 1
        heatSettings << " " << std::hex << (int)(MASK_Y|MASK_Y2|MASK_G | MASK_DH); // heat state 2 
        heatSettings << " " << std::hex << (int)(MASK_W | MASK_DH); // heat stage 3 (switch to furnace only

        int secondsToStage2Heat = 60 * 15; // 15 minutes of stage 1 by default
        heatSettings << " " << std::dec << secondsToStage2Heat; // 

        int secondsToStage3Heat = 60 * 5;
        for (int i = 0; i < argc; i++)
        {
            if (strcmp(argv[i], "-ss3") == 0)
            {
                i += 1;
                if (i < argc)
                    secondsToStage3Heat = atoi(argv[i]);
            }
        }

        heatSettings << " " << std::dec << secondsToStage2Heat + secondsToStage3Heat; /// seconds to stage 3

        DoCommandAndWait(heatSettings.str(), sp);
    }
    DoCommandAndWait("HVAC COMMIT", sp);

    // wHEAT mode
    DoCommandAndWait("HVAC TYPE=2 MODE=1", sp); // switch to the newly created mode
    DoCommandAndWait("HVAC NAME=wHEAT", sp); // name the mode

    {
        std::ostringstream heatSettings;
        heatSettings << "HVAC_SETTINGS 1 0"; // temperatures are 0.1C off, 0.0C on
        heatSettings << " " << std::hex << sensorMask;  // thermometer mask
        heatSettings << " " << std::hex << (int)(MASK_G); // fan mask
        heatSettings << " " << std::hex << (int)MASK_DH; // always on (dehumidify--reverse logic)
        heatSettings << " " << std::hex << (int)(MASK_W | MASK_DH); // heat stage 1
        heatSettings << " " << std::hex << (int)(MASK_W | MASK_DH); // heat state 2 (same as stage 1)
        heatSettings << " " << std::hex << (int)(MASK_W | MASK_DH); // heat stage 3 (switch to furnace only
        heatSettings << " " << std::dec << 10; // second stage matches 1, so short timeout
        heatSettings << " " << std::dec << 20; // 3 stage matches 2, so short timeout
        DoCommandAndWait(heatSettings.str(), sp);
    }

    DoCommandAndWait("HVAC COMMIT", sp);

    // COOL mode
    DoCommandAndWait("HVAC TYPE=3 COUNT=1", sp);    // set up a mapping mode 
    DoCommandAndWait("HVAC TYPE=3 MODE=0", sp); // switch to the newly created mode
    DoCommandAndWait("HVAC NAME=COOL", sp); // name the mode

    {
        std::ostringstream coolSettings;
        coolSettings << "HVAC_SETTINGS 400 410"; // temperatures are 40C off, 41C on
        coolSettings << " " << std::hex << sensorMask;  // thermometer mask
        coolSettings << " " << std::hex << (int)(MASK_G); // fan mask
        coolSettings << " " << std::hex << (int)(MASK_O | MASK_DH); // always ON
        coolSettings << " " << std::hex << (int)(MASK_O | MASK_DH | MASK_Y | MASK_G); // cool stage 1
        coolSettings << " " << std::hex << (int)(MASK_O | MASK_DH | MASK_Y2 | MASK_Y | MASK_G); // cool stage 2 
        coolSettings << " " << std::hex << (int)(MASK_O | MASK_DH | MASK_Y2 | MASK_Y | MASK_G); // cool stage 3 
        coolSettings << " " << std::dec << 1200; // stage 1 timeout. 20 minutes
        coolSettings << " " << std::dec << 9999; // 3 stage matches 2
        DoCommandAndWait(coolSettings.str(), sp);
    }
    {
        std::ostringstream dehumidify;
        dehumidify << "HUM_SETTINGS";
        dehumidify << " " << std::dec << 600; // 60% humidity target
        dehumidify << " " << std::hex << (int)0; // turn ON no bits
        dehumidify << " " << std::hex << (int)(MASK_DH); // turn OFF the DH wire.
        DoCommandAndWait(dehumidify.str(), sp); // no demudify function.
    }

    DoCommandAndWait("HVAC COMMIT", sp);

    return 0;
}

 int doSetMode(PacketThermostat::SerialPort &sp, int argc, char **argv)
{
    if (argc < 4)
        return 1;
    std::string mode;
    const char *p = argv[3];
    while (*p)
        mode.push_back(toupper(*p++));
    if (mode == "PASS")
    {
        sp.Write("SendMessageToNode " FURNACE_NODEID " HVAC TYPE=0 MODE=0\r");
        return 0;
    } else if (mode == "NOHP")
    {
        sp.Write("SendMessageToNode " FURNACE_NODEID " HVAC TYPE=1 MODE=0\r");
        return 0;
    } else
        std::cerr << "Unknown SETMODE command: " << argv[3] << std::endl;

    return 1;
}

}