




struct AirKitConfig {
  unsigned long mac;
  char ssid[32];
  char password[32];
  char thingspeakApi[32];
  byte deltaON;       // Minimum delta to activate the air flow
  byte deltaOFF;      // Maximum delta to disable the air flow
  bool Smode;         // Winter = 0
};

//Timer config
#define TIMER1 1000       //  1s for timer 1
#define TIMER2 1000*30    // 30s for timer 2
#define TIMER3 1000*60*5  // 5mn for timer 3

//EEPROM config
#define eeAddrSSID          0     // address in the EEPROM  for SSID
#define eeSizeSSID         32     // size in the EEPROM  for SSID
#define eeAddrPASS         32     // address in the EEPROM  for SSID
#define eeSizePASS         32     // size in the EEPROM  for SSID
#define eeAddrTSAPI        64     // address in the EEPROM  for SSID
#define eeSizeTSAPI        32     // size in the EEPROM  for SSID
#define eeAddrDeltaON      510    
#define eeAddrDeltaOFF     511
#define eeAddrSmode        512   

enum waterLevel {
  DOWN = 0,
  FILLING = 1,
  UP = 2,
  CLEARING = 3
};

// Default time definition
#define FAN_ON      4
#define FAN_OFF     2
#define MODE        0


