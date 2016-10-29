/* 
 *  
 * Launitor (Laundry Monitor) v1.0
 
   Note - this sketch was built and tested with the Arduino IDE v1.6.5 http://www.arduino.cc
   Note - this sketch uses Adafruit's Huzzah breakout board for the ESP8266! https://www.adafruit.com/products/2471
   Note - this sketch uses Adafruit's LIS3DH accelerometer boards. https://www.adafruit.com/products/2809
   Note - this sketch uses the free smtp2go service to send email. http://www.smtp2go.com/
   Note - you need to encode your smtp2go username and password and enter it in the 'sendEmail'
          function in the appropriate place. You can use the free website https://www.base64encode.org/ 
          to convert your smtp2go username and password into Base64 format.

   Note - if you're going to be hooking up 2 accelerometers and use I2C for communication, you need to make
   sure that your accelerometers have different 'addresses'. If you use the one that I mention in the next
   paragraph, it's easy to do. Set the 'SDO' pin on one of them to GND (or just leave open), and set the
   other one to 3.3V. This will assign the first one to address of 0x18, and the second one to 0x19.
   See https://learn.adafruit.com/adafruit-lis3dh-triple-axis-accelerometer-breakout/pinouts
 
   This sketch reads 2 accelerometers to detect the vibrations of a washing machine and/or dryer
    while it is running. 
   Note - This sketch was built around an accelerometer (LIS3DH) that has the feature
   of tap/double-tap detection such as the one sold by Adafruit:
   https://www.adafruit.com/products/2809


    ToDos:
    1. Should we move the click/tap detection out of the individual if/then blocks?



    Notes:
    When code is complete, leave 'debugging' (serial.println) lines in place, but comment them out of course, 
      and add a comment or 2 about uncommenting it, telling what that line is for


    ###### Explanation of how the code (logic) works: #######
    ###### This is a high-level overview - it doesn't go into detail ########
    First, we monitor the accelerometer(s) for taps/double-taps. 
    If there's a tap, and it's the first tap we detect, we record that exact moment in time and start counting up time from there
      AND we start a number-of-taps-counter 
    With each following tap we detect, we increment the counter by 1
    If we meet the minimum amount of taps within the set time window, we declare the washer or dryer is running
      AND record that exact moment in time for the next section - determining when the washer or dryer is done
    If we do NOT see the minimum amount of taps within the set time window, we reset the number-of-taps-counter
      and we basically start from the beginning again watching for that initial tap, etc.

    Next, if we've determined that the washer or dryer IS running
    (Remember - we started counting time up from when we initially determined that the washer or dryer is running)
    If we are under the maximum time window then we check:
      With every tap, we increment our tap counter
      If the max amount of taps exceeds our limit, reset the counter, and record this exact moment in time
      (overriding the previous one)
    We never exceed the maximum amount of time AND maximum amount of taps, because almost everything happens in the
      if/then block that executes code for when we're still within the max amount of time window...
    Sooooo, if we actually reach the maximum window of time, then we know that the washer or dryer is done because the
      maximum amount of taps couldn't have been exceeded. 
        NOTE - because of this fact, the washer or dryer will always be declared done when that maximum amount of time 
        has been reached - NEVER BEFORE!

    Now that the washer or dryer has been declared done, we just have to run notification code...

    When the user hits the 'reset' button, everything gets reset and we start from the very beginning again, 
    monitoring the washer/dryer
    ###### End of explanation of how the code (logic) works: #######




  Quick pin reference:
  4 - I2C:SDA
  5 - I2C:SCL
  12 - LED - washer
  13 - LED - dryer
  14 - Reset button - washer
  15 - Reset button - dryer


  Libraries used, locations:

  ESP8266wifi library - https://github.com/ekstrand/ESP8266wifi
  Wire library (for I2C) - (available/standard library with Arduino IDE? You may not have to download/install this specifically) https://www.arduino.cc/en/Reference/Wire
    (the I2C is for communication with the accelerometers)
  SPI library - (available/standard library with Arduino IDE? You may not have to download/install this specifically) https://www.arduino.cc/en/Reference/SPI
  Adafruit LIS3DH library - https://github.com/adafruit/Adafruit_LIS3DH/archive/master.zip
  Adafruit Sensor library - https://github.com/adafruit/Adafruit_Sensor/archive/master.zip

  
 */


 

#include <Wire.h> // For I2C communications with the accelerometer(s)
#include <SPI.h> // Not sure why we need to keep this as we're using I2C, but this sketch fails compilation if we comment it out
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <ESP8266WiFi.h>



// The 2 lines immediately following this comment is for I2C:
Adafruit_LIS3DH lis1 = Adafruit_LIS3DH(); // washer
Adafruit_LIS3DH lis2 = Adafruit_LIS3DH(); // dryer

#if defined(ARDUINO_ARCH_SAMD)
// for Zero, output on USB Serial console, remove line below if using programming port to program the Zero!
   #define Serial SerialUSB
#endif



// Adjust this number for the sensitivity of the 'click' force
// this strongly depends on the range! for 16G, try 5-10
// for 8G, try 10-20. for 4G try 20-40. for 2G try 40-80
// (the lower the number the more sensitive it is)
#define washerClickThreshhold 7 // ------------------ UPDATE THIS FOR YOUR WASHER ----------------------
#define dryerClickThreshhold 6 // ------------------ UPDATE THIS FOR YOUR DRYER ----------------------

 
// ------------------ UPDATE THESE CONSTANTS FOR YOUR WASHER/DRYER ----------------------
const unsigned long washerXSeconds = 300; // The maximum window of time (in seconds) to detect a certain amount (this amount is determined in the below constant, 'washerNumOfTapsInASetTimePeriodToBeDeclaredRunning') of taps
const int washerNumOfTapsInASetTimePeriodToBeDeclaredRunning = 75; // How many taps we need to see within a given amount of time frame (time frame is the above constant, 'washerXSeconds') to decide that the washer is running
const unsigned long washerIsRunningXSeconds = 600; // The maximum window of time (in seconds) to determine whether the washer is done yet (works with below variable, 'washerMaxNumOfTapsInASetTimePeriodToBeDeclaredDone')
const unsigned long washerMaxNumOfTapsInASetTimePeriodToBeDeclaredDone = 3; // The maximum number of taps we allow to determine that the washer is done within a given amount of time frame (time frame is the above constant, 'washerIsRunningXSeconds')
const unsigned long dryerXSeconds = 300; // The maximum window of time (in seconds) to detect a certain amount (this amount is determined in the below constant, 'dryerNumOfTapsInASetTimePeriodToBeDeclaredRunning') of taps
const int dryerNumOfTapsInASetTimePeriodToBeDeclaredRunning = 75; // How many taps we need to see within a given amount of time frame (time frame is the above constant, 'dryerXSeconds') to decide that the dryer is running
const unsigned long dryerIsRunningXSeconds = 360; // The maximum window of time (in seconds) to determine whether the dryer is done yet (works with below variable, 'dryerMaxNumOfTapsInASetTimePeriodToBeDeclaredDone')
const unsigned long dryerMaxNumOfTapsInASetTimePeriodToBeDeclaredDone = 2; // The maximum number of taps we allow to determine that the dryer is done within a given amount of time frame (time frame is the above constant, 'dryerIsRunningXSeconds')
// ------------------ UPDATE THESE CONSTANTS FOR YOUR WASHER/DRYER ----------------------



// Define/set the pins for the 2 LEDs
#define washerLEDPin 12      // LED connected to digital pin 12
#define dryerLEDPin 13      // LED connected to digital pin 13


// begin variables, constants for making an LED blink when the washer and/or dryer is running
int washerLEDState = LOW; // used to set the washer LED
unsigned long washerPreviousMillisForLED = 0; // used to store the last time the washer LED was updated
const long washerLEDBlinkInterval = 1000; // interval at which to blink (in milliseconds) the washer LED
int dryerLEDState = LOW; // used to set the dryer LED
unsigned long dryerPreviousMillisForLED = 0; // used to store the last time the dryer LED was updated
const long dryerLEDBlinkInterval = 1000; // interval at which to blink (in milliseconds) the dryer LED
// end variables, constants for making an LED blink when the washer and/or dryer is running



// these variables will change:
int washerSensorDetectorCounter = 0;  // variable to keep track how many times in a set amount of time a vibration was detected
int washerSensorDetectorCounterWasherRunning = 0;  // variable to keep track how many times in a set amount of time a vibration was detected after we know that the washer has been running
int dryerSensorDetectorCounter = 0;  // variable to keep track how many times in a set amount of time a vibration was detected
int dryerSensorDetectorCounterDryerRunning = 0;  // variable to keep track how many times in a set amount of time a vibration was detected after we know that the dryer has been running


boolean washerHasDetectedTapYet = 0; // This is to determine if we have detected a tap yet or not on the washer
// 0 = washer hasn't had any taps detected yet, 1 = washer has had an initial tap detection
boolean dryerHasDetectedTapYet = 0; // This is to determine if we have detected a tap yet or not on the dryer
// 0 = dryer hasn't had any taps detected yet, 1 = dryer has had an initial tap detection


unsigned long washerXSecondsCounter = 0;     // variable to count to X seconds for the washer (for detecting whether or not the washer has started running)
unsigned long washerIsRunningXSecondsCounter = 0;  // variable to count up to X seconds for the washer (for detecting whether or not the washer has finished running)
boolean washerIsRunning = 0; // washerIsRunning : 0 = the washer is not running; 1 = washer is running
boolean washerIsDone = 0; // washerIsDone : 0 = washer is not done; 1 = washer is done
unsigned long dryerXSecondsCounter = 0;     // variable to count to X seconds for the dryer (for detecting whether or not the dryer has started running)
unsigned long dryerIsRunningXSecondsCounter = 0;  // variable to count up to X seconds for the dryer (for detecting whether or not the dryer has finished running)
boolean dryerIsRunning = 0; // dryerIsRunning : 0 = the dryer is not running; 1 = dryer is running
boolean dryerIsDone = 0; // dryerIsDone : 0 = dryer is not done; 1 = dryer is done


unsigned long washerIsDoneXSecondsCounter = 0; // variable to keep track of seconds once the washer is done running for notifications
unsigned long dryerIsDoneXSecondsCounter = 0; // variable to keep track of seconds once the dryer is done running for notifications


// The below are for when the washer/dryer is complete and whether you want to be notified more than once,
//   and if so, how often? (How many minutes between each notification)
const boolean notifyWasherDoneMultipleTimes = 1; // 1 = will notify multiple times for the washer being done (besides LED); 0 = will notify just 1 time
const boolean notifyDryerDoneMultipleTimes = 1; // 1 = will notify multiple times for the dryer being done (besides LED); 0 = will notify just 1 time
const int notifyWasherDoneEveryXMinutes = 30; // If the above constant is set to 1, how often will we 'notify' (for the washer), in minutes
const int notifyDryerDoneEveryXMinutes = 30; // If the above constant is set to 1, how often will we 'notify' (for the dryer), in minutes
unsigned long tempVariableForWasherCalc = 0; // A variable to hold temporary calculations for when multiple notifications for the washer is enabled
unsigned long tempVariableForDryerCalc = 0; // A variable to hold temporary calculations for when multiple notifications for the dryer is enabled




// button stuff
#define washerResetButton 14  // the input pin where the washer pushbutton is connected
int washerButtonVal = 0;      // washerButtonVal will be used to store the state of the washer input pin

#define dryerResetButton 15  // the input pin where the dryer pushbutton is connected
int dryerButtonVal = 0;      // dryerButtonVal will be used to store the state of the dryer input pin
// button stuff



// button debouncing stuff
int washerLastButtonState = LOW;  // the washer previous debounced button state
int washerLastReading= LOW;       // the washer previous reading from the input pin
int dryerLastButtonState = LOW;  // the dryer previous debounced button state
int dryerLastReading= LOW;       // the dryer previous reading from the input pin


// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long washerLastDebounceTime = 0;  // the last time the washer input pin was toggled
long dryerLastDebounceTime = 0;  // the last time the dryer input pin was toggled

// we can get away with having just 1 delay variable for both buttons
long debounceDelay = 10;    // the debounce time; increase if the output flickers
// button debouncing stuff







// Wi-Fi stuff
const char* ssid = "YOUR WIRELESS SSID HERE!!!";
const char* password = "YOUR WIRELESS PASSWORD HERE!!!";

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);
WiFiClient client;

// this must be unique for your module
byte mac[] = { 0x18, 0xFE, 0x34, 0xA6 ,0x04, 0x10 }; //from ESP8266 
// change network settings to yours
IPAddress ip( 192, 168, 0, 130 );    
IPAddress gateway( 192, 168, 0, 1 );
IPAddress subnet( 255, 255, 255, 0 );
//IPAddress server(?,?,?,?);  // numeric IP for Yahoo (no DNS)
char myserver[] = "mail.smtp2go.com";
// int port = 465;
int port = 2525;
// End Wi-Fi stuff





void setup() {


 
  #ifndef ESP8266
    while (!Serial);     // will pause Zero, Leonardo, etc until serial console opens
  #endif
  
  Serial.begin(9600);
    
  if (! lis1.begin(0x18)) {   // change this to 0x19 for alternative i2c address
    Serial.println("Couldnt start washer sensor");
    while (1);
  }
  Serial.println("LIS3DH washer sensor found!");

  if (! lis2.begin(0x19)) {   // changed to 0x19 for alternative i2c address
    Serial.println("Couldnt start dryer sensor");
    while (1);
  }
  Serial.println("LIS3DH dryer sensor found!");
  
  
  lis1.setRange(LIS3DH_RANGE_16_G);   // 2, 4, 8 or 16 G! // washer
  lis2.setRange(LIS3DH_RANGE_16_G);   // 2, 4, 8 or 16 G! // dryer
  
  Serial.print("Range1 = "); Serial.print(2 << lis1.getRange());  
  Serial.println("G");
  Serial.print("Range2 = "); Serial.print(2 << lis2.getRange());  
  Serial.println("G");
  Serial.println("");
  
  // 0 = turn off click detection & interrupt
  // 1 = single click only interrupt output
  // 2 = double click only interrupt output, detect single click
  // Adjust threshhold, higher numbers are less sensitive
  lis1.setClick(2, washerClickThreshhold);
  lis2.setClick(2, dryerClickThreshhold);
  delay(100);




  
  pinMode(washerLEDPin, OUTPUT); // declare the washerLEDPin as as OUTPUT
  pinMode(washerResetButton, INPUT);  // and washerResetButton is an input
  
  pinMode(dryerLEDPin, OUTPUT); // declare the dryerLEDPin as as OUTPUT
  pinMode(dryerResetButton, INPUT);  // and dryerResetButton is an input



  WiFi.begin(ssid, password);
  WiFi.config(ip, gateway, subnet);



  // Begin Wi-Fi stuff
  server.begin(); 
  delay(10000);
  // End Wi-Fi stuff



  
  // A visual confirmation that device seems to be working - blink both LEDs 3 times at the same time
  for (int z = 1; z <= 3; z++) {
    digitalWrite(washerLEDPin, HIGH);
    digitalWrite(dryerLEDPin, HIGH);
    delay(500);
    digitalWrite(washerLEDPin, LOW);
    digitalWrite(dryerLEDPin, LOW);
    delay(500);
  }
  
 
} //end 'Setup' section












void loop() {

  

  // ************************************** NOTE TO SELF *****************************************
  // The below should go in an 'IF' statement so that the button only performs the actions if
  // the system is in 'washer completed' mode ??
  // ************************************** NOTE TO SELF *****************************************
  // monitor the Washer Reset Button
  washerButtonVal = digitalRead(washerResetButton);  // read the input value of the washerResetButton
 
  // check to see if you just pressed the washer button
  // (i.e. the input went from LOW to HIGH),  and you've waited
  // long enough since the last press to ignore any noise:  

  // If the switch changed, due to noise or pressing:
  if (washerButtonVal != washerLastReading) {
    // reset the debouncing timer
    washerLastDebounceTime = millis();
    // save the reading.  Next time through the loop,
    // it'll be lastReading:
    washerLastReading = washerButtonVal;
  }

  if ((millis() - washerLastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so accept the button changed state:
  
    // go ahead and execute the following code if the state of the washer button changes from LOW to HIGH:
    if (washerLastButtonState == LOW && washerButtonVal == HIGH) {
      // EXECUTE EVERYTHING IN HERE!!!
      //Serial.println("Washer LED turning off due to reset button");
      digitalWrite(washerLEDPin, LOW); // turn off the washer notification LED
      //Serial.println("Washer LED OFF due to reset button");
      // and reset variables
      washerSensorDetectorCounterWasherRunning = 0;
      washerIsRunningXSecondsCounter = 0;
      washerIsDone = 0;
      washerIsRunning = 0;
      washerSensorDetectorCounter = 0;
      washerHasDetectedTapYet = 0;
      washerXSecondsCounter = 0;
      tempVariableForWasherCalc = 0;
      Serial.println("");
      Serial.println("Washer Acknowledgement button pressed");
      Serial.println("");
      // EXECUTE EVERYTHING IN HERE!!!
    }
    washerLastButtonState = washerButtonVal;
  }



  
  // ************************************** NOTE/UPDATE *****************************************
  // The below should go in an 'IF' statement so that the button only performs the actions if
  // the system is in 'dryer completed' mode ??
  // ************************************** NOTE/UPDATE *****************************************
  // monitor the Dryer Reset Button
  dryerButtonVal = digitalRead(dryerResetButton);  // read the input value of the dryerResetButton

  // check to see if you just pressed the dryer button
  // (i.e. the input went from LOW to HIGH),  and you've waited
  // long enough since the last press to ignore any noise:  

  // If the switch changed, due to noise or pressing:
  if (dryerButtonVal != dryerLastReading) {
    // reset the debouncing timer
    dryerLastDebounceTime = millis();
    // save the reading.  Next time through the loop,
    // it'll be lastReading:
    dryerLastReading = dryerButtonVal;
  }

  if ((millis() - dryerLastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so accept the button changed state:
  
    // go ahead and execute the following code if the state of the dryer button changes from LOW to HIGH:
    if (dryerLastButtonState == LOW && dryerButtonVal == HIGH) {
      // EXECUTE EVERYTHING IN HERE!!!
      //Serial.println("Dryer LED turning off due to reset button");
      digitalWrite(dryerLEDPin, LOW); // turn off the dryer notification LED
      //Serial.println("Dryer LED OFF due to reset button");
      // and reset variables
      dryerSensorDetectorCounterDryerRunning = 0;
      dryerIsRunningXSecondsCounter = 0;
      dryerIsDone = 0;
      dryerIsRunning = 0;
      dryerSensorDetectorCounter = 0;
      dryerHasDetectedTapYet = 0;
      dryerXSecondsCounter = 0;
      tempVariableForDryerCalc = 0;
      Serial.println("");
      Serial.println("Dryer Acknowledgement button pressed");
      Serial.println("");
      // EXECUTE EVERYTHING IN HERE!!!
    }
    dryerLastButtonState = dryerButtonVal;
  }


  uint8_t click1 = lis1.getClick(); // washer
  uint8_t click2 = lis2.getClick(); // dryer

  



  // This section is the 'reset' code block for the washer - if the set amount of time goes by without detecting the
  //  minimum number of 'taps', it will reset the counter back to 0
  // (so, if the set amount of seconds pass by without hitting the set amount of activity)
  if (millis() >= washerXSecondsCounter + (washerXSeconds * 1000) && washerSensorDetectorCounter > 0 && washerIsDone == 0) {
    // reset stuff
    Serial.println("Washer - time expired, resetting...");
    washerHasDetectedTapYet = 0; // set this back to 0
    washerXSecondsCounter = 0; // set this back to 0
    washerSensorDetectorCounter = 0; // set this back to 0
    Serial.println("Washer - reset complete");
  } // end if the set amount of seconds pass by without hitting the set amount of activity for the washer


  // This section is the 'reset' code block for the dryer - if the set amount of time goes by without detecting the
  //  minimum number of 'taps', it will reset the counter back to 0
  // (so, if the set amount of seconds pass by without hitting the set amount of activity)
  if (millis() >= dryerXSecondsCounter + (dryerXSeconds * 1000) && dryerSensorDetectorCounter > 0 && dryerIsDone == 0) {
    // reset stuff
    Serial.println("Dryer - time expired, resetting...");
    dryerHasDetectedTapYet = 0; // set this back to 0
    dryerXSecondsCounter = 0; // set this back to 0
    dryerSensorDetectorCounter = 0; // set this back to 0
    Serial.println("Dryer - reset complete");
  } // end if the set amount of seconds pass by without hitting the set amount of activity for the dryer

  

  // This section is the 'washer detection' code block
  // if washer is NOT running (i.e. maybe microcontroller was just turned on)
  if (washerIsRunning == 0 && washerIsDone == 0) {

    // if a single tap OR double tap was detected
    if (click1 & 0x10 || click1 & 0x20) {

      // if this is the FIRST tap detected
      if (washerHasDetectedTapYet == 0) {
        Serial.println("Washer - Initial tap detected!");
        washerSensorDetectorCounter = washerSensorDetectorCounter + 1; // increment the detector counter by 1 (yes, it is just 1 right here since this is the first time)
        washerXSecondsCounter = millis(); // set 'washerXSecondsCounter' to this point in time (to the current millis())
        washerHasDetectedTapYet = 1; // set variable to show that a tap has been detected now
      } else { // otherwise, if there's already been an 'initial tap'...
        washerSensorDetectorCounter = washerSensorDetectorCounter + 1; // increment the detector counter
        Serial.println("Washer - Single/Double tap detected!");
      } // end if this is the FIRST tap detected
      
    } // end if a single click OR double click was detected
    
  } // end if washer is NOT running
  

  
  // This section is the 'dryer detection' code block
  // if dryer is NOT running (i.e. maybe microcontroller was just turned on)
  if (dryerIsRunning == 0 && dryerIsDone == 0) {

    // if a single tap OR double tap was detected
    if (click2 & 0x10 || click2 & 0x20) {

      // if this is the FIRST tap detected
      if (dryerHasDetectedTapYet == 0) {
        Serial.println("Dryer - Initial tap detected!");
        dryerSensorDetectorCounter = dryerSensorDetectorCounter + 1; // increment the detector counter by 1 (yes, it is just 1 right here since this is the first time)
        dryerXSecondsCounter = millis(); // set 'dryerXSecondsCounter' to this point in time (to the current millis())
        dryerHasDetectedTapYet = 1; // set variable to show that a tap has been detected now
      } else { // otherwise, if there's already been an 'initial tap'...
        dryerSensorDetectorCounter = dryerSensorDetectorCounter + 1; // increment the detector counter
        Serial.println("Dryer - Single/Double tap detected!");
      } // end if this is the FIRST tap detected
      
    } // end if a single click OR double click was detected
    
  } // end if dryer is NOT running
 


  // This section of code is what's run when we initially determine that the washer is running
  if (washerSensorDetectorCounter == washerNumOfTapsInASetTimePeriodToBeDeclaredRunning && washerIsDone == 0) {
    washerIsRunning = 1;
    washerSensorDetectorCounter = 0;
    washerXSecondsCounter = 0;
    washerIsRunningXSecondsCounter = millis(); // let's take this point in time for reference in the next section of code (the section that detects if the dryer is still running or if it's done)
    Serial.println("**************************WASHER IS RUNNING*************************");
    return; // This seems to be required in order to prevent the little 'bug' that we had where immediately after
      // it's been determined that the washer is running, we would get a 'Washer Is Still Running' (see next section
      // of code).
  }
  

  // This section of code is what's run when we initially determine that the dryer is running
  if (dryerSensorDetectorCounter == dryerNumOfTapsInASetTimePeriodToBeDeclaredRunning && dryerIsDone == 0) {
    dryerIsRunning = 1;
    dryerSensorDetectorCounter = 0;
    dryerXSecondsCounter = 0;
    dryerIsRunningXSecondsCounter = millis(); // let's take this point in time for reference in the next section of code (the section that detects if the dryer is still running or if it's done)
    Serial.println("**************************DRYER IS RUNNING*************************");
    return; // This seems to be required in order to prevent the little 'bug' that we had where immediately after
      // it's been determined that the dryer is running, we would get a 'Dryer Is Still Running' (see next section
      // of code).
  }





  // This section of code is the part that determines when the washer is done running
  if (washerIsRunning == 1 && washerIsDone == 0) {

    // if we're still under the set time/amount of seconds before we can declare the washer done...
    if (millis() < washerIsRunningXSecondsCounter + (washerIsRunningXSeconds * 1000)) {
      
      // if a single tap OR double tap was detected
      if (click1 & 0x10 || click1 & 0x20) { // if we detect a tap or double tap
        //Serial.print("washerSensorDetectorCounterwasherRunning = "); Serial.print(washerSensorDetectorCounterWasherRunning);
        washerSensorDetectorCounterWasherRunning = washerSensorDetectorCounterWasherRunning + 1; // increment the counter by 1
        Serial.println("Washer Is Still Running"); // message saying washer is still running
      } // End if a single or double tap was detected

      // if the the max number of taps allowed is exceeded
      if (washerSensorDetectorCounterWasherRunning > washerMaxNumOfTapsInASetTimePeriodToBeDeclaredDone) {
        // Reset stuff here
        Serial.println("Washer - time/taps exceeded"); // message saying time/taps exceeded - washer is still running
        washerSensorDetectorCounterWasherRunning = 0; // set to 0 (basically resetting it)
        washerIsRunningXSecondsCounter = millis(); // update reference time to this point in time (kind of like resetting it)
      } // End if the the max number of taps allowed is exceeded
      
    } else {
      // if washer has exceeded time limit for when it is running...

      // if the number of taps have not exceeded the set limit...the washer is done!
      if (washerMaxNumOfTapsInASetTimePeriodToBeDeclaredDone >= washerSensorDetectorCounterWasherRunning) {
        Serial.println("--------------------------------------------------Washer is Done!--------------------------------------------------");
        //Serial.println("Turn on washer LED since it's done!");
        turnOnLEDWhenDryerOrWasherIsDone(0); // turn on the washer-is-done notification LED
        //Serial.println("Washer LED ON because it's done!");
        washerSensorDetectorCounterWasherRunning = 0;
        washerIsRunningXSecondsCounter = 0;
        washerIsDone = 1;
        washerIsRunning = 0;
        washerIsDoneXSecondsCounter = millis();
        notifyRepeatWasherOrDryerIsDone(0); // 0 = washer
        delay(500);
        return; // This seems to be required in order to prevent the little 'bug' that we had where immediately after
                // the email was sent, it would sometimes turn off the LED, which I'm sure was happening because the line, 'blinkLEDWhiteMachineIsRunning'
                // would run 1 more time. The said line is a few lines below
      } else {
        // This should never happen!
        Serial.println("Houston, we have a problem (washer)");
      }
      
    } // End if we're still under the set time/amount of seconds

 
  //Serial.println("Blink LED (for washer) called");
  blinkLEDWhileMachineIsRunning(0); // Let's blink the washer LED to show that the washer is running
  //Serial.println("Blink LED (for washer) executed");
    
  } // End if washer is running and not done

  




  // This section of code is the part that determines when the dryer is done running
  if (dryerIsRunning == 1 && dryerIsDone == 0) {

    // if we're still under the set time/amount of seconds before we can declare the dryer done...
    if (millis() < dryerIsRunningXSecondsCounter + (dryerIsRunningXSeconds * 1000)) {
      
      // if a single tap OR double tap was detected
      if (click2 & 0x10 || click2 & 0x20) { // if we detect a tap or double tap
        //Serial.print("dryerSensorDetectorCounterDryerRunning = "); Serial.print(dryerSensorDetectorCounterDryerRunning);
        dryerSensorDetectorCounterDryerRunning = dryerSensorDetectorCounterDryerRunning + 1; // increment the counter by 1
        Serial.println("Dryer Is Still Running"); // message saying dryer is still running
      } // End if a single or double tap was detected

      // if the the max number of taps allowed is exceeded
      if (dryerSensorDetectorCounterDryerRunning > dryerMaxNumOfTapsInASetTimePeriodToBeDeclaredDone) {
        // Reset stuff here
        Serial.println("Dryer - time/taps exceeded"); // message saying time/taps exceeded - dryer is still running
        dryerSensorDetectorCounterDryerRunning = 0; // set to 0 (basically resetting it)
        dryerIsRunningXSecondsCounter = millis(); // update reference time to this point in time (kind of like resetting it)
      } // End if the the max number of taps allowed is exceeded
      
    } else {
      // if dryer has exceeded time limit for when it is running...

      // if the number of taps have not exceeded the set limit...the dryer is done!
      if (dryerMaxNumOfTapsInASetTimePeriodToBeDeclaredDone >= dryerSensorDetectorCounterDryerRunning) {
        Serial.println("--------------------------------------------------Dryer is Done!--------------------------------------------------");
        //Serial.println("Turn on dryer LED since it's done!");
        turnOnLEDWhenDryerOrWasherIsDone(1); // turn on the dryer-is-done notification LED
        //Serial.println("Dryer LED ON because it's done!");
        dryerSensorDetectorCounterDryerRunning = 0;
        dryerIsRunningXSecondsCounter = 0;
        dryerIsDone = 1;
        dryerIsRunning = 0;
        dryerIsDoneXSecondsCounter = millis();
        notifyRepeatWasherOrDryerIsDone(1); // 1 = dryer // LED TURNS OFF HERE?????????????????????????????????????????????????????????????????
        delay(500);
        return; // This seems to be required in order to prevent the little 'bug' that we had where immediately after
                // the email was sent, it would sometimes turn off the LED, which I'm sure was happening because the line, 'blinkLEDWhiteMachineIsRunning'
                // would run 1 more time. The said line is a few lines below
      } else {
        // This should never happen!
        Serial.println("Houston, we have a problem (dryer)");
      }
      
    } // End if we're still under the set time/amount of seconds

    //Serial.println("Blink LED (for dryer) called");
    blinkLEDWhileMachineIsRunning(1); // Let's blink the dryer LED to show that the dryer is running
    //Serial.println("Blink LED (for dryer) executed");
    
  } // End if dryer is running and not done







  
  // if we want to be notified multiple times, and the washer is done, then let's do stuff!  
  if (notifyWasherDoneMultipleTimes == 1 && washerIsDone == 1) {

    // The next 2 lines are converting the value of 'notifyWasherDoneEveryXMinutes' constant from minutes to milliseconds
    //  (for some reason I couldn't do this properly in-line with the immediately-following 'if' statement)
    tempVariableForWasherCalc = notifyWasherDoneEveryXMinutes * 60; // convert minutes to seconds
    tempVariableForWasherCalc = tempVariableForWasherCalc * 1000; // convert seconds to milliseconds
    
    if (millis() >= washerIsDoneXSecondsCounter + tempVariableForWasherCalc) {
    //if (millis() >= washerIsDoneXSecondsCounter + ((notifyWasherDoneEveryXMinutes * 60) * 1000)) { // Couldn't do this correctly :(
      notifyRepeatWasherOrDryerIsDone(0); // run the repeating notification function
      washerIsDoneXSecondsCounter = millis(); // reset the counter to current milliseconds
    }
    
  } // End if we want to be notified multiple times, and the washer is done, then let's do stuff!








  // if we want to be notified multiple times, and the dryer is done, then let's do stuff!  
  if (notifyDryerDoneMultipleTimes == 1 && dryerIsDone == 1) {

    // The next 2 lines are converting the value of 'notifyDryerDoneEveryXMinutes' constant from minutes to milliseconds
    //  (for some reason I couldn't do this properly in-line with the immediately-following 'if' statement)
    tempVariableForDryerCalc = notifyDryerDoneEveryXMinutes * 60; // convert minutes to seconds
    tempVariableForDryerCalc = tempVariableForDryerCalc * 1000; // convert seconds to milliseconds
    
    if (millis() >= dryerIsDoneXSecondsCounter + tempVariableForDryerCalc) {
    //if (millis() >= dryerIsDoneXSecondsCounter + ((notifyDryerDoneEveryXMinutes * 60) * 1000)) { // Couldn't do this correctly :(
      notifyRepeatWasherOrDryerIsDone(1); // run the repeating notification function
      dryerIsDoneXSecondsCounter = millis(); // reset the counter to current milliseconds
    }
    
  } // End if we want to be notified multiple times, and the dryer is done, then let's do stuff!







  // comment out the following line when this sketch is done!?
  //delay(250);  // delay to avoid overloading the serial port buffer
  delay(50); // a higher delay (like 250 above) seems to be a problem!
             //  and so does having a lower (or no) delay.
             //  see 'notes' in separate file regarding this
  
  
} // End main 'loop'





// This function is responsible to blink the washer/dryer LED while the corresponding machine is running
// It's sort of a visual confirmation that this device knows that the machine is running and is working properly
void blinkLEDWhileMachineIsRunning(boolean washerOrDryer) {
  // washerOrDryer : 0 = washer, 1 = dryer
  
  unsigned long washerCurrentMillis = millis();
  unsigned long dryerCurrentMillis = millis();

  if (washerOrDryer == 0) { // If washer was specified, let's deal with the washer LED
    if (washerCurrentMillis - washerPreviousMillisForLED >= washerLEDBlinkInterval) {
      // save the last time you blinked the LED
      washerPreviousMillisForLED = washerCurrentMillis;
  
      // if the LED is off turn it on and vice-versa:
      if (washerLEDState == LOW) {
        washerLEDState = HIGH;
      } else {
        washerLEDState = LOW;
      }
  
      // set the LED with the ledState of the variable:
      digitalWrite(washerLEDPin, washerLEDState);
    }
  } // end if washerOrDryer == 0


  if (washerOrDryer == 1) { // If dryer was specified, let's deal with the dryer LED
    if (dryerCurrentMillis - dryerPreviousMillisForLED >= dryerLEDBlinkInterval) {
      // save the last time you blinked the LED
      dryerPreviousMillisForLED = dryerCurrentMillis;
  
      // if the LED is off turn it on and vice-versa:
      if (dryerLEDState == LOW) {
        dryerLEDState = HIGH;
      } else {
        dryerLEDState = LOW;
      }
  
      // set the LED with the ledState of the variable:
      digitalWrite(dryerLEDPin, dryerLEDState);
    }
  } // end if washerOrDryer == 1

} // end of blinkLEDWhileMachineIsRunning function




void turnOnLEDWhenDryerOrWasherIsDone(boolean washerOrDryer) {
  // washerOrDryer : 0 = washer, 1 = dryer
  if (washerOrDryer == 0) {
    digitalWrite(washerLEDPin, HIGH);
  } else {
    digitalWrite(dryerLEDPin, HIGH);
  }
} // end of turnOnLEDWhenDryerOrWasherIsDone function





void notifyRepeatWasherOrDryerIsDone(boolean washerOrDryer) {
  // washerOrDryer : 0 = washer, 1 = dryer
  if (washerOrDryer == 0) {
    if(sendEmail(0)) Serial.println(F("Email sent"));
    else Serial.println(F("Email failed"));
  } else {
    if(sendEmail(1)) Serial.println(F("Email sent"));
    else Serial.println(F("Email failed"));
  }
}












byte sendEmail(boolean washerOrDryer) {
  // washerOrDryer : 0 = washer, 1 = dryer
  
  //WiFi.begin(ssid, password);
      
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  
  byte thisByte = 0;
  byte respCode;

  if(client.connect(myserver,port) == 1) {
    Serial.println(F("connected"));
  } else {
    Serial.println(F("connection failed"));
    return 0;
  }
  if(!eRcv()) {Serial.println("before ehlo");return 0 ;}

  Serial.println(F("Sending hello"));
// replace with your ESP8266's ip
  client.println("EHLO 192.168.0.130");
  if(!eRcv()) {Serial.println("ehlo");return 0 ;}

  Serial.println(F("Sending auth login"));
  client.println("auth login");
  if(!eRcv()) {Serial.println("auth");return 0 ;}

  Serial.println("Sending User");
// Change to your base64 encoded user
  client.println("base64EncodedSmtp2goUsernameHere");
 
  if(!eRcv()) {Serial.println("user");return 0 ;}

  Serial.println(F("Sending Password"));
// change to your base64 encoded password
  client.println("base64EncodedSmtp2goPasswordHere");

  if(!eRcv()) {Serial.println("ehlo");return 0;}

// change to your email address (sender)
  Serial.println(F("Sending From"));
  client.println("MAIL From: fromemail@email.com");
  if(!eRcv()) {Serial.println("email");return 0 ;}

// change to recipient address
  Serial.println(F("Sending To"));
  client.println("RCPT To: toemail@email.com");
  //client.println("RCPT To: additionalrecipient@email.com"); // Uncomment this line and update the email address for sending to an additional email recipient
  if(!eRcv()) {Serial.println("email");return 0 ;}

  Serial.println(F("Sending DATA"));
  client.println("DATA");
  if(!eRcv()) {Serial.println("email");return 0 ;}

  Serial.println(F("Sending email"));

// change to recipient address(es)
  client.println("To: Joe Shmo toemail@email.com");
  client.println("To: Jill Shmo toemail@email.com");

// change to your address
  client.println("From: Email Sender fromemail@email.com");


  if (washerOrDryer == 0) { // 0 = washer
    client.println("Subject: Washer is done!\r\n");
    client.println("The washing machine is done!");
  } else { // 1 = dryer
    client.println("Subject: Dryer is done!\r\n");
    client.println("The dryer is done! Please go collect your clothes.\r\n Have a nice day! :)");
  } 


  client.println(".");

  if(!eRcv()) return 0;

  Serial.println(F("Sending QUIT")); //Serial.println(F("Sending QUIT"));
  client.println("QUIT");
  
  if(!eRcv()) return 0;

  client.stop();

  Serial.println(F("disconnected"));

  return 1;
} // end sendEmail function





byte eRcv()
{
  byte respCode;
  byte thisByte;
  int loopCount = 0;

  while(!client.available()) {
    delay(1);
    loopCount++;

    // if nothing received for 10 seconds, timeout
    if(loopCount > 10000) {
      client.stop();
      Serial.println(F("10 sec \r\nTimeout"));
      return 0;
    }
  }

  respCode = client.peek();

  while(client.available())
  {  
    thisByte = client.read();    
    Serial.write(thisByte);
  }

  if(respCode >= '4')
  {
    efail();
    return 0;  
  }

  return 1;
} // end eRcv function





void efail()
{
  byte thisByte = 0;
  int loopCount = 0;

  client.println(F("QUIT"));

  while(!client.available()) {
    delay(1);
    loopCount++;

    // if nothing received for 10 seconds, timeout
    if(loopCount > 10000) {
      client.stop();
      Serial.println(F("efail \r\nTimeout"));
      return;
    }
  }

  while(client.available())
  {  
    thisByte = client.read();    
    Serial.write(thisByte);
  }

  client.stop();

  Serial.println(F("disconnected"));
} // end efail function


