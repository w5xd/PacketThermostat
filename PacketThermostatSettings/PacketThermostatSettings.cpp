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
#include <functional>
#include <stdexcept>
#include <algorithm>

#include <PacketThermostat/PcbSignalDefinitions.h>
#ifdef WIN32
#include "SerialPortWin.h"
#else
#include "SerialPortLinux.h"
#endif
#ifdef min 
#undef min
#endif
#ifdef max
#undef max
#endif

class SerialWrapper {
public:
    SerialWrapper() : m_readState(0)
    {
        m_write = [this] (const std::string &s)
        {
            std::cout << s << std::endl;
            m_readState = 0;
            return true;
        };
        m_read = [this] (unsigned char* buf, unsigned len, unsigned* bytesRead)
        {
            if (m_readState == 0)
            {
                static const char READY[] = "ready>";
                strncpy_s(reinterpret_cast<char*>(buf), len, READY, len-1);
                *bytesRead = std::min(len, static_cast<unsigned>(sizeof(READY) / sizeof(READY[0])));
            }
            else
                *bytesRead = 0;
            m_readState = 1;
            return true;
        };
    }
    SerialWrapper(std::unique_ptr<PacketThermostat::SerialPort> &sp) : m_readState(0)
    {
        m_sp = std::move(sp);
        // The Write method is overloaded, so std::bind needs some compile-time help
        m_write = std::bind(static_cast<bool (PacketThermostat::SerialPort:: *)(const std::string&)>(&PacketThermostat::SerialPort::Write), 
            m_sp.get(), std::placeholders::_1);
        m_read = std::bind(&PacketThermostat::SerialPort::Read, 
            m_sp.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    }
    bool Write(const std::string &s)
    {
        return m_write(s);
    }
    bool Read(unsigned char*buf, unsigned len, unsigned*bytesRead)
    {
        return m_read(buf, len, bytesRead);
    }
protected:
    std::function<bool(const std::string &)> m_write;
    std::function<bool(unsigned char* , unsigned , unsigned* )> m_read;
    std::unique_ptr<PacketThermostat::SerialPort> m_sp;
    int m_readState;
};

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

namespace {
    const uint8_t MASK_W = 1 << BN_W;
    const uint8_t MASK_Y = 1 << BN_X2;
    const uint8_t MASK_Y2 = 1 << BN_Z2;
    const uint8_t MASK_G = 1 << BN_Z1;
    const uint8_t MASK_DH = 1 << BN_ZX;

    // The default for this program is to support the O wire reversing valve logic. Command line -B switches to B wire logic.
    uint8_t MASK_O = 1 << BN_X1; 
    uint8_t MASK_B = 0;

    int doConfigure(SerialWrapper&, int argc, char **argv);
}

struct WaitFailed : public std::runtime_error
{
    WaitFailed(const std::string &e) : std::runtime_error(e)
    {}
};


int main(int argc, char **argv)
{
    static const char *USAGE1 = 
        "usage: PacketThermostatSettings [<COMMPORT> | - ] CONFIGURE -s <thermometer#1> -s <thermometer#2> ... -s <thermometer#n>";
    if (argc < 3)
    {
        std::cerr << USAGE1 << std::endl;
        return 1;
    }

    std::unique_ptr<SerialWrapper> sp;

    if (strcmp(argv[1],"-") == 0) // write commands to STDOUT instead of COM port
    {
        sp.reset(new SerialWrapper());
    }
    else
    {
        static const int BAUD = 9600;
        std::unique_ptr<PacketThermostat::SerialPort> port(new PacketThermostat::SerialPort(argv[1], BAUD));
        if (port->OpenCommPort() < 0)
        {
            std::cerr << "failed to open Serial Port" << std::endl;
            return 1;
        }
        sp.reset(new SerialWrapper(port));
    }

    std::string cmdUpper;
    const char *p = argv[2];
    while (*p)
        cmdUpper.push_back(toupper(*p++));

    try {
        if (cmdUpper == "CONFIGURE")
            return doConfigure(*sp.get(), argc, argv);
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

 void WaitForReady(const std::string &error, SerialWrapper &sp)
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

 void DoCommandAndWait(const std::string &cmd, SerialWrapper&sp)
{
     for (int i = 0; i < 15; i++)
     {// this is just a timed delay
         unsigned char buf[1]; unsigned sze;
         for (; sp.Read(&buf[0], sizeof(buf), &sze), sze != 0;); // 100msec time out
     }
     sp.Write(cmd + '\r');
     WaitForReady(cmd, sp);
}

 int doConfigure(SerialWrapper&sp, int argc, char **argv)
{
     std::string wireNames = "HV R Y2 G W d Y O";
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
         else if (strcmp(argv[i], "-B") == 0)
         {
             wireNames = "HV R Y2 G W d Y B";
             MASK_B = 1 << BN_X1;
             MASK_O = 0;
         }
     }

    // name the wires
    DoCommandAndWait(wireNames, sp);

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
        const uint8_t COMPRESSOR_MASK = MASK_Y | MASK_Y2;
        map[i] = i << 1; // initialize map to input to output. Shift-by-one cuz signals start one bit shifted left
        uint8_t item = map[i];
        if ((0 != (item & COMPRESSOR_MASK))  // detects compressor on: Y or Y2
            && (0 == (item & MASK_O))// without O?
            && (MASK_B == (item & MASK_B))// with B?
            )
        {
            map[i] &= ~COMPRESSOR_MASK; // turn off heat pump
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

    // HEAT mode
    DoCommandAndWait("HVAC TYPE=2 COUNT=2", sp);    // set up a mapping mode 
    DoCommandAndWait("HVAC TYPE=2 MODE=0", sp); // switch to the newly created mode
    DoCommandAndWait("HVAC NAME=HEAT", sp); // name the mode 

    {
        std::ostringstream heatSettings;
        heatSettings << "HVAC_SETTINGS 1 0"; // temperatures are 0.1C off, 0.0C on
        heatSettings << " " << std::hex << sensorMask;  // thermometer mask
        heatSettings << " " << std::hex << (int)(MASK_G); // fan mask
        heatSettings << " " << std::hex << (int)(MASK_B | MASK_DH); // always on (dehumidify--reverse logic)
        heatSettings << " " << std::hex << (int)(MASK_B | MASK_Y | MASK_G | MASK_DH); // heat stage 1
        heatSettings << " " << std::hex << (int)(MASK_B | MASK_Y | MASK_Y2 | MASK_G | MASK_DH); // heat state 2 
        heatSettings << " " << std::hex << (int)(MASK_B | MASK_W | MASK_DH); // heat stage 3 (switch to furnace only

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
        heatSettings << " " << std::dec << (60 * 20); // stage 2 timeout is ALSO used by thermostat to notice thermometer timeout: 20 minutes
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

    // Heat mode safety check. Force furnace off if intake temperature exceeds setting
    DoCommandAndWait("HS T 300", sp); // heat safety timeout 5 minutes. once triggered, off this long
    DoCommandAndWait("HS C 322", sp); // heat safety temperature 32.2C (about 90F)

    {
        std::ostringstream safety1;
        // if W is on, force it off
        uint8_t dontCare = ~MASK_W;
        uint8_t mustMatchMask  = MASK_W;
        uint8_t toClear = MASK_Y | MASK_Y2 | MASK_W;

        safety1 << "HS 1 " << std::hex << static_cast<int>(dontCare) << " " << 
                              std::hex << static_cast<int>(mustMatchMask) << " " <<
                              std::hex << static_cast<int>(toClear);
        DoCommandAndWait(safety1.str(), sp);

        dontCare = ~(MASK_Y | MASK_O | MASK_B); // Y and O or B are what we DO care about.
        mustMatchMask = MASK_Y | MASK_B ; // compressor ON, and reversing valve is HEAT
        std::ostringstream safety2;
        safety2 << "HS 2 " << std::hex << static_cast<int>(dontCare) << " " <<
            std::hex << static_cast<int>(mustMatchMask) << " " <<
            std::hex << static_cast<int>(toClear);
        DoCommandAndWait(safety2.str(), sp);

        DoCommandAndWait("HS 3", sp);
    }

    {
        // clear all schedule entries
        const int NUM_SCHEDULE_ENTRIES = 16;
        for (int i = 0; i < NUM_SCHEDULE_ENTRIES; i++)
        {
            std::ostringstream oss;
            oss << "SE " << i;
            DoCommandAndWait(oss.str(), sp);
        }
    }

    return 0;
}

}