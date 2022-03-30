CC_FLAGS = -g -O2 -pthread -c -fPIC -DPIC -DLINUX -I..
OBJECTS = SerialPortLinux.o PacketThermostatSettings.o
all: PacketThermostatSettings

clean: rm *.o PacketThermostatSettings

.cpp.o:
	g++ $(CC_FLAGS) $< -o $@

PacketThermostatSettings: $(OBJECTS) 
	g++ -fPIC -DPIC  $(OBJECTS) -O2 -pthread  -o PacketThermostatSettings

