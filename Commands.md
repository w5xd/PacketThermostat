<h2>Commands</h2>

<p>Commands are text sent to the PacketThermostat
by either its USB serial port, or over its packet
radio interface. In the following descriptions, the commands are described. The case of the
letters matters and must be as documented. Items in angle brackets like this: &lt;YYYY&gt;
(usually) must be present and the YYYY is the name of the thing you must enter, and is documented
with the command. For this example, YYYY might be a four digit year. Individual commands
below note when an item in angle brackets can be omitted, and its always the case
that if one is missing, then all after it in the command are also missing.</p> 

<p>Many of these commands write results that are saved in the Packet Thermostat's
EEPROM, and many, but not all, such commands are most easily set up 
using the PacketThermostatSettings application published here in this same
repository. PacketThermostatSettings is coded for a particular mapping
of thermostat control wires to the PacketThermostat's input and output pins. 
</p>

<ul>
<li><code>T=&lt;YYYY&gt; &lt;MM&gt; &lt;DD&gt; &lt;HH&gt; &lt;MM&gt; &lt;SS&gt; &lt;DOW&gt;</code><br/>
Sets the real time clock to the year, month, day, hours,
minutes, and seconds. Year is four digits, month is 1 through 12,
day is 1 through 31, hours is 0 through 23, minutes and seconds are
0 through 59. DOW is day-of-week and is from 0 through 6 (for
Sunday through Saturday).</li>
<li><code>I</code><br/>
Prints out on the USB serial port what the radio parameters
are set to. Except this command prints nothing if 
the C++ preprocessor setting has eliminated the printout
to preserve program memory.</li>
<li><code>HV &lt;R&gt; &lt;Z2&gt; &lt;Z1&gt; &lt;W&gt; &lt;ZX&gt; &lt;X2&gt; &lt;X1&gt;</code><br/>
Set wire names to be displayed on the LCD, and
to be reported using the packet radio. At most two characters
are allowed per wire name. R, Z2, Z1, W, ZX, X2 and X1 
are the names of the physical connectors labeled on the PCB.
</li>
<li><code>COMPRESSOR=0x&lt;SignalMask&gt; &lt;seconds&gt;</code><br/>
Sets the thermostat's compressor timer lockout bits and timer length.
<code>SignalMask</code> is hexadecimal digits (0 through 9, and A through F). The bit
order is the same as the <code>HV</code> command above, which is
in order of least significant bit to most significant. &lt;seconds&gt; is
the length of time the compressor bits are forced to remain
OFF after any is changed from ON to OFF.</li>
<li><code>DU=F</code> or <code>DU=C</code><br/>
The first sets the LCD temperature units as Farenheit. Otherwise
its Celsius.</li>
<li><code>RH</code><br/>
Initiates an update to the LCD, the radio, and
the USB Serial port of the current control wire
inputs and outputs to/from the packet thermostat.</li>
<li><code>HS C &lt;CelsiusX10&gt;</code><br/>
&lt;CelsiusX10&gt; is the heat safety detect
temperature in Celsius times 10. That is, 
<code>HS C 300</code> sets the detection temperature
to 30.0 degrees C. Set to zero to disable
the heat safety detection.</li>
<li><code>HS T &lt;SECONDS&gt;</code><br/>
&lt;SECONDS&gt; is the heat safety timeout. When
the packet thermostat outlet temperature exceeds
the threshhold, the furnace outputs are held zero
for this long.</li>
<li><code>HS &lt;1-3&gt; &lt;DontCare&gt; &lt;MustMatch&gt; &lt;ToClear&gt;</code><br/>
&lt;1-3&gt; is one of the digits 1 through 3. Up to three different heat modes can be
detected. (For example: commanding the heat pump on is a different signal configuration
than commanding auxilliary heat.)
DontCare is a SignalMask (in hex) of the furnace output wires that are not used to detect
the furnace is in a heat mode. MustMatch is the value of the furnace output wire
mask that, once DontCare signals are zeroed, matches the furnace in the heat mode
to be detected.
ToClear is a SignalMask of bits to clear to force the furnace out of the
detected heat mode. </li>
<li><code>SE &lt;ScheduleEntry&gt; &lt;Celsiusx10&gt; &lt;HOUR&gt; &lt;MINUTE&gt; &lt;DAY-OF-WEEK&gt;</code><br/>
&lt;ScheduleEntry&gt; is a decimal number in the range of 0 through 15 specifying one of the 16
schedule entries. &lt;Celsiusx10&gt;, &lt;HOUR&gt;, &lt;MINUTE&gt; are decimal numbers.
The temperature is in degrees C times 10 (e.g. 200 means 20.0C). HOUR is the range 0 through 23,
and MINUTE is in the range 0 through 59. DAY-OF-WEEK is a seven bit hexadecimal number in the range of 0 through
7F where each bit specifies a day of the week. Bit 0 is SUNDAY, bit 1 is MONDAY, and so on.
If any or all of the values after the ScheduleEntry number are omitted, the corresponding schedule
entry is cleared in the Packet Thermostat's EEPROM. The Packet Thermostat enforces the schedule
for its HEAT and COOL modes only, and the same entries are used regardless of mode.
 </li>
<li><code>HVAC TYPE=&lt;n&gt; COUNT=&lt;m&gt;</code><br/>
&lt;n&gt; is a digit in the range of 0 through 4. The values of n correspond to the modes:
<ol type='1' start='0' >
<li>PassThrough<br/> This mode has only one COUNT, which cannot be changed</li>
<li>MapInputToOutput</li>
<li>HEAT</li>
<li>COOL</li>
<li>AUTO</li>
</ol>
 This command updates the number of MODES in the given TYPE, and it <b>destroys</b> the values in
 the Packet Thermostat EEPROM for all TYPES of higher numbers than &lt;n&gt;.
</li>
 <li><code>HVAC TYPE=&lt;n&gt; MODE=&lt;m&gt;</code><br/>
 &lt;n&gt; is 0 through 4 as the TYPEs above, and &lt;m&gt; must be less than the number
 specified in COUNT above. This command sets the Packet Thermostat's type and mode of operation. Subsequent
 commands from below (starting with HVAC) apply to this particular TYPE and MODE</li>
 <li><code>HVAC COMMIT</code><br/>
 The HVAC_SETTINGS (below) are not written to EEPROM until this COMMIT command. This means, for example, that
 if "HVAC_SETTINGS 200" has been used to set the current target temperature to 20C (which is 68F) and for
 any reason the Packet Thermostat looses power, the HVAC_SETTINGS are restored to what they were at 
 the previous HVAC COMMIT (not necessarily the previous HVAC_SETTINGS)</li>
<li><code>HVAC FAN=ON</code> or <code>HVAC FAN=OFF</code><br/>
Sets or clears the ventilation fan to continuous ON mode.</li>
<li><code>HVAC_SETTINGS &lt;target temperature Cx10&gt; &lt;activate temperature Cx10&gt; &lt;sensor id mask&gt; &lt;Stage 1 Mask&gt; &lt;Stage 2 Mask&gt; &lt;Stage 3 Mask&gt; &lt;Fan Mask&gt; &lt;Seconds to Stage 2&gt; &lt;seconds to Stage 3&gt;</code><pre>
For EXAMPLE, to set the thermostat in COOL mode with typical mapping of PCB to thermostat wires:
                 206     decimal to  69F (20.6C)  
                 211     decimal activate at 70F (21.1), 
                 300     hex use sensors 8 and 9
                  10     hex The Fan is the G wire, mapped to Z1
                  04     hex Keep the O output, mapped to X1, always ON
                  08     hex Stages 1, 2 and 3 are all Y output, mapped to X2
                   1     decimal Seconds to stages 2 and 3 are unimportant, 1 second each
     HVAC_SETTINGS 206 211 300 10 04 08 08 08 1 1
</pre> Any or all of the values after the &lt;target temperature Cx10&gt; may be omitted. If the 
&lt;activate temperature Cx10&gt; is omitted, it is calculated as 0.6C above/below the target,
as is appropriate for the given mode. All settings in the command after the activate temperature are
retained unchanged from their setting at the time of this command. <br/>
This same command is used for HEAT mode as well, but in HEAT you must set the activate temperature
lower than the target temperature (or omit it and it will be set 0.6C below the target.)<br/>
The Seconds-to-stage settings are timed from when stage 1 was started (not from
when any previous stage was started.)</li>
<li><code>HUM_SETTINGS</code><br/>
This command only applies to TYPE=2 (COOL) and TYPE=3 (AUTO)</li>
<li><code>AUTO_SETTINGS</code><br/>
This command only applies to TYPE=3 (AUTO)</li>
<li><code>HVACMAP=0x</code><br/>
This command only applies to TYPE=1, MapInputToOutput</li>
</ul> 
