# EPPG Controller
Arduino logic for OpenPPG Throttle Controller


## MacOS
### Install the driver

The Arduino nano inside the controller uses the CH340G chip to connect over USB. Install the driver by

1. Download the latest driver package - [CH341SER_MAC.ZIP (148KB)](https://www.google.com/url?q=https://kig.re/downloads/CH341SER_MAC.ZIP&sa=D&ust=1529887332687000)
2. Extract the contents of the zip file to a local installation directory
3. Double-click CH34x_Install.pkg
4. Follow the installation steps
5. Restart your Mac after finishing installation

### Install Arduino IDE

1. Download the latest version for Mac OS X [https://www.arduino.cc/en/Main/Software](https://www.arduino.cc/en/Main/Software)
2. Expand the zip and Copy the Arduino application into the Applications folder

### Download and Prepare OpenPPG Code

1. Download the latest controller code zip from [here](https://github.com/openppg/eppg-controller/archive/master.zip)
2. Extract and open "eppg-controller.ino" in the Arduino IDE
3. Open Library Manager (Sketch -> Include Library -> Manage Libraries)
4. Install the following libraries by searching and installing the latest versions:
- `ResponsiveAnalogRead`
- `Adafruit_SSD1306`
- `Adafruit GFX Library`
- `AceButton`

#### Configure for the OLED Screen

1. Open up the file named "Adafruit_SSD1306.h" by going to `/Users/USERNAME/Documents/Arduino/libraries/Adafruit_SSD1306/` in your favorite text/code editor
2. Uncomment line 73 where it says `#define SSD1306_128_64`
3. Comment out the line below so it says `//   #define SSD1306_128_32`
4. Save and close the file

### Flash the OpenPPG Code

1. First make sure the code compiles by hitting the check button in the top right name "Verify"
2. Connect the controller to your computer by using the USB mini port on the bottom of the controller
3. Select the proper port (Tools -> Port). It should show up as something like "/dev/tty.wchusbserial*"
4. Select the proper board (Tools -> Board -> Arduino Nano). You may also have to select the "old bootloader" under processor
5. Click the right arrow in the top right named "Upload"
6. Wait for the code to flash and the Arduino IDE to say "Done" at the bottom. Success!

## Windows Instructions Coming Soon
