# Squeezelite-esp32
## Supported Hardware 
### SqueezeAMP
Works with the SqueezeAMP see [here](https://forums.slimdevices.com/showthread.php?110926-pre-ANNOUNCE-SqueezeAMP-and-SqueezeliteESP32) and [here](https://github.com/philippe44/SqueezeAMP/blob/master/README.md). Add repository https://raw.githubusercontent.com/sle118/squeezelite-esp32/master/plugin/repo.xml to LMS if you want to have a display

Use the `squeezelite-esp32-SqueezeAmp-sdkconfig.defaults` configuration file.

### ESP32-A1S
Works with [ESP32-A1S](https://docs.ai-thinker.com/esp32-a1s) module that includes audio codec and headset output. You still need to use a demo board or an external amplifier if you want direct speaker connection. 

### ESP32-WROVER + I2S DAC
Squeezelite-esp32 requires esp32 chipset and 4MB PSRAM. ESP32-WROVER meets these requirements. To get an audio output an I2S DAC can be used. Cheap PCM5102 I2S DACs work others may also work. PCM5012 DACs can be hooked up via:

I2S - WROVER  
VCC - 3.3V  
3.3V - 3.3V  
GND - GND  
FLT - GND  
DMP - GND  
SCL - GND  
BCK - (see below)  
DIN - (see below)  
LCK - (see below)
FMT - GND  
XMT - 3.3V 

Use the `squeezelite-esp32-I2S-4MFlash-sdkconfig.defaults` configuration file.

## Configuration
To access NVS, in the webUI, go to credits and select "shows nvs editor". Go into the NVS editor tab to change NFS parameters. In syntax description below \<\> means a value while \[\] describe optional parameters. 

### I2C
The NVS parameter "i2c_config" set the i2c's gpio used for generic purpose (e.g. display). Leave it blank to disable I2C usage. Note that on SqueezeAMP, port must be 1. Syntax is
```
sda=<gpio>,scl=<gpio>,port=0|1
```
### SPI
The NVS parameter "spi_config" set the spi's gpio used for generic purpose (e.g. display). Leave it blank to disable SPI usage. The D/C parameter is needed for displays. Syntax is
```
data=<gpio>,clk=<gpio>[,d/c=<gpio>][,host=0|1|2]
```
### DAC/I2S
The NVS parameter "dac_config" set the gpio used for i2s communication with your DAC. You can also define these at compile time but nvs parameter takes precedence. Note that on SqueezeAMP and A1S, these are forced at runtime, so this parameter does not matter. If your DAC also requires i2c, then you must go the re-compile route. Syntax is
```
bck=<gpio>,ws=<gpio>,do=<gpio>
```
### SPDIF
The NVS parameter "spdif_config" sets the i2s's gpio needed for SPDIF. 

SPDIF is made available by re-using i2s interface in a non-standard way, so although only one pin (DO) is needed, the controller must be fully initialized, so the bit clock (bck) and word clock (ws) must be set as well. As i2s and SPDIF are mutually exclusive, you can reuse the same IO if your hardware allows so.

Note that on SqueezeAMP, these are automatically defined, so this parameter does not matter.

Leave it blank to disable SPDIF usage, you can also define them at compile time using "make menuconfig". Syntax is 
```
bck=<gpio>,ws=<gpio>,do=<gpio>
```
### Display
The NVS parameter "display_config" sets the parameters for an optional display. Syntax is
```
I2C,width=<pixels>,height=<pixels>[address=<i2c_address>][,HFlip][,VFlip]
SPI,width=<pixels>,height=<pixels>,cs=<gpio>[,HFlip][,VFlip]
```
- VFlip and HFlip are optional can be used to change display orientation

The NVS parameter "metadata_config" sets how metadata is displayed for AirPlay and Bluetooth. Syntax is
```
[format=<display_content>][,speed=<speed>]
```
- 'speed' is the scrolling speed in ms (default is 33ms)

- 'format' can contain free text and any of the 3 keywords %artist%, %album%, %title%. Using that format string, the keywords are replaced by their value to build the string to be displayed. Note that the plain text following a keyword that happens to be empty during playback of a track will be removed. For example, if you have set format=%artist% - %title% and there is no artist in the metadata then only <title> will be displayed not " - <title>".

Currently only 128x32 I2C display like [this](https://www.buydisplay.com/i2c-blue-0-91-inch-oled-display-module-128x32-arduino-raspberry-pi) are supported

### Set GPIO
The parameter "set_GPIO" is use to set assign GPIO to various functions.

GPIO can be set to GND provide or Vcc at boot. This is convenient to power devices that consume less than 40mA from the side connector. Be careful because there is no conflict checks being made wrt which GPIO you're changing, so you might damage your board or create a conflict here. 

This parameter can use used as well to assign a GPIO that will be set to 1 when playback starts and wil be reset to 0 when squeezelite becomes idle. The idle timeout is set on the squeezelite command line through -C \<timeout\>

Finally, if you have an audio jack that supports insertion (set to GND when inserted), you can specify whch GPIO it's connected to. Using the parameter jack_mutes_amp allows to mute the amp when headset (e.g.) is inserted.

Syntax is:

```
<gpio_1>=Vcc|GND|amp|jack_h|jack_l[,<gpio_n>=Vcc|GND|amp|jack_h|jack_l]
```
NB: jack_h is for jack inserted sets GPIO to 1 and jack_l is for 0
### Rotary Encoder
One rotary encoder is supported, quadrature shift with press. Such encoders usually have 2 pins for encoders (A and B), and common C that must be set to ground and an optional SW pin for press. A, B and SW must be pulled up, so automatic pull-up is provided by ESP32, but you can add your own resistors. A bit of filtering on A and B (~470nF) helps for debouncing which is not made by software. 

Encoder is normally hard-coded to respectively knob left, right and push on LMS and to volume down/up/play toggle on BT and AirPlay. Using the option 'volume' makes it hard-coded to volume down/up/play toggle all the time (even in LMS). The option 'longpress' allows an alternate mode when SW is long-pressed. In that mode, left is previous, right is next and press is toggle. Every long press on SW alternates between modes.

Use parameter rotary_config with the following syntax:

```
A=<gpio>,B=<gpio>[,SW=gpio>[,volume][,longpress]]
```

HW note: all gpio used for rotary have internal pull-up so normally there is no need to provide Vcc to the encoder. Nevertheless if the encoder board you're using also has its own pull-up that are stronger than ESP32's ones (which is likely the case), then there will be crosstalk between gpio, so you must bring Vcc. Look at your board schematic and you'll understand that these board pull-up create a "winning" pull-down when any other pin is grounded. 

### Buttons
Buttons are described using a JSON string with the following syntax
```
[
{"gpio":<num>,		
 "type":"BUTTON_LOW | BUTTON_HIGH",	
 "pull":[true|false],
 "long_press":<ms>, 
 "debounce":<ms>,
 "shifter_gpio":<-1|num>,
 "normal": {"pressed":"<action>","released":"<action>"},
 "longpress": { <same> },
 "shifted": { <same> },
 "longshifted": { <same> },
 },
 { ... },
 { ... },
] 
```

Where (all parameters are optionals except gpio) 
 - "type": (BUTTON_LOW) logic level when the button is pressed 
 - "pull": (false) activate internal pull up/down
 - "long_press": (0) duration (in ms) of keypress to detect long press, 0 to disable it
 - "debounce": (0) debouncing duration in ms (0 = internal default of 50 ms)
 - "shifter_gpio": (-1) gpio number of another button that can be pressed together to create a "shift". Set to -1 to disable shifter
 - "normal": ({"pressed":"ACTRLS_NONE","released":"ACTRLS_NONE"}) action to take when a button is pressed/released (see below)
 - "longpress": action to take when a button is long-pressed/released (see above/below)
 - "shifted": action to take when a button is pressed/released and shifted (see above/below)
 - "longshifted": action to take when a button is long-pressed/released and shifted (see above/below)

Where \<action\> is either the name of another configuration to load or one amongst 
				ACTRLS_NONE, ACTRLS_VOLUP, ACTRLS_VOLDOWN, ACTRLS_TOGGLE, ACTRLS_PLAY, 
				ACTRLS_PAUSE, ACTRLS_STOP, ACTRLS_REW, ACTRLS_FWD, ACTRLS_PREV, ACTRLS_NEXT, 
				BCTRLS_PUSH, BCTRLS_UP, BCTRLS_DOWN, BCTRLS_LEFT, BCTRLS_RIGHT
				
One you've created such a string, use it to fill a new NVS parameter with any name below 16(?) characters. You can have as many of these configs as you can. Then set the config parameter "actrls_config" with the name of your default config

For example a config named "buttons" :
```
[{"gpio":4,"type":"BUTTON_LOW","pull":true,"long_press":1000,"normal":{"pressed":"ACTRLS_VOLDOWN"},"longpress":{"pressed":"buttons_remap"}},
 {"gpio":5,"type":"BUTTON_LOW","pull":true,"shifter_gpio":4,"normal":{"pressed":"ACTRLS_VOLUP"}, "shifted":{"pressed":"ACTRLS_TOGGLE"}}]
``` 
Defines two buttons
- first on GPIO 4, active low. When pressed, it triggers a volume down command. When pressed more than 1000ms, it changes the button configuration for the one named "buttons_remap"
- second on GPIO 5, active low. When pressed it triggers a volume up command. If first button is pressed together with this button, then a play/pause toggle command is generated.

While the config named "buttons_remap"
```
[{"gpio":4,"type":"BUTTON_LOW","pull":true,"long_press":1000,"normal":{"pressed":"BCTRLS_DOWN"},"longpress":{"pressed":"buttons"}},
 {"gpio":5,"type":"BUTTON_LOW","pull":true,"shifter_gpio":4,"normal":{"pressed":"BCTRLS_UP"}}]
``` 
Defines two buttons
- first on GPIO 4, active low. When pressed, it triggers a navigation down command. When pressed more than 1000ms, it changes the button configuration for the one descrobed above
- second on GPIO 5, active low. When pressed it triggers a navigation up command. That button, in that configuration, has no shift option

Below is a difficult but functional 2-buttons interface for your decoding pleasure

*buttons*
```
[{"gpio":4,"type":"BUTTON_LOW","pull":true,"long_press":1000,
 "normal":{"pressed":"ACTRLS_VOLDOWN"},
 "longpress":{"pressed":"buttons_remap"}},
 {"gpio":5,"type":"BUTTON_LOW","pull":true,"long_press":1000,"shifter_gpio":4,
 "normal":{"pressed":"ACTRLS_VOLUP"}, 
 "shifted":{"pressed":"ACTRLS_TOGGLE"}, 
 "longpress":{"pressed":"ACTRLS_NEXT"}}
]
```
*buttons_remap*
```
[{"gpio":4,"type":"BUTTON_LOW","pull":true,"long_press":1000,
 "normal":{"pressed":"BCTRLS_DOWN"},
 "longpress":{"pressed":"buttons"}},
 {"gpio":5,"type":"BUTTON_LOW","pull":true,"long_press":1000,"shifter_gpio":4,
 "normal":{"pressed":"BCTRLS_UP"},
 "shifted":{"pressed":"BCTRLS_PUSH"},
 "longpress":{"pressed":"ACTRLS_PLAY"},
 "longshifted":{"pressed":"BCTRLS_LEFT"}}
]
```
### Battery / ADC
The NVS parameter "bat_config" sets the ADC1 channel used to measure battery/DC voltage. Scale is a float ratio applied to every sample of the 12 bits ADC. A measure is taken every 10s and an average is made every 5 minutes (not a sliding window). Syntax is
```
channel=0..7,scale=<scale>
```
NB: Set parameter to empty to disable battery reading
## Setting up ESP-IDF
### Docker
You can use docker to build squeezelite-esp32  
First you need to build the Docker container:
```
docker build -t esp-idf .
```
Then you need to run the container:
```
docker run -i -t -v `pwd`:/workspace/squeezelite-esp32 esp-idf
```
The above command will mount this repo into the docker container and start a bash terminal
for you to then follow the below build steps

### Manual Install of ESP-IDF
<strong>Currently this project requires a specific combination of IDF 4 with gcc 5.2. You'll have to implement the gcc 5.2 toolchain from an IDF 3.2 install into the IDF 4 directory in order to successfully compile it</strong>

You can install IDF manually on Linux or Windows (using the Subsystem for Linux) following the instructions at: https://www.instructables.com/id/ESP32-Development-on-Windows-Subsystem-for-Linux/
And then copying the i2s.c patch file from this repo over to the esp-idf folder

## Building Squeezelite-esp32
MOST IMPORTANT: create the right default config file
- for all libraries, add -mlongcalls. 
- make defconfig
(Note: You can also copy over config files from the build-scripts folder to ./sdkconfig)
Then adapt the config file to your wifi/BT/I2C device (can also be done on the command line)
- make menuconfig
Then

```
# Build recovery.bin, bootloader.bin, ota_data_initial.bin, partitions.bin  
# force appropriate rebuild by touching all the files which may have a RECOVERY_APPLICATION specific source compile logic
	find . \( -name "*.cpp" -o -name "*.c" -o -name "*.h" \) -type f -print0 | xargs -0 grep -l "RECOVERY_APPLICATION" | xargs touch
	export PROJECT_NAME="recovery" 
	make -j4 all EXTRA_CPPFLAGS='-DRECOVERY_APPLICATION=1'
make flash
#
# Build squeezelite.bin
# Now force a rebuild by touching all the files which may have a RECOVERY_APPLICATION specific source compile logic
find . \( -name "*.cpp" -o -name "*.c" -o -name "*.h" \) -type f -print0 | xargs -0 grep -l "RECOVERY_APPLICATION" | xargs touch
export PROJECT_NAME="squeezelite" 
make -j4 app EXTRA_CPPFLAGS='-DRECOVERY_APPLICATION=0'
python ${IDF_PATH}/components/esptool_py/esptool/esptool.py --chip esp32 --port ${ESPPORT} --baud ${ESPBAUD} --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x150000 ./build/squeezelite.bin  
# monitor serial output
make monitor

```
 
# Configuration
1/ setup WiFi
- Boot the esp, look for a new wifi access point showing up and connect to it.  Default build ssid and passwords are "squeezelite"/"squeezelite". 
- Once connected, navigate to 192.168.4.1 
- Wait for the list of access points visible from the device to populate in the web page.
- Choose an access point and enter any credential as needed
- Once connection is established, note down the address the device received; this is the address you will use to configure it going forward 

2/ setup squeezelite command line (optional)

At this point, the device should have disabled its built-in access point and should be connected to a known WiFi network.
- navigate to the address that was noted in step #1
- Using the list of predefined options, hoose the mode in which you want squeezelite to start
- Generate the command
- Add or change any additional command line option (for example player name, etc)
- Activate squeezelite execution: this tells the device to automatiaclly run the command at start
- Update the configuration
- click on the "start toggle" button. This will force a reboot. 
- The toggle switch should be set to 'ON' to ensure that squeezelite is active after booting

3/ Updating Squeezelite
- From the firmware tab, click on "Check for Updates"
- Look for updated binaries
- Select a line
- Click on "Flash!"
- The system will reboot into recovery mode (if not already in that mode), wipe the squeezelite partition and download/flash the selected version 

3/ Recovery
- From the firmware tab, click on the "Recovery" button. This will reboot the ESP32 into recovery, where additional configuration options are available from the NVS editor

# Additional command line notes, configured from the http configuration
The squeezelite options are very similar to the regular Linux ones. Differences are :

	- the output is -o ["BT -n '<sinkname>' "] | [I2S]
	- if you've compiled with RESAMPLE option, normal soxr options are available using -R [-u <options>]. Note that anything above LQ or MQ will overload the CPU
	- if you've used RESAMPLE16, <options> are (b|l|m)[:i], with b = basic linear interpolation, l = 13 taps, m = 21 taps, i = interpolate filter coefficients

For example, so use a BT speaker named MySpeaker, accept audio up to 192kHz and resample everything to 44100 and use 16 bits resample with medium quality, the command line is:
	
	squeezelite -o "BT -n 'BT <sinkname>'" -b 500:2000 -R -u m -Z 192000 -r "44100-44100"

See squeezlite command line, but keys options are

	- Z <rate> : tell LMS what is the max sample rate supported before LMS resamples
	- R (see above)
	- r "<minrate>-<maxrate>"

## Additional misc notes to do you build
- as of this writing, ESP-IDF has a bug int he way the PLL values are calculated for i2s, so you *must* use the i2s.c file in the patch directory
- for all libraries, add -mlongcalls.
- audio libraries are complicated to rebuild, open an issue if you really want to
- libmad, libflac (no esp's version), libvorbis (tremor - not esp's version), alac work
- libfaad does not really support real time, but if you want to try
	- -O3 -DFIXED_POINT -DSMALL_STACK
	- change ac_link in configure and case ac_files, remove ''
	- compiler but in cfft.c and cffti1, must disable optimization using
			#pragma GCC push_options
			#pragma GCC optimize ("O0")
			#pragma GCC pop_options
- opus & opusfile
	- for opus, the ESP-provided library seems to work, but opusfile is still needed
	- per mad & few others, edit configure and change $ac_link to add -c (faking link)
	- change ac_files to remove ''
	- add DEPS_CFLAGS and DEPS_LIBS to avoid pkg-config to be required
- better use helixaac			
- set IDF_PATH=/home/esp-idf
- set ESPPORT=COM9
- update flash partition size
- other compiler #define
	- use no resampling or set RESAMPLE (soxr) or set RESAMPLE16 for fast fixed 16 bits resampling
	- use LOOPBACK (mandatory)
	- use BYTES_PER_FRAME=4 (8 is not fully functionnal)
	- LINKALL (mandatory)
	- NO_FAAD unless you want to us faad, which currently overloads the CPU
	- TREMOR_ONLY (mandatory)

