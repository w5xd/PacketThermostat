<h2>Printed Circuit Board</h2>
The gerber files for REV03 PCB fabrication are <a href='PCB/REV03-Gerbers.zip'>here</a>. There
are several updates in REV04 of <a href='PCB/thermostat remote.rrb'>PCB/thermostat remote.rrb</a> which 
can only be fabricated by <a href='http://expresspcb.com'>expresspcb.com</a>.
While there are some through-hole patterns on the PCB,
many of the parts are available only SMD. A <a href='https://www.whizoo.com/controleo3'>Reflow Oven</a> makes PCB assembly
faster and probably easier.
Or if your hand is steady enough for 0.050" IC pin spacing, you can hand solder.
<p style='margin-top:8px'>I had bad results reflow soldering with the RFM69 in place. Solder paste spreads under the module and shorts its vias. Its easier to hand solder it.</p>
All parts mount on the PCB top.
<p style='margin-top:8px'>The enclosure is 3D printable on common 3D printers. The printable STL files 
are <a href="STL">here</a>. If instead you want to design a different enclosure, then you are welcome to start with the
SolidWorks models <a href="CAD">here</a>.</p>
<p>I used PETG filament. It prints in three parts: the top, the bottom and a 1/4" diameter bracing cylinder.
Where does that little cylinder go? Use a #4 machine screw x 1/8" inserted 
through the bottom of the PCB in the hole in the middle of the board. The cylinder 
is permanently left in place (only <i>after</i> you have reflow soldered, of course.)
Orient it so its rounded edge faces the PCB board edge in the direction of the qwiic connector. Its purpose is 
to keep both the LCD and the PCB from
falling into the center of the enclosure by pressing them against the top and bottom, respectively.
</p>

The enclosure <b>top</b> has nut slots for 4 nuts, one each on the sides of the legs. Four #4 machine screws, 5/8" long, 
enter through the enclosure bottom and hold the entire assembly together.
(On the enclosure <b>bottom</b> 
there are two more slots if you happen to find them, but you may safely ignore 
these nut slots on the enclosure bottom. Those 4 screws through the entire assembly hold the PCB securely in place.)

<p><a href='https://www.digikey.com/short/jwdbnhr7'>Here</a> is a Digikey parts list with all the parts except the PCB, enclosure, and hookup wire.

<ol>
<li> <a href='https://www.sparkfun.com/products/12587'>SparkFun Pro Micro 3.3V Arduino</a>.<br/>
<ol><li>Do <b><i>not</i></b> attempt to use the 5V version of the Arduino! The RFM69 radio is a 3.3V part only!</li>
<li>When programming the Pro Micro using the Arduino IDE, pay
attention to the fact you <b>must</b> specify the correct processor voltage in the Arduino IDE Tools/Processor menu. Programming at the wrong voltage bricks the
Pro Micro such that it is difficult (although not impossible) to recover it. See Sparkfun's 
<a href='https://learn.sparkfun.com/tutorials/pro-micro--fio-v3-hookup-guide/troubleshooting-and-faq#ts-revive'>page</a>.
</li></ol></li>
<li> <a href='https://www.sparkfun.com/products/16281'>SparkFun Real Time clock module, RV-8803</a> <br/>
The current version of the firmware will run without the RTC, but if you ever want to program scheduling
into your sketch, you're going to need it.</li>
<li> <a href='https://www.sparkfun.com/products/13909'>Sparkfun RFM69HCW packet radio module.</a></li>
<li> <a href='https://www.sparkfun.com/products/14417'>Sparkfun Qwiic SMD connector</a></li>
<li> <a href='https://www.sparkfun.com/products/16396'>Sparkfun 16x2 SerLCD -RGB - Qwiic</a></li>
<li> <a href='https://www.digikey.com/en/products/detail/omron-electronics-inc-emc-div/G3VM-61GR2/5810883'>G3VM-61GR2 Omron
Solid State Relay<a>. (Quantity: 7)</li>
<li> <a href='https://www.digikey.com/en/products/detail/cit-relay-and-switch/J1031C5VDC-15S/14002065'>SPDT relay</a></li>
<li> <a href='https://www.digikey.com/en/products/detail/vishay-semiconductor-opto-division/TCMT4600T0/4074845'>Quad 
Optoisolator AC input, 4mm 16-SOP</a>. (Quantity: 2)</li>
<li> <a href='https://www.digikey.com/en/products/detail/stmicroelectronics/LM334DT/1038704'>Current Source, 8-SOIC</a>, 4mm width. (Quantity: 3)
<br/>Pin 1 is on the beveled side of the LM334</li>
<li> <a href='https://www.digikey.com/en/products/detail/diodes-incorporated/AZ1117IH-3-3TRG1/5699672'>3.3V regulator, SOT223</a></li>
<li> <a href='https://www.digikey.com/en/products/detail/onsemi/MMBT100/3504512'>2n3904 (or equivalent) in SOT-23</a></li>
<li> <a href='https://www.digikey.com/en/products/detail/texas-instruments/SN74HC594DR/1571252'>74hc594 shift register in 4mm width 16-SOIC </a></li>
<li> All resistors and capacitors on the PCB have dual SMD pads size 1206. Except the SMD for the 10uF polarized is 2312</li>
<li> Use a 5VDC wall wart. Do not use anything higher than 6V!
</ol>
All of the Sparkfun products listed above are also in DigiKey's catalog. 
Search Digikey.com for the Sparkfun product number (13909, 14417, or 16396).

Mechanical parts:
<ol>
<li> <a href='https://www.digikey.com/en/products/detail/molex/0003091091/26302'>9 pin Molex RECPT panel mount</a>.</li>
<li> <a href='https://www.digikey.com/en/products/detail/molex/0003091094/61333'>9 pin Molex RECPT free hanging</a>. (Quantity: 2)</li>
<li> <a href='https://www.digikey.com/en/products/detail/molex/0003092092/61309'>9 pin Molex PLUG free hanging</a>. (Quantity: 2)</li>
<li> <a href='https://www.digikey.com/en/products/detail/molex/0469990653/5723549'>4 pin Molex RECPT panel mount</a></li>
<li> <a href='https://www.digikey.com/en/products/detail/molex/0003092049/61303'>4 pin Molex PLUG</a></li>
<li> <a href='https://www.digikey.com/en/products/detail/molex/0002092118/26388'>Molex PIN</a>. (Quantity: 22)</li>
<li> <a href='https://www.digikey.com/en/products/detail/molex/0002091119/26390'>Molex SOCKET</a>. (Quantity: 22)</li>
<li> <a href='https://www.digikey.com/en/products/detail/cui-devices/PJ-202A/252007'> PJ-202A 2MMx5.5MM kinked pin power jack</a></li>
<li> <a href='https://www.digikey.com/en/products/detail/TSW-150-07-T-S/SAM1035-50-ND/1101574?itemSeq=320138980'>0.100 inch Connection headers</a>. (Quantity: 3)<br/>
Use this specified header soldered both sides and without a socket to be sure the micro USB on the Pro Micro lines up with its hole in the enclosure</li>
<li> <a href='https://www.sparkfun.com/products/17260'>50mm Qwiic cable</a>. (Quantity: 2)</li>
</ol>
The Molex quantities listed include (exactly enough) parts to populate the enclosure <i>and</i> one free hanging mating connector.

<p style='margin-top:9px'>You have to decide at assembly time which PCB otuput/input should be wired to which 
Molex pin number. Here are some recommendations:</p>
<ul>
<li> Use the same pinout on the two 9 pin molex housings, and put pins in one and sockets in the other. The result is that 
you can
unplug the packet thermostat and plug the two hanging connectors to each other and the furnace will operate without it.
(Maybe your HVAC service person would be more comfortable working on a unit without a mysterious box on its 
control cable?)</li>
<li>AWG 18 wire is recommended for the furnace-facing 9 pin molex. </li>
<li>A smaller gauge, e.g. AWG 24, is fine for the thermostat-facing molex, and for the wires to the thermometers.</li>
<li>Along the furnace facing side of the board, the number of holes in the PCB (14) is one more than the 
number of pins in the specified
Molex connectors (13). If you're still looking for another wire into the enclosure, a last resort might be the
opposite side of the PCB. It has the reverse
situation: one more molex pin (9) than holes in the PCB (8).
</ul>
Part orientations
<ul>
<li>Only one orientation of the RTC module fits the enclosure:
<ul>
<li>Its qwiic connectors face away from the enclosure top
<li>The lithium cell faces toward the enclosure top, and is towards the LCD display.
</ul></li>
<li> The RTC module has two qwiic connectors. Electrically, it does not matter which goes to the 
LCD and PCB, but you'll discover the 50mm length cables only fit one way.
<li>The LCD display recommended orientation is that its lettering is upright when 
the LCD window in the enclosure is LOWER. It can be oriented the other way,
but there are two disadvantages to the other orientation.
<ul>
<li>The specified 50mm qwiic cable is too short
<li> The micro-USB and 5VD power holes are on the box top where gravity pulls dust into the box.
</ul></li>
</ul>

Don't forget to jumper the output ZX and W header pins to either Rx or Rz or both, else those 
outputs won't work. To know which
R in the thermostat to connect to which R in the furnace, you need to know a little about how your 
furnace is wired. The PCB also
has a jumper position to tie Rx to Rz.

<p align='center'><img src='jumpers.png' alt='jumpers.png'/></p>

Firmware and initial test
<ol>
<li>For initial tests, do not plug in the 5VDC. Connect your PC to the micro-USB only, which will
power the thermostat. While there is a blocking diode between the micro-USB
power and the 5VDC connector, do not trust it to protect your PC if you plug both in at once.
<li>Load the sketch on the Arduino using the Arduino IDE (or the software tool of your choice).
<li>The packet radio won't function until you program its Network ID, Node ID, and Frequency Band into its 
EEPROM. And if you're running your gateway
encrypted, you have to specify the key. (By the way, the symptom if the encryption keys on this thermostat and
the gateway do not match, is the packets are delivered anyway, but they are gibberish.)
<li>On an out-of-the-box Pro Micro, the EEPROM comes will all bits set, which corresponds to no types 
nor modes other than 
Pass through per the below instructions. The commands here initialize the thermostat
to pass-through mode. Use the Arduion IDE "Serial Monitor" function with 9600 baud set. The 
Arduino sketch, if properly installed, will response "ready>" after each command. If not...check the hardware 
and/or upload the sketch again.
<ul>
<li> HV R d G W Y2 Y O
<li> HVAC TYPE=1 COUNT=0
<li> HVAC TYPE=2 COUNT=0
<li> HVAC TYPE=3 COUNT=0
<li> HVAC TYPE=4 COUNT=0
<li> HVAC TYPE=0 MODE=0
<li> HVAC NAME=PasT
<li> HVAC COMMIT
</ul> On the penultimate command, the LCD should display the name <code>PasT</code></li>
<li> I strongly recommend you first test ONLY the fan function (Green control wire). Most thermostats have 
a Fan On button. Try it. If anything is wrong, you have only one 
signal to debug.
<li> Test with the device wired between the conventional thermostat and the furnace. If the packet radio EEPROM setup is done,
you'll get messages at your gateway when the control wires change, but regardless of the radio, the thermostat should
pass through all incoming 24VAC signals from the thermostat to the furnace while displaying its output wire 
states on its LCD.
</ol>
