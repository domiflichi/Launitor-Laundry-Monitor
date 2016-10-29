Launitor (Laundry Monitor) is my little project based on an ESP8266 and a couple
of accelerometers. It senses when your dryer/washing machine starts running, 
and when it is done. When it's done, it will send you an email notifying you that 
it is done. You can also configure it to keep sending you (email) reminders every 
30 minutes (or any other amount of minutes that you want), until you
remove the items from your washer/dryer and hit the reset button.


Back to the hardware - I don't use a straight ESP8266, but actually use Adafruit's
Huzzah ESP8266 breakout board:
https://www.adafruit.com/products/2471
This makes everything much easier as you don't have to worry about voltages, access to
the I/O pins you need, etc. So instead of paying $3-$4 you pay $10, but believe me,
it's worth it if you're not an expert in electronics (Which I am definitely not!).

If you're not sure about using the Huzzah ESP8266 Breakout board, you might
want to check out Adafruit's tutorial:
https://learn.adafruit.com/adafruit-huzzah-esp8266-breakout/overview
If you're comfortable with following along the tutorial and actually doing the things that's talked
about, you shouldn't have much trouble (if any) building a Launitor for yourself!


The sketch
As you'll see when you look at / open the Arduino sketch, you will need to make sure you 
have the following libraries installed:

ESP8266wifi - https://github.com/ekstrand/ESP8266wifi
Wire - https://www.arduino.cc/en/Reference/Wire
SPI -  https://www.arduino.cc/en/Reference/SPI
LIS3DH - https://github.com/adafruit/Adafruit_LIS3DH/archive/master.zip
Sensor - https://github.com/adafruit/Adafruit_Sensor/archive/master.zip

For more information on libraries, such as where to find them, how to 'install' them, etc., see:
https://learn.adafruit.com/arduino-tips-tricks-and-techniques/arduino-libraries
 

Customization:
You're probably going to want to change a few things in the sketch to meet your specific
setup (your washer/dryer is probably not an exact match of mine). For sure you'll need to change
your Wi-Fi info - the SSID (Name of your wireless network), and the password to said network. 
And you'll probably need to change your IP configuration. You will need to update your email
address(es) to send the notifications to you.


A debugging note:
I left the serial console lines (serial.println(""); for example) uncommented even 
though I'm done with the code pretty much, but it helps to have them run when you're 
calibrating so you can see what's going on. Even when you're done, you can leave those lines
in there as it doesn't seem to hurt anything (at least that's been my experience).


I tried to comment the code, so things may make more sense when you check out
the sketch.


For more information about Launitor, my Laundry Monitor project, please visit my blog and 
click on the 'Projects' link:
http://jamienerd.blogspot.com
