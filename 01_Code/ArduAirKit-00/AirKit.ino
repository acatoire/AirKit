/*
 AirKit
 Language: Arduino

 This program get temperature info from two OneWire Temperature sensors then :
   - sends a serial messages containing it  
   - activate two outputs depending of the temperature delta

   The circuit:
 * 2 OneWire sensor on pin 2 and 4
 * jumper for summer/winter state

 */
#include "Timer.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

// ***** Board configuration
// Led definition
#define LED 13
bool ledState = 0;
// Serial string management variables
String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete

// ***** Libraries configuration
//OneWire configuration for inside temp sensor
OneWire oneWireIn(2);                     // Setup a oneWire instance on pin 2
DallasTemperature sensorsIn(&oneWireIn);  // Pass our oneWire reference to Dallas Temperature.
DeviceAddress insideThermometer;          // arrays to hold device address
//OneWire configuration for outside temp sensor
OneWire oneWireOut(4);                     // Setup a oneWire instance on pin 4
DallasTemperature sensorsOut(&oneWireOut); // Pass our oneWire reference to Dallas Temperature.
DeviceAddress outsideThermometer;          // arrays to hold device address
// Timer configuration
Timer t;

// ***** System elements
float tempIn = 20;   //Inside room temperature value
float tempOut = 30;  //Outside room temperature value
float tempDelta = 0; //Temperature delta between inside and outside (tempOut - tempIn)
bool gateState = 0;  //State of the gate, 0 = close 1 = open
bool fanState = 1;   //State of the fan, 0 = stopped 1 = running

// ***** System Configuration
bool summerMode;      //Boolean to activate the summer mode
// We need two different delta value to avoid tilting ON/OFF/ON/OFF the air flow
byte deltaFanON = 10; //Minimum delta to activate the air flow
byte deltaFanOFF = 5; //Maximum delta to disable the air flow

/* void setup(void) 
 *  Setup software run only once
 *  
*/
void setup()
{
  // Elements initialisation
  pinMode(LED, OUTPUT); // initialize the digital pin as an output. 
  Serial.begin(9600); // start serial port at 9600 bps:
  int tickEvent = t.every(1000, measureAction); // Establish communication every seconds
  // OneWire temperature setup
  sensorsIn.begin();    // Start Sensor libraries  
  sensorsOut.begin();    // Start Sensor libraries  
  if (!sensorsIn.getAddress(insideThermometer, 0))   Serial.println("Unable to find address for Device 0"); 
  if (!sensorsOut.getAddress(outsideThermometer, 0)) Serial.println("Unable to find address for Device 0"); 
  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensorsIn.setResolution(insideThermometer, 12);
  sensorsOut.setResolution(insideThermometer, 12);
  
  // Parameters initialisation from EEPROM
  deltaFanON = EEPROM.read(0);
  deltaFanOFF = EEPROM.read(1);
  summerMode = EEPROM.read(2);
  
}

/* void loop(void) 
 *  Main program automatically loaded
 *  
*/
void loop()
{
  t.update(); // Update the timer
  
  //Check for serial input
  if(Serial.available() > 0)
  {
    serialStack();
  }
  
  if (stringComplete)
  {
    executeCommand();
    inputString = "";
    stringComplete = false;
  }
}

/* void measureAction(void) is executed every 1000ms by a Timer
 *  It read the temperatures values and change the system value
 *  This is the main function
*/
void measureAction() {
  ledState = ledState ^ 1;
  digitalWrite(LED, ledState);
  //Get the temperature value
  tempIn = sensorsIn.getTempC(insideThermometer); 
  tempOut = sensorsOut.getTempC(outsideThermometer); 
  
  // Calculate the temperature delta
  tempDelta = tempOut - tempIn;
  
  // Choose action depending of the mode
  if (summerMode)
  {// In summer mode the delta will be negative when interesting
      // Turn On the Fan if the outside temperature is cold enough
      if (tempDelta < -deltaFanON)
      {
        gateState = 1;  
        fanState = 1;
      }
      // Turn Off the Fan if the outside temperature goes up back to a specific level
      if (tempDelta > -deltaFanOFF)
      {
        gateState = 0;  
        fanState = 0;
      }
  }
  else
  {// In winter mode the delta will be positive when interesting
      // Turn On the Fan if the outside temperature is hot enough
      if (tempDelta > deltaFanON)
      {
        gateState = 1;  
        fanState = 1;
      }
      // Turn Off the Fan if the outside temperature goes down back to a specific level
      if (tempDelta < deltaFanOFF)
      {
        gateState = 0;  
        fanState = 0;
      }
  }
  // If someone is connected to the serial communication port (Useful only on Leonardo)
  if (Serial) 
  { //Communicate the system values
    Serial.print(summerMode + ';' +
                 tempIn + ';' +
                 tempOut + ';' +
                 tempDelta + ';' +
                 fanState + ';' +
                 gateState + ';' );
  }
  
  // Start the new conversion
  sensorsIn.requestTemperatures();  // Send the command to get temperatures
  sensorsOut.requestTemperatures(); // Send the command to get temperatures
}

// 
/* void printAddress(DeviceAddress deviceAddress)
 *  Function to print a OneWire device address
 *  
 *  Input : the address of the device to print
*/
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

/* void printInfo(void)
 *  Print on serial output the status of the application
 *
*/
void printInfo()
{
    Serial.print("TempIn: Parasite power is "); 
    if (sensorsIn.isParasitePowerMode()) Serial.print("ON");
    else Serial.print("OFF ");
    Serial.print("Address: ");
    printAddress(insideThermometer);
    Serial.println();
    
    Serial.print("TempOut: Parasite power is "); 
    if (sensorsOut.isParasitePowerMode()) Serial.print("ON");
    else Serial.print("OFF ");
    Serial.print("Address: ");
    printAddress(outsideThermometer);
    Serial.println();
    
    Serial.print("Winter Fan Delta : ");
    Serial.print(deltaFanON);
    Serial.print("/");
    Serial.println(deltaFanOFF);
}

/* void serialStack(void)
 *  Stack all serial char received in one string until a '\n'
 *  Set stringComplete to True when '\n' is received
 *  This implementation is good enough for this project because serial commands
 *  will be send slowly.
*/
void serialStack()
{
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    
    // if the incoming character is a newline, set a flag
    if (inChar == '\n') 
    {// so the main loop can do something about it
      stringComplete = true;
    }
    else
    {// add it to the inputString
      inputString += inChar;
    }
  }
}

/* void executeCommand(void)
 *  Execute received serial command
 *  Commandes list :
 *   - "i", "I", "info", "Info" : Return basic system informations, mainly for debug purpose
 *   - "FXX"                    : Setup the minimum temperature delta to activate the Airflow (0 to 100 C)
 *   - "fXX"                    : Setup the maximum temperature delta to disable the Airflow (0 to 100 C)
 *   - "S1" or "S0"             : Enable ("S1") or disable ("S0") the summer mode
 *
*/
void executeCommand()
{
  // Define the appropriate action depending on the first character of the string
  switch (inputString[0]) 
  {
    // INFO Request
    case 'i':
    case 'I':
      printInfo(); // Print on serial the system info
      break;
    // Airflow ON setup 
    case 'F':
      inputString.remove(0,1);
      Serial.print("F > Change the winter Delta ON to : ");
      Serial.println(inputString);
      deltaFanON = inputString.toInt();
      EEPROM.update(0, deltaFanON);
      break;
    // Airflow OFF setup 
    case 'f':
      inputString.remove(0,1);
      Serial.print("f > Change the winter Delta OFF to : ");
      Serial.println(inputString);
      deltaFanOFF = inputString.toInt();
      EEPROM.update(0, deltaFanOFF);
      break;
    // Summer Mode setup 
    case 's':
    case 'S':
      inputString.remove(0,1);
      Serial.print("S > Set Summer Mode to : ");
      Serial.println(inputString);
      summerMode = (1 == inputString.toInt());
      EEPROM.update(3, summerMode);
      break;
    // Unknown command 
    default: 
      Serial.print(inputString[0]);
      Serial.println(' > ?');
  }
}
