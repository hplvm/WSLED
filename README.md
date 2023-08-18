# WSLED
Control Neopixels from WebUI, voice assistant and also from terminal, yeah you heard it right, TUI

##Get Up and Running

```
git clone https://github.com/hplvm/WSLED
```
Open `WSLED.ino` in Arduino-IDE, grab this [link](https://www.arduino.cc/en/software) to download if already haven't. 

Head on to [Sinric Pro](https://sinric.pro/), 
- SIGN UP / LOG IN
- Switch to Rooms tab
- Create a new room with name as per preference
- Switch to Devices tab
- Click add device
- Fill in device name as per preference, set `Device Type` as `Smart Light Bulb` and  `Room` as the one you created just know, everything else to default, click next and customise other setting as per preference and hot save.
- Now back back to devices tab, locate your device that you have just now created, copy ID and fill it in LIGHT_ID in the sketch/code.
- Switch to credentials tab, and create new APP KEY and APP SECRET, copy them and fill it in the sketch/code

Update the top block with wifi credentials and other settings.

Connect ESP32/ESP8266, select appropriate board and port, hit upload.

For voice assistant integration, go to Google Home/ Alexa and add click services, search and select sinric pro, login to your sincricpro account and add your light to devices.

Waalaah Finally Set-up finally done.


