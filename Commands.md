<h2>Commands</h2>

<p>Commands are text sent to the PacketThermostat
by either its USB serial port, or over its packet
radio interface. The commands are described below. The case of the
letters matters and must be as documented. Items in angle brackets like this: &lt;YYYY&gt;
must be present (unless specified otherwise below) and the YYYY is the name of the thing you must enter as is documented
with the command. For this example, YYYY might be a four digit year.</p> 

<p>Many of these command results are saved in the Packet Thermostat's
EEPROM, and many, but not all, such commands are most easily set up 
using the PacketThermostatSettings application published here in this 
repository. PacketThermostatSettings is coded for a particular mapping
of thermostat control wires to the PacketThermostat's input and output pins. 
 You are invited to modify it if your thermostat wiring assignment
 differs.
</p>

<ul>
<li><code>T=&lt;YYYY&gt; &lt;MM&gt; &lt;DD&gt; &lt;HH&gt; &lt;MM&gt; &lt;SS&gt; &lt;DOW&gt;</code><br/>
Sets the real time clock to the year, month, day, hours,
minutes, seconds and day-of-week. Year is four digits, month is 1 through 12,
day is 1 through 31, hours is 0 through 23, minutes and seconds are
0 through 59. DOW is day-of-week and is from 0 through 6 (for
Sunday through Saturday).</li>
<li><code>I</code><br/>
Prints out on the USB serial port what the radio parameters
are set to. Except this command prints nothing if 
the C++ preprocessor setting has eliminated the printout
to preserve program memory.</li>
<li><code>HV &lt;R&gt; &lt;Z2&gt; &lt;Z1&gt; &lt;W&gt; &lt;ZX&gt; &lt;X2&gt; &lt;X1&gt; &lt;X3&gt;</code><br/>
Set wire names to be displayed on the LCD, and
to be reported using the packet radio. At most two characters
are allowed for each of the 8 PCB signals. R, Z2, Z1, W, ZX, X2, X1 and X3
are the names of the physical connectors labeled on the PCB.  They are ordered 
 as above
with the least significant bit for R(bit 0), to the most significant, X3 (bit 
7). This bit order applies in all the signal masks in the remaining
commands below, and for both the inputs to the Packet Thermostat, 
and for its outputs. (The order X2, X1, and X3 is not a typo. That
 is their bit order.) The R wire is not a
signal input used to compute outputs--its the 24VAC supply which means
bit 0 cannot be used in any masks for either input or output. X3 is an
 output, not an input, and R is not an output. The number of input
 signals that can contribute to the output is 6, <code>&lt;Z2&gt;</code>
 through <code>&lt;X1&gt;</code> above, and the number of outputs
 that Packet Thermostat can control is 7: <code>&lt;Z2&gt;</code>
 through <code>&lt;X3&gt;</code> above.
</li>
<li><code>COMPRESSOR=0x&lt;SignalMask&gt; &lt;seconds&gt;</code><br/>
Sets the thermostat's compressor timer lockout bits and timer length.
<code>SignalMask</code> is hexadecimal digits (0 through 9, and A through F). The bit
order as specified in the <code>HV</code> command above. &lt;seconds&gt; is
the length of time the compressor bits are forced to remain
OFF after any of the masked output signals is changed from ON to OFF.</li>
<li><code>DU=F</code> or <code>DU=C</code><br/>
The first sets the LCD temperature units as Farenheit. Otherwise
its Celsius.</li>
<li><code>RH</code><br/>
Forces an update to the LCD, the radio, and
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
the packet thermostat inlet  temperature sensor (wired to Ti on the PCB) exceeds
the threshhold, the furnace heating outputs are held zero
for this long.</li>
<li><code>HS &lt;1-3&gt; &lt;DontCare&gt; &lt;MustMatch&gt; &lt;ToClear&gt;</code><br/>
&lt;1-3&gt; is a digit 1 through 3. Up to three different heat modes can be
detected. (For example: commanding the heat pump on is a different signal configuration
than commanding auxilliary heat, and detecting a heat over temperature for
 the different signal configurations each requires a different <code>HS</code> entry.)
DontCare is a SignalMask (in hex) of the furnace output wires that are not used to detect
the furnace is in a heat mode. MustMatch is the value of the furnace output wire
mask that, once DontCare signals are zeroed, matches the furnace in the heat mode
to be detected.
ToClear is a SignalMask of bits to clear to force the furnace out of the
detected heat mode. </li>
<li><code>SE &lt;ScheduleEntry&gt; &lt;Celsiusx10&gt; &lt;HOUR&gt; &lt;MINUTE&gt; &lt;DAY-OF-WEEK&gt; &lt;AutoOnly&gt;</code><br/>
&lt;ScheduleEntry&gt; is a decimal number in the range of 0 through 15 specifying one of the 16
schedule entries. &lt;Celsiusx10&gt;, &lt;HOUR&gt;, &lt;MINUTE&gt; are decimal numbers.
The temperature is in degrees C times 10 (e.g. 200 means 20.0C). HOUR is the range 0 through 23,
and MINUTE is in the range 0 through 59. DAY-OF-WEEK is a seven bit hexadecimal number in the range of 0 through
7F where each bit specifies a day of the week. Bit 0 is SUNDAY, bit 1 is MONDAY, and so on.
 Unless <code>&lt;AutoOnly&gt;</code> is present and
is the digit <code>1</code>, the Packet Thermostat enforces the schedule
 if is in HEAT or COOL type, setting the target temperature specified by the <code>&lt;Celsiusx10&gt;</code>. 
 If <code>&lt;AutoOnly&gt;</code> is 
 <code>1</code>, the Packet Thermostat sets the heat target temperature if it is in AUTO type.
 If any or all of the values after the ScheduleEntry number are omitted, the corresponding schedule
entry is cleared in the Packet Thermostat's EEPROM.
 </li>
<li><code>HVAC TYPE=&lt;n&gt; COUNT=&lt;m&gt;</code><br/>
&lt;n&gt; is a digit in the range of 0 through 4. The values of n correspond to the types:
<ol type='1' start='0' >
<li>PassThrough<br/> This type has exactly one COUNT, and cannot be changed</li>
<li>MapInputToOutput</li>
<li>HEAT</li>
<li>COOL</li>
<li>AUTO</li>
</ol>
 This command updates the number of MODES in the given TYPE and does so in EEPROM. It <b>destroys</b> the values in
 the Packet Thermostat EEPROM for all TYPES of higher numbers than &lt;n&gt;. All types except PassThrough
may have COUNT=0, which prevents the thermostat from entering that type, even if command to. PassThrough
always has only one MODE, and the only setting it has is its NAME.
</li>
 <li><code>HVAC TYPE=&lt;n&gt; MODE=&lt;m&gt;</code><br/>
 &lt;n&gt; is 0 through 4 as the TYPEs above, and &lt;m&gt; must be less than the number
 specified in COUNT above. This command sets the Packet Thermostat's type and mode of operation. Subsequent
 commands documented below (starting with HVAC) will apply to this particular TYPE and MODE. This
command sets the Packet Thermostat's current operating state, initializes its
settings from EEPROM, and sets the EEPROM state to restore this TYPE and MODE should the
thermostat power down and back up.</li>
 <li><code>HVAC COMMIT</code><br/>
 The HVAC_SETTINGS and HVAC commands (below) are not written to EEPROM until this COMMIT command. This means, for example, that
 if "HVAC_SETTINGS 200" has been used to set the current target temperature to 20C (which is 68F) and for
 any reason the Packet Thermostat loses power, the HVAC_SETTINGS are restored to what they were at 
 the previous HVAC COMMIT (not necessarily the previous HVAC_SETTINGS)</li>
<li><code>HVAC FAN=ON</code> or <code>HVAC FAN=OFF</code><br/>
 This command only has effect when the Packet Thermostat is in HEAT, COOL or AUTO type.<br/>
Sets or clears the ventilation fan to continuous ON mode.</li>
<li><code>HVAC NAME=&lt;name&gt;</code>
<br/>The name displayed for the current TYPE and MODE in the LCD. Five characters maximum.</li>
<li><code>HVAC_SETTINGS &lt;target temperature Cx10&gt; &lt;activate temperature Cx10&gt; &lt;sensor id mask&gt; &lt;Stage 1 Mask&gt; &lt;Stage 2 Mask&gt; &lt;Stage 3 Mask&gt; &lt;Fan Mask&gt; &lt;Seconds to Stage 2&gt; &lt;seconds to Stage 3&gt;</code><pre>
For EXAMPLE, to set the thermostat COOL type with typical mapping of PCB to thermostat wires:
                 206     decimal. target is 69F (20.6C)  
                 211     decimal. activate at 70F (21.1), 
                 300     hex. use sensors 8 and 9
                  10     hex. The Fan is the G wire, mapped to Z1
                  04     hex. Keep the O output, mapped to X1, always ON
                  08 08 08     hex. Stages 1, 2 and 3 are all Y output, mapped to X2
                   1  1        decimal. Seconds to stages 2 and 3 are unimportant, 1 second each
     HVAC_SETTINGS 206 211 300 10 04 08 08 08 1 1
</pre> Leading zeros as shown above are allowed but are not required.
 Any or all of the values after the &lt;target temperature Cx10&gt; may be omitted. If the 
&lt;activate temperature Cx10&gt; is omitted, it is calculated as 0.6C above/below the target,
as is appropriate for the given type. All settings in the command after the activate temperature are
retained unchanged from their setting at the time of this command. <br/>
This same command is used for HEAT type as well, but in HEAT you must set the activate temperature
lower than the target temperature (or omit it and it will be set 0.6C below the target.)<br/>
The Seconds-to-stage settings are timed from when stage 1 was started (not from
when any previous stage was started.)<br/>
 This command only has effect when the Packet Thermostat is in HEAT, COOL or AUTO type.</li>
<li><code>HUM_SETTINGS &lt;HumdityX10&gt; &lt;MaskON&gt; &lt;MaskOFF&gt;</code><br/>
This command only has effect if TYPE=2 (COOL) or TYPE=3 (AUTO)<br/>
&lt;HumdityX10&gt; is percent humidty times 10 in decimal (e.g. 400 is 40% humidity.)
The two masks are hexadecimal. <code>&lt;MaskON&gt;</code> specifies the output bits to
turn on when the humidity exceeds the setting and <code>&lt;MaskOFF&gt;</code> specifies
the bits to turn off, also on high humidity. When the humidity input
is below the setting, the appropriate <code>Stage n Mask</code> specifies
the output.</li>
<li><code>AUTO_SETTINGS &lt;target heat Cx10&gt; &lt;activate heat Cx10&gt; &lt;Stage 1 Mask&gt; &lt;Stage 2 Mask&gt; &lt;Stage 3 Mask&gt;</code><br/>
This command only has effect if TYPE=3 (AUTO)<br/>
AUTO mode uses the <code>HVAC_SETTINGS</code> for cooling, and for heating, it uses these settings. The seconds-in-stage for
heating in AUTO are the same for heating as for cooling as specified in  <code>HVAC_SETTINGS</code>. 
If only the target heat temperature is specified, the activate temperature is set to 0.6C lower.</li>
<li><code>HVACMAP=0x&lt;addr&gt; &lt;v1&gt; &lt;v2&gt; ... &lt;v8&gt;</code><br/>
This command only has effect if TYPE=1, MapInputToOutput<br/>
The MapInputToOutput has 64 one-byte entries in its map. Each entry corresponds
to one of the 64 possible combintations of the 6 inputs being either on (represented
 by a one) or off (represented by zero.) The <code>&lt;addr&gt;</code> is hex and
 sets the position in the map of the next argument,  <code>&lt;v1&gt;</code>. Up
 to 8 mask entries may be specified with <code>&lt;v2&gt;</code> and so on
each applied to the next higher <code>&lt;addr&gt</code>. To fully specify the map 
requires issuing this command at least 8 times starting with, for example, <code>HVACMAP=0x0 0 1 2 3 4 5 6 7</code>
up to the eighth command which might look like <code>HVACMAP=0x38 38 39 3A 3B 3C 3D 3E 3F </code>. This example
shows the first and last commands of eight that would set up a map where the Packet Thermostat output wires
are set to match its input wires (which is exactly what <code>HVAC TYPE=0 MODE=0</code> does.) All
the values &lt;addr&gt; and &lt;v1&gt; through &lt;v8&gt; are hexadecimal.
This command is the only exception to the rule that input and output masks are
 computed as bit masks with the R signal as bit zero. The output values (<code>&lt;v1&gt;</code> and so on) in this
command <i>are</i> in that coding but the &lt;addr&gt; values are <i>not</i>. This is because the R signal is ignored when 
 the Packet Thermostat calculates
the map entry. Instead, for the purpose of specifying <code>&lt;addr&gt;</code>, Z2 is bit zero. 
Therefore, the 64 valid values for  &lt;addr&gt; start at zero for the
case of the 6 inputs Z2 through X1 off, the value 0x1 is for only Z2 on, up through
0x3F for all inputs Z2 through X2 on.
</li>
</ul> 
