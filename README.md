# Squeezelite-esp32
## Supported Hardware 
### SqueezeAMP
Works with the SqueezeAMP see [here](https://forums.slimdevices.com/showthread.php?110926-pre-ANNOUNCE-SqueezeAMP-and-SqueezeliteESP32) and [here](https://github.com/philippe44/SqueezeAMP/blob/master/README.md). Add repository https://raw.githubusercontent.com/sle118/squeezelite-esp32/master/plugin/repo.xml to LMS if you want to have a display

Use the `squeezelite-esp32-SqueezeAmp-sdkconfig.defaults` configuration file.

### ESP32-WROVER + I2S DAC
Squeezelite-esp32 requires esp32 chipset and 4MB PSRAM. ESP32-WROVER meets these requirements.  
To get an audio output an I2S DAC can be used. Cheap PCM5102 I2S DACs work others may also work. PCM5012 DACs can be hooked up via:

I2S - WROVER  
VCC - 3.3V  
3.3V - 3.3V  
GND - GND  
FLT - GND  
DMP - GND  
SCL - GND  
BCK - 26  
DIN - 22  
LCK - 25  
FMT - GND  
XMT - 3.3V 

Use the `squeezelite-esp32-I2S-4MFlash-sdkconfig.defaults` configuration file.

## Configuration
To access NVS, in the webUI, go to credits and select "shows nvs editor". Go into the NVS editor tab to change NFS parameters 
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

Where <action> is either the name of another configuration to load or one amongst 
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
- second on GPIO 5, acive low. When pressed it triggers a volume up command. If first button is pressed together with this button, then a play/pause toggle command is generated.

While the config named "buttons_remap"
```
[{"gpio":4,"type":"BUTTON_LOW","pull":true,"long_press":1000,"normal":{"pressed":"BCTRLS_DOWN"},"longpress":{"pressed":"buttons"}},
 {"gpio":5,"type":"BUTTON_LOW","pull":true,"shifter_gpio":4,"normal":{"pressed":"BCTRLS_UP"}}]
``` 
Defines two buttons
- first on GPIO 4, active low. When pressed, it triggers a navigation down command. When pressed more than 1000ms, it changes the button configuration for the one descrobed above
- second on GPIO 5, acive low. When pressed it triggers a navigation down command. That button, in that configuration, has no shift option

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

