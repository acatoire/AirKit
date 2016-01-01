
/*
 *  First implementation of the airKit project on esp8266 with arduino
 *  - Get information from 2 DS18B20 sensor
 *  - Logging on thingspeak
 *  - The use of timers allows to optimize and creat time based environment
 *  - Configuration from serial link
 *  - Configuration saved in eeprom
 *  - Configuration from internet possible
 *  
 *
 *  Developed from :
 *    https://github.com/atwc/ESP8266-arduino-demos
 *  Use the librairies
 *  - OneWire
 *  - Timer in user_interface
 *  
 *  Comments:
 *  - Fan is ON with 1
 *  - Led are ON with 0
 *  
 *  Serial Link commandes is composed by one letter and the value : YXXXXXXX
 *  Commande detail :
 *  - A > change winter delta ON
 *  - C > change winter delta OFF
 *  - M > change summer/winter mode
 *  - P > Change the wifi pasword
 *  - S > Change the SSID
 *  - H > Change thingspeak channel
 *  - R > Reset values
 *  - W > Write parameters in eeprom
 *  
 *  
 *  TODO
 *  - Use a table for thingspeak inputs
 *  - Detect no wifi config
 *  
 */

#include "hardware_def.h"
#include "airKit.h"

#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

extern "C" {
#include "user_interface.h"
}

#define DEBUG 1 //Use DEBUG to reduce the time 1 minute = 5s

//Serial Config
#define BAUDRATE 115200  // Serial link speed

//Timer elements
os_timer_t myTimer1;
os_timer_t myTimer2;
os_timer_t myTimer3;
// boolean to activate timer events
bool tick1Occured;
bool tick2Occured;
bool tick3Occured;
// Need to have a global address for the timer id
const char tickId1=1;
const char tickId2=2;
const char tickId3=3;
// Functions declaration
void timersSetup(void);
void timerCallback(void *);
void timerInit(os_timer_t *pTimerPointer, uint32_t milliSecondsValue, os_timer_func_t *pFunction, char *pId);

//Wifi config
//Function declarations
void wifiConnect(void);
//Wifi Global variables
WiFiClient client;
#define MAX_CONNEXION_TRY 50

//Function declarations
void thingSpeakWrite (String, unsigned long, float, float, float, float, float, float, float);

//Sensors config
//OneWire configuration for inside temp sensor
OneWire oneWireIn(TEMP_IN);                     // Setup a oneWire instance on pin 2
DallasTemperature sensorsIn(&oneWireIn);  // Pass our oneWire reference to Dallas Temperature.
DeviceAddress insideThermometer;          // arrays to hold device address
//OneWire configuration for outside temp sensor
OneWire oneWireOut(TEMP_OUT);                     // Setup a oneWire instance on pin 4
DallasTemperature sensorsOut(&oneWireOut); // Pass our oneWire reference to Dallas Temperature.
DeviceAddress outsideThermometer;          // arrays to hold device address

//Application config
AirKitConfig conf;
unsigned long getMacAddress(void);
void ioInits(void);
void printInfo(void);
void measureAction(void);

//Application values
float tempIn = 20;   //Inside room temperature value
float tempOut = 30;  //Outside room temperature value
float tempDelta = 0; //Temperature delta between inside and outside (tempOut - tempIn)
//bool gateState = 0;  //State of the gate, 0 = close 1 = open
bool fanState = 0;   //State of the fan, 0 = stopped 1 = running

//EEPROM Function declarations
void eepromWrite(void);
void eepromRead(void);

// Serial string management variables
void executeCommand();
void serialStack();
String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete


//Web parameters Function declarations
//void checkWebValues(void);
//void getConfFromWeb(void);
//Web parameters Variables
//byte webDayStart = 0;       // Hour of the day that the lamp starts
//byte webDayTime = 0;        // Number of hours of daylight (LAMP ON)
//byte webPumpFreq = 0;       // Time between 2 pump cycles
//byte webFloodedTime = 0;    // Time of water at high level

/* void setup(void) 
 *  Setup software run only once
 *  
*/
void setup() {
  
  //Start Serial
  Serial.begin(BAUDRATE);
  Serial.println("");
  Serial.println("--------------------------");
  Serial.println("    ESP8266 Full Test     ");
  Serial.println("--------------------------");
 
  timersSetup();
  ioInits();

  conf.mac = getMacAddress();
  eepromRead();

  digitalWrite(FAN, LOW);

  Serial.println("------");
  
  //Init wifi and web server
  wifiConnect();

  // OneWire temperature setup
  sensorsIn.begin();    // Start Sensor libraries  
  sensorsOut.begin();    // Start Sensor libraries  
  if (!sensorsIn.getAddress(insideThermometer, 0))   Serial.println("Unable to find address for Device 0"); 
  if (!sensorsOut.getAddress(outsideThermometer, 0)) Serial.println("Unable to find address for Device 0"); 
  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensorsIn.setResolution(insideThermometer, 12);
  sensorsOut.setResolution(insideThermometer, 12);
  
}

/* void loop(void) 
 *  Main program automatically loaded
 *  
*/
void loop() {
  
  //Check if a timer occured to execute the action
  //Timer 1 action every seconds
  if (tick1Occured == true){
    tick1Occured = false;
    //Toggle LED
    digitalWrite(RED_LED, !digitalRead(RED_LED));
  }
  
  //Check for serial input
  serialStack();
  //Execute recived command
  executeCommand();
  
  //Timer 2 action every 30s
  if (tick2Occured == true){
    tick2Occured = false;

    //check the config values on web
    //checkWebValues();
    
    measureAction();
    printInfo();
  }
  
  //Timer 3 action every 10mn
  if (tick3Occured == true){
    tick3Occured = false;
    thingSpeakWrite ( conf.thingspeakApi, conf.mac, tempIn, tempOut, fanState,
                      NAN, conf.Smode, conf.deltaON, conf.deltaOFF);
  }
 
  //Give the time to th os to do his things
  yield();  // or delay(0);
}

/* void timerSetup(void *pArg)
 *  Setup all timers 
 *  
 *  Input  : 
 *  Output :
*/
void timersSetup(void)
{
  //Init and start timers
  tick1Occured = false;
  tick2Occured = false;
  tick3Occured = false;
 
  if(DEBUG)
  { //Reduce timing for test and debug
    timerInit(&myTimer1, 1000,  timerCallback, (char*)&tickId1);
    timerInit(&myTimer2, 1000*5,  timerCallback, (char*)&tickId2);
    timerInit(&myTimer3, 1000*60,  timerCallback, (char*)&tickId3);
  }
  else
  { //Normal timing
    timerInit(&myTimer1, TIMER1,  timerCallback, (char*)&tickId1);
    timerInit(&myTimer2, TIMER2,  timerCallback, (char*)&tickId2);
    timerInit(&myTimer3, TIMER3,  timerCallback, (char*)&tickId3);
  }
}

/* void timerCallback(void *pArg)
 *  Function called by the os_timers at every execution
 *  Only one function is used for all timers, the timer id comm in the pArg
 *  
 *  Input  : 
 *  Output :
*/
void timerCallback(void *pArg) {
  
  char timerId = *(char*)pArg; //Value inside (*) of pArg, casted into a char pointer
  
  switch (timerId){
    case 1 :
      tick1Occured = true;
      break;
    case 2 :
      tick2Occured = true;
      break;
    case 3 :
      tick3Occured = true;
      break;
    default :
      //Nothings to do
      break;
  }
} 

/* timerInit(os_timer_t *pTimerPointer, uint32_t milliSecondsValue, os_timer_func_t *pFunction, char *pId) 
 *  Start and init all timers 
 *  
 *  Input  : 
 *  Output :
*/
void timerInit(os_timer_t *pTimerPointer, uint32_t milliSecondsValue, os_timer_func_t *pFunction, char *pId) {
   /*
    Maximum 7 timers
    os_timer_setfn - Define a function to be called when the timer fires
    void os_timer_setfn(os_timer_t *pTimer, os_timer_func_t *pFunction, void *pArg)
    
    Define the callback function that will be called when the timer reaches zero. 
    The pTimer parameters is a pointer to the timer control structure.
    The pFunction parameters is a pointer to the callback function.
    The pArg parameter is a value that will be passed into the called back function. 
    The callback function should have the signature: void (*functionName)(void *pArg)
    The pArg parameter is the value registered with the callback function.
  */
  os_timer_setfn(pTimerPointer, pFunction, pId);
  /*
    os_timer_arm -  Enable a millisecond granularity timer.
    void os_timer_arm( os_timer_t *pTimer, uint32_t milliseconds, bool repeat)
  
    Arm a timer such that is starts ticking and fires when the clock reaches zero.
    The pTimer parameter is a pointed to a timer control structure.
    The milliseconds parameter is the duration of the timer measured in milliseconds. 
    The repeat parameter is whether or not the timer will restart once it has reached zero.
  */
  os_timer_arm(pTimerPointer, milliSecondsValue, true);
} 

/* void wifiConnect(void)
 *  Try to connect to the wifi, stop after a time without success
 *  
 *  Input  : None
 *  Output : None
*/
void wifiConnect(void){
  
  int wifiErrorCount;
  
  // Connect to WiFi network
  Serial.println();
  Serial.println("Connecting to ");
  Serial.println(conf.ssid);
  Serial.println("Connecting to ");
  Serial.println(conf.password);

  WiFi.begin(conf.ssid, conf.password);
  
  wifiErrorCount = 0;
  while (WiFi.status() != WL_CONNECTED and wifiErrorCount < MAX_CONNEXION_TRY )
  {
    delay(500);
    Serial.print(".");
    wifiErrorCount++;
  }
  Serial.println("");
  if (wifiErrorCount < MAX_CONNEXION_TRY)
  {
    Serial.println("WiFi connected");
  }
  else
  {
    Serial.println("WiFi not connected");
  }
}

/* void thingSpeakWrite(void)
 *  Write data to thingspeak server 
 *  TODO Use a table as input
 *  
 *  Input  : APIKey - the write api key from the channel
 *           fieldX - every channel values or NAN if not used
 *  Output :
*/
void thingSpeakWrite (String APIKey,
                      unsigned long field1, float field2, float field3, float field4,
                      float field5, float field6, float field7, float field8)
{
  
  const char* thingspeakServer = "api.thingspeak.com";

  if (client.connect(thingspeakServer,80)) {  //   "184.106.153.149" or api.thingspeak.com
    String postStr = APIKey;
    if (!isnan(field1))
    {
      postStr +="&field1=";
      postStr += String(field1);
    }
    if (!isnan(field2))
    {
      postStr +="&field2=";
      postStr += String(field2);
    }
    if (!isnan(field3))
    {
      postStr +="&field3=";
      postStr += String(field3);
    }
    if (!isnan(field4))
    {
      postStr +="&field4=";
      postStr += String(field4);
    }
    if (!isnan(field5))
    {
      postStr +="&field5=";
      postStr += String(field5);
    }
    if (!isnan(field6))
    {
      postStr +="&field6=";
      postStr += String(field6);
    }
    if (!isnan(field7))
    {
      postStr +="&field7=";
      postStr += String(field7);
    }
    if (!isnan(field8))
    {
      postStr +="&field8=";
      postStr += String(field8);
    }
    postStr += "\r\n\r\n";
 
     client.print("POST /update HTTP/1.1\n");
     client.print("Host: api.thingspeak.com\n");
     client.print("Connection: close\n");
     client.print("X-THINGSPEAKAPIKEY: "+String(conf.thingspeakApi)+"\n");
     client.print("Content-Type: application/x-www-form-urlencoded\n");
     client.print("Content-Length: ");
     client.print(postStr.length());
     client.print("\n\n");
     client.print(postStr);
 
  }
  client.stop();
}

/* void printInfo(void)
 *  Print all application info on the serial link 
 *  
 *  Input  : 
 *  Output :
*/
void printInfo(void)
{

    Serial.print("--->>>MAC : ");
    Serial.println(String(conf.mac,HEX));
    
    Serial.print("Activate if delta > to : ");
    Serial.print(conf.deltaON);
    Serial.print(". Disable if delta > to : ");
    Serial.print(conf.deltaOFF);
    Serial.print(". Mode: ");
    Serial.println(conf.Smode);

    Serial.print("Values : M");
    Serial.print(conf.Smode);
    Serial.print(";I");
    Serial.print(tempIn);
    Serial.print(";O");
    Serial.print(tempOut);
    Serial.print(";D");
    Serial.print(tempDelta);
    Serial.print(";F");
    Serial.print(fanState);
    Serial.println(";");
}

/* void ioInits(void)
 *  Init all Inputs and Outputs for the application 
 *  
 *  Input  : 
 *  Output :
*/
void ioInits(void)
{
  //Init leds
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(BLUE_LED, HIGH);
  digitalWrite(RED_LED, HIGH);
  
  // Outputs for relays
  pinMode(FAN, OUTPUT);
  digitalWrite(FAN, LOW);
}

/* void eepromWrite(void)
 *  Write all application parameters in eeprom 
 *  
 *  Input  : 
 *  Output :
*/
void eepromWrite(void)
{
  char letter;
  int i, addr;
  //Activate eeprom
  EEPROM.begin(512);

  Serial.println("Save application parameters in eeprom");

  // save wifi ssid in eeprom
  addr = eeAddrSSID;
  for (i = 0 ; i < eeSizeSSID ; i++)
  { 
    EEPROM.write(addr, conf.ssid[i]);
    if('\0' == conf.ssid[i])
      break;
    addr++;
  }
  // save wifi password in eeprom
  addr = eeAddrPASS;
  for (i = 0 ; i < eeSizePASS ; i++)
  {
    EEPROM.write(addr, conf.password[i]);
    if('\0' == conf.password[i])
      break;
    addr++;
  }
  // save thingspeak api in eeprom
  addr = eeAddrTSAPI;
  for (i = 0 ; i < eeSizeTSAPI ; i++)
  {
    EEPROM.write(addr, conf.thingspeakApi[i]);
    if('\0' == conf.thingspeakApi[i])
      break;
    addr++;
  }

  EEPROM.write(eeAddrDeltaON,  conf.deltaON);
  EEPROM.write(eeAddrDeltaOFF, conf.deltaOFF);
  EEPROM.write(eeAddrSmode,    conf.Smode);
  
  EEPROM.end();
}

/* void eepromRead(void)
 *  Read all application parameters from eeprom 
 *  
 *  Input  : 
 *  Output :
*/
void eepromRead(void)
{
  char letter;
  int i, addr;
  //Activate eeprom
  EEPROM.begin(512);

  // Read string values from eeprom
  // Get wifi SSID from eeprom
  addr = eeAddrSSID;
  for (i = 0 ; i < eeSizeSSID ; i++)
  { 
    conf.ssid[i] = EEPROM.read(addr);
    if('\0' == conf.ssid[i])
      break;
    addr++;
  }
  
  // Get wifi PASSWORD from eeprom
  addr = eeAddrPASS;
  for (i = 0 ; i < eeSizePASS ; i++)
  {
    conf.password[i] = EEPROM.read(addr);
    if('\0' == conf.password[i])
      break;
    addr++;
  }
  
  // Get thingspeak api from eeprom
  addr = eeAddrTSAPI;
  for (i = 0 ; i < eeSizeTSAPI ; i++)
  {
    conf.thingspeakApi[i] = EEPROM.read(addr);
    if('\0' == conf.thingspeakApi[i])
      break;
    addr++;
  }

  // Read Bytes values from eeprom
  conf.deltaON    = EEPROM.read(eeAddrDeltaON);
  conf.deltaOFF   = EEPROM.read(eeAddrDeltaOFF); 
  conf.Smode      = EEPROM.read(eeAddrSmode);

  Serial.println("Application parameters read from eeprom");
  printInfo();
}

/* void serialStack(void)
 *  Stack all serial char received in one string until a '\n'
 *  Set stringComplete to True when '\n' is received
 *  This implementation is good enough for this project because serial commands
 *  will be send slowly.
*/
void serialStack()
{
  if(Serial.available())
  {
    while (Serial.available()) {
      // get the new byte:
      char inChar = (char)Serial.read();
      
      // if the incoming character is a newline, set a flag
      if (inChar == '\n' or inChar == '\r') 
      {// so the main loop can do something about it
        inputString += '\0';
        stringComplete = true;
      }
      else
      {// add it to the inputString
        inputString += inChar;
      }
    }
  }
}

/* void executeCommand(void)
 *  Execute received serial command
 *  Commandes list :
 *  - A > change delta ON
 *  - C > change delta OFF
 *  - M > change summer/winter mode
 *  - P > Change the wifi pasword
 *  - S > Change the SSID
 *  - H > Change thingspeak channel
 *  - R > Reset values
 *  - W > Write parameters in eeprom
 *
*/
void executeCommand()
{
  if (stringComplete)
  {
    // Define the appropriate action depending on the first character of the string
    switch (inputString[0]) 
    {
      // INFO Request
      case 'i':
      case 'I':
        printInfo(); // Print on serial the system info
        break;
      // A > change delta ON
      case 'a': case 'A':
        inputString.remove(0,1);
        Serial.print("A > change delta ON to : ");
        Serial.println(inputString);
        conf.deltaON = byte(inputString.toInt());
        break;
      // C > change delta OFF
      case 'b': case 'B':
        inputString.remove(0,1);
        Serial.print("C > change delta OFF to :");
        Serial.println(inputString);
        conf.deltaOFF = byte(inputString.toInt());
        break;
      // M > change summer/winter mode
      case 'm': case 'M':
        inputString.remove(0,1);
        Serial.print("M > change summer/winter mode to : ");
        Serial.println(inputString);
        conf.Smode = bool(inputString.toInt());
        break;
      // PASSWORD
      case 'P': case 'p':
        inputString.remove(0,1);
        Serial.print("P > Change the wifi pasword to : ");
        Serial.println(inputString);
        strcpy (conf.password, inputString.c_str());
        break;
      // SSID
      case 'S': case 's':
        inputString.remove(0,1);
        Serial.print("S > Change the SSID to : ");
        Serial.println(inputString);
        strcpy (conf.ssid, inputString.c_str());
        break;
      // Thingspeak channel
      case 'H': case 'h':
        inputString.remove(0,1);
        Serial.print("H > Change the thingspeak write api to : ");
        Serial.println(inputString);
        strcpy (conf.thingspeakApi, inputString.c_str());
        break;
      // Write command
      case 'W': case 'w':
        inputString.remove(0,1);
        Serial.println("W > Write parameters in eeprom");
        eepromWrite();
        break;
      // Reset default timing config
      case 'R': case 'r':
        Serial.println("T > Reset parameters");
        conf.deltaON    = FAN_ON;
        conf.deltaOFF   = FAN_OFF;
        conf.Smode      = MODE;
        break;
      // Reset Wifi Credential
      // Reboot
      
      // Ignore (in case of /r/n)
      case '\r': case '\n': case '\0':
        break;
      
      // Unknown command 
      default: 
        Serial.print(inputString[0]);
        Serial.println(" > ?");
    }
    inputString = "";
    stringComplete = false;
  }
}

/* void executeCommand(void)
 *  Get the board mac address
 * 
 *  Input  : 
 *  Output : result mac address as int for id use
*/

unsigned long getMacAddress() {
  byte mac[6];
  unsigned long intMac = 0;
  unsigned long intMacConv = 0;

  //TODO see for full mac (6 numbers not 4)
  
  WiFi.macAddress(mac);
  for (int i = 0; i < 4; ++i) 
  {
    intMacConv = mac[i];
    intMac = intMac + (intMacConv <<(8*i));
    //Serial.println(String(i));
    //Serial.println(String(intMacConv<<(8*i),HEX));
    //Serial.println(String(intMac, HEX));
  }
  return intMac;
}

/* void getConfFromWeb(void) 
 *  Download config file from webserver
 *  
 *  TODO: improve the seach pattern
 *  TODO Make it not stoping the app (dedicated timer app?)
*/
/*
void getConfFromWeb(void) {
  const char* host = "nebulair.co";
  String parametersString;
  char parameters[32];
  //Serial.print("connecting to ");
  //Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.print("connecting to ");
    Serial.print(host);
    Serial.println(" failed");
    webDayStart    = 0;   
    webDayTime     = 0; 
    webPumpFreq    = 0;  
    webFloodedTime = 0;
    return;
  }
  
  // We now create a URI for the request
  String url = "/config-files/conf.310366044.txt";
  //Serial.print("Requesting URL: ");
  //Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
  delay(500);
  
  // Read all the lines of the reply from server and print them to Serial
  while(client.available()){
    String line = client.readStringUntil('\r');
      if (-1 != line.indexOf('>')){
        parametersString = line.substring(line.indexOf('>')+1, line.indexOf('<'));
        parametersString.toCharArray(parameters, parametersString.length()+1);
        //Convert values
        webDayStart    = atoi(strtok(parameters, ";"));   
        webDayTime     = atoi(strtok(NULL, ";")); 
        webPumpFreq    = atoi(strtok(NULL, ";"));  
        webFloodedTime = atoi(strtok(NULL, ";"));
         
        //Serial.println(parameters);
        //Serial.println(webDayStart);
        //Serial.println(webDayTime);
        //Serial.println(webPumpFreq);
        //Serial.println(webFloodedTime);
        //Stop looking
        break;
      }
  }
  
  //Serial.println();
  //Serial.println("closing connection");
}
*/

/* void checkWebValues(void)
 *  Chack and update config from web
 * 
 *  TODO Make it not stoping the app
*/
/*
void checkWebValues(void)
{
  getConfFromWeb();
  
  //Check if parameters need to be adjusted
  if( webDayStart != 0 & webDayTime != 0&
      webPumpFreq != 0 & webFloodedTime != 0)
  {
    //Paremeters are valid, check if they are new
    if( webDayStart != conf.dayStart| webDayTime != conf.dayTime| 
        webPumpFreq != conf.pumpFreq| webFloodedTime != conf.floodedTime) 
    {
      //New parameters, Update values
      conf.dayStart = webDayStart;
      conf.dayTime = webDayTime;
      conf.pumpFreq = webPumpFreq;
      conf.floodedTime = webFloodedTime;
      eepromWrite();
      
      Serial.println("Config Updated from web");
    }
  }
}
*/

/* void measureAction(void) is executed every 1000ms by a Timer
 *  It read the temperatures values and change the system value
 *  This is the main function
*/
void measureAction() 
{
  //Get the temperature value
  tempIn = sensorsIn.getTempC(insideThermometer); 
  tempOut = sensorsOut.getTempC(outsideThermometer); 
  
  // Calculate the temperature delta
  tempDelta = tempOut - tempIn;

  // Choose action depending of the mode
  if (conf.Smode)
  {// In summer mode the delta will be negative when interesting
      // Turn On the Fan if the outside temperature is cold enough
      if (tempDelta < -conf.deltaON)
      {
        //gateState = 1;  
        fanState = 1;
        digitalWrite(FAN, fanState);
      }
      // Turn Off the Fan if the outside temperature goes up back to a specific level
      if (tempDelta > -conf.deltaOFF)
      {
        //gateState = 0;  
        fanState = 0;
        digitalWrite(FAN, fanState);
      }
  }
  else
  {// In winter mode the delta will be positive when interesting
      // Turn On the Fan if the outside temperature is hot enough
      if (tempDelta > conf.deltaON)
      {
        //gateState = 1;  
        fanState = 1;
        digitalWrite(FAN, fanState);
      }
      // Turn Off the Fan if the outside temperature goes down back to a specific level
      if (tempDelta < conf.deltaOFF)
      {
        //gateState = 0;  
        fanState = 0;
        digitalWrite(FAN, fanState);
      }
  }
  
  // Start the new conversion
  sensorsIn.requestTemperatures();  // Send the command to get temperatures
  sensorsOut.requestTemperatures(); // Send the command to get temperatures
}
