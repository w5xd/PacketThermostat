#include "SerialPortWin.h"
namespace PacketThermostat {
SerialPort::SerialPort(const char *commPortName, unsigned baudrate) : 
    m_commPortName(commPortName),
    m_BaudRate(baudrate),
    m_CommPort(INVALID_HANDLE_VALUE)
{}

SerialPort::~SerialPort()
{
    if (m_CommPort != INVALID_HANDLE_VALUE)
        ::CloseHandle(m_CommPort);
}

int SerialPort::OpenCommPort()
{
    if (m_CommPort != INVALID_HANDLE_VALUE)
        ::CloseHandle(m_CommPort);
    std::string fname = std::string("\\\\.\\") + m_commPortName;
    m_CommPort = CreateFileA(fname.c_str(),
                    GENERIC_READ|GENERIC_WRITE,
                    0, 0, OPEN_EXISTING, 
                    FILE_ATTRIBUTE_NORMAL, 0);
    if (m_CommPort == INVALID_HANDLE_VALUE)
        return -1;

    DCB Dcb;
	memset(&Dcb, 0, sizeof(Dcb));
	Dcb.DCBlength = sizeof(Dcb);
	Dcb.BaudRate = m_BaudRate;
	Dcb.StopBits = ONESTOPBIT;
	Dcb.Parity = NOPARITY;
	Dcb.fParity = 0;
	Dcb.ByteSize = 8;
	Dcb.fBinary = TRUE;
	Dcb.fOutxCtsFlow = 0;
	Dcb.fOutxDsrFlow = 0;
	Dcb.fOutX = 0;
	Dcb.fInX = 0;
	Dcb.fNull = 0;
	Dcb.fDtrControl = DTR_CONTROL_DISABLE;
	Dcb.fRtsControl = RTS_CONTROL_DISABLE;
	if (SetCommState(m_CommPort, &Dcb) == 0)
        return -2;
    if (SetupComm(m_CommPort, 200, 200) == 0)    // queue length in and out
        return -3;
    COMMTIMEOUTS CommTimeOuts;
    memset(&CommTimeOuts, 0, sizeof(CommTimeOuts));
    CommTimeOuts.ReadIntervalTimeout = 0; // 100x as long as a char
    CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
    CommTimeOuts.ReadTotalTimeoutConstant = 100;    //allowed pause for response
	if (SetCommTimeouts(m_CommPort, &CommTimeOuts) == 0)
        return -4;

    EscapeCommFunction(m_CommPort, SETDTR);
    EscapeCommFunction(m_CommPort, SETRTS);
    return 0;
}

bool SerialPort::Read(unsigned char *rbuf, unsigned sizeToRead, unsigned *nrr)
{
    DWORD nr;
    BOOL res = ::ReadFile(m_CommPort, rbuf, sizeToRead, &nr, 0);
    *nrr = nr;
    return res ? true : false;  // true is success
}

bool SerialPort::Write(const unsigned char *v, unsigned s)
{
    DWORD nw;
    if ((::WriteFile(m_CommPort, (void *)v, s, &nw, 0)==0)
        || (nw != s)) return false;
    return true;
}
}