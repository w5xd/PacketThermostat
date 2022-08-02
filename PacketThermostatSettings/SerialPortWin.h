#pragma once
#include <windows.h>
#include <string>

namespace PacketThermostat {
class SerialPort
{
public:
        SerialPort(const char *commPortName, unsigned baudrate = 19200);
        ~SerialPort();
        int OpenCommPort();
        bool Read(unsigned char *, unsigned, unsigned *);
        bool Write(const unsigned char *, unsigned);
        bool Write(const std::string &s) { return Write(reinterpret_cast<const unsigned char *>(s.c_str()), static_cast<unsigned>(s.size()));}
        const std::string &commPortName()const {return m_commPortName;}
protected:
        const std::string m_commPortName;
        const unsigned m_BaudRate;
        HANDLE  m_CommPort;
};
}
