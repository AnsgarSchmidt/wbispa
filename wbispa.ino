#include <OneWire.h>
#include <DallasTemperature.h>
#include <Time.h>
#include <CmdMessenger.h>
#include <Base64.h>
#include <Streaming.h>
#include <avr/wdt.h>
#include <EEPROM.h>

////////////////// Hardware Settings //////////////////////////////////
#define TEMP_SENSOR_PLATE_1 {0x10, 0xD2, 0x4B, 0x57, 0x02, 0x08, 0x00, 0xAF}
#define PI_VAL              ((3.1415926539*2)/1440)

#define COMRX_PIN                  0 // Serial Receive PREDEFINED
#define COMTX_PIN                  1 // Serial Transmit PREDEFINED
#define MOINSTURE                  2 // Moisture interrupt pin
#define FAN_1_HAL                  3 // Fan 1 interrupt for hal sensor
#define D4                         4 // 
#define FLOWER_POWER_1             5 // Activate Power for flow 1 measurement
#define FLOWER_POWER_2             6 // Activate Power for flow 2 measurement 
#define D7                         7 //
#define TEMP_DATA_PIN              8 // Temp One Wire interface
#define HEAT_PLATE_1_PWM_PIN       9 // PWM Pin Heat
#define FAN_1_PWM_PIN             10 // PWM Pin Fan
#define MOSI                      11 // Programm Mosi
#define MISO                      12 // Programm Miso
#define SCK                       13 // Programm SCK
#define SOLAR_PIN                 A0 // Solar cell light measurement
#define FLOWER_1_PIN              A1 // Flower 1 measurement
#define FLOWER_2_PIN              A2 // Flower 2 measurement
#define A3                        A3 // 
#define SDA                       A4 // TWI SDA
#define SCL                       A5 // TWI SCL

#define NUM_PLATES 1

boolean time_needed = true;

struct Timer{
  uint16_t delay;
  uint32_t lastTime;
};

Timer sampleTimer;
Timer calcTimer;
Timer fanTimer;
Timer flowerTimer;

uint16_t fanControll[60];

uint8_t  flowerCounter = 0; 
uint32_t flowerValue1  = 0;
uint32_t flowerValue2  = 0;

////////////////////////////////////////////////////////////
// ENUM
////////////////////////////////////////////////////////////
enum{
  kCOMM_ERROR    = 000,
  kACK           = 001,
  kARDUINO_READY = 002,
  kERR           = 003,
  kREQUEST_TIME  = 004,
  kTEMP          = 005,
  kPWM           = 006,
  kCONFIG        = 007,
  kTIME          = 010, //  8
  kSOLAR         = 011, //  9
  kFLOWER        = 012, // 10
  k11            = 013, // 11
  k12            = 014, // 12
  k13            = 015, // 13
  k14            = 016, // 14
  k15            = 017, // 15
  k16            = 020, // 16
  k17            = 021, // 17
  k18            = 022, // 18
  k19            = 023, // 19
  kSEND_CMDS_END,
};

messengerCallbackFunction messengerCallbacks[] = {
  set_time,   // 020
  get_time,   // 021
  get_temp,   // 022
  get_pwm,    // 023
  set_config, // 024
  get_config, // 025
  get_solar,  // 026
  get_flower, // 027
  set_fan,    // 028
  NULL
};

struct Plate{
  DeviceAddress sensor;
  uint16_t      setTemp;
  uint8_t       setPWM;
  uint16_t      currentTemp;
  uint16_t      maxTemp;
  uint16_t      minTemp;
  uint8_t       minPWM;
  uint8_t       pwmPin;
};

Plate plate[NUM_PLATES];

////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////
OneWire           oneWire(TEMP_DATA_PIN);
DallasTemperature sensors(&oneWire);
CmdMessenger      cmdMessenger = CmdMessenger(Serial, ',', ';');

void setup(){

  // Com
  Serial.begin(9600);   // Slow to make sure the connection is stable
  cmdMessenger.print_LF_CR();   // Make output more readable whilst debugging in Arduino Serial Monitor
  cmdMessenger.attach(kARDUINO_READY, arduino_ready);
  cmdMessenger.attach(unknownCmd);
  attach_callbacks(messengerCallbacks);
  arduino_ready();

  // Sensors
  // Dallas
  uint8_t s[NUM_PLATES][8] = {TEMP_SENSOR_PLATE_1  };
  uint8_t p[NUM_PLATES]    = {HEAT_PLATE_1_PWM_PIN };
  for(int i=0;i<NUM_PLATES;i++){
    for(int j=0;j<8;j++){
      plate[i].sensor[j]=s[i][j];
    }

    plate[i].pwmPin       = p[i];
    plate[i].setTemp      = 2500;
    plate[i].setPWM       = 255;
    plate[i].currentTemp  = 2500;
    plate[i].minTemp      = EEPROM.read(i*5+0) + (EEPROM.read(i*5+1) << 8);
    plate[i].maxTemp      = EEPROM.read(i*5+2) + (EEPROM.read(i*5+3) << 8);
    plate[i].minPWM       = EEPROM.read(i*5+4);
  }
  sensors.begin();
  sensors.setResolution(TEMP_12_BIT); // Global
  for(int i=0;i<NUM_PLATES;i++){
    if(checkSensor(plate[i].sensor)){
      sensors.setResolution(plate[i].sensor ,TEMP_12_BIT);
    }
  }
  sensors.setWaitForConversion(true);

  // Time
  setSyncProvider( requestSync );

  // Timers
  sampleTimer.delay    = 10000L;
  sampleTimer.lastTime = millis();
  calcTimer.delay      = 20000L;
  calcTimer.lastTime   = millis();
  fanTimer.delay       = 1000L;
  fanTimer.lastTime    = millis();
  flowerTimer.delay    = 20000L;
  flowerTimer.lastTime = millis();

  fanControll[52] = 2048; // Dummy value after reset, no need to store in eprom

  wdt_enable(WDTO_2S);

}

//////////////////////////////////////////////////////////////////////////////////
// LOOP
//////////////////////////////////////////////////////////////////////////////////
void loop(){
  wdt_reset();
  cmdMessenger.feedinSerialData();
  wdt_reset();
  samplingHandling(); 
  wdt_reset();
  calc();
  wdt_reset();
  fanHandling();
  wdt_reset(); 
  flowerHandling();
  wdt_reset();
}

///////////////////////////////////////////////////////////////
// Flower Handling
///////////////////////////////////////////////////////////////
void flowerHandling(){
  if( (millis() - flowerTimer.lastTime) > flowerTimer.delay){
    
    flowerTimer.lastTime = millis();

    uint8_t ITTERATIONS = 100;     
    digitalWrite(FLOWER_POWER_1,HIGH);
    digitalWrite(FLOWER_POWER_2,HIGH);
    wdt_reset();
    delay(250);
    wdt_reset();
    uint32_t flower1 = 0;
    uint32_t flower2 = 0;
    for(int i=0;i<ITTERATIONS;i++){
      wdt_reset();
      flower1 += analogRead(FLOWER_1_PIN);
      wdt_reset();
      flower2 += analogRead(FLOWER_2_PIN);
    }
    digitalWrite(FLOWER_POWER_1,LOW);
    digitalWrite(FLOWER_POWER_2,LOW);    
    flowerValue1 += (flower1/ITTERATIONS);
    flowerValue2 += (flower2/ITTERATIONS);
    flowerCounter++;
    
    if(flowerCounter > 200){
      flowerValue1  = 0;
      flowerValue2  = 0;
      flowerCounter = 0;
    }
  }
}

/////////////////////////////////////////////////////////////
// Fan
/////////////////////////////////////////////////////////////
void fanHandling(){
  if( (millis() - fanTimer.lastTime) > fanTimer.delay){

    fanTimer.lastTime = millis();

    if (fanControll[minute()] > 0){ // Dummy until we handling with the interrupt handling
      analogWrite(FAN_1_PWM_PIN, 255);
    }
    else{
      analogWrite(FAN_1_PWM_PIN, 0);
    }

  }
}

///////////////////////////////////////////////////////////////////////////////////////
///// Sampler
///////////////////////////////////////////////////////////////////////////////////////
void samplingHandling(){
  if( (millis() - sampleTimer.lastTime) > sampleTimer.delay){

    sampleTimer.lastTime = millis();

    for(int i = 0; i < NUM_PLATES; i++){ 
      uint16_t newTemp = getTemperature(plate[i].sensor);
      if(newTemp > 0 ){
        plate[i].currentTemp = newTemp;
      }
      if(plate[i].currentTemp < plate[i].setTemp){
        int newPWM = 255 - (plate[i].setTemp - plate[i].currentTemp);
        if(newPWM < plate[i].minPWM){
          plate[i].setPWM = plate[i].minPWM;
        }else{
          plate[i].setPWM = newPWM;
        }
      }
      else{
        plate[i].setPWM = 255;
      }
      analogWrite(plate[i].pwmPin, plate[i].setPWM);
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
//// Calc
/////////////////////////////////////////////////////////////////////////////////////////
void calc(){
  if( (millis() - calcTimer.lastTime) > calcTimer.delay){

    calcTimer.lastTime = millis();

    for(int i = 0; i < NUM_PLATES; i++){
      if(timeStatus() != timeNotSet){
        time_t    t  = now();
        uint16_t  m  = hour(t) * 60 + minute(t);
        float     c = (1 - cos(PI_VAL*m)) * 0.5;
        uint16_t  s  = (plate[i].maxTemp - plate[i].minTemp) * c;
        plate[i].setTemp = plate[i].minTemp+s;
      }  
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////
////////////////////////// Sensor Handling ////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
boolean checkSensor(DeviceAddress insensor){
  if(!sensors.validAddress(insensor)){
    return false;
  }
  if(!sensors.isConnected(insensor)){
    return false;
  }
  return true;
}

uint16_t getTemperature(DeviceAddress insensor){
  if(checkSensor(insensor)){
    sensors.requestTemperatures();
    float temp = sensors.getTempC(insensor);
    if(temp < 0){
      return DEVICE_DISCONNECTED;
    }
    return  sensors.getTempC(insensor) * 100;
  }
  else{
    return DEVICE_DISCONNECTED;
  }
}

time_t requestSync(){
  //cmdMessenger.sendCmd(kREQUEST_TIME,"request time");
  time_needed = true;
  return 0;
}

//////////////////////////////////////
// Callback
//////////////////////////////////////
void set_time(){
  while ( cmdMessenger.available() ){
    char buf[350] = { 
      '\0'                     };
    cmdMessenger.copyString(buf, 350);
    if(buf[0]){ 
      cmdMessenger.sendCmd(kACK, buf);
      time_t pctime = 0;
      char c;
      for(int i=0; i < 10; i++){   
        c = buf[i];          
        if( c >= '0' && c <= '9'){   
          pctime = (10 * pctime) + (c - '0') ; // convert digits to a number    
        }
      }   
      setTime(pctime); 
      cmdMessenger.sendCmd(kACK,"time_setting cmd recieved");
      time_needed = false;
    }
  }
}

void get_time(){
  char buf[100];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", year(),month(),day(),hour(),minute(),second());
  cmdMessenger.sendCmd(kTIME,buf);
}

void get_solar(){
  char buf[100];
  sprintf(buf, "%04d,", analogRead(SOLAR_PIN));
  cmdMessenger.sendCmd(kSOLAR,buf);
  if(time_needed){
    cmdMessenger.sendCmd(kREQUEST_TIME,"request time");
    time_needed = false;
  }
}

void get_flower(){
  char buf[100];

  if(flowerCounter > 0){
    uint16_t flower1 = (flowerValue1/flowerCounter);
    uint16_t flower2 = (flowerValue2/flowerCounter);
    sprintf(buf, "%04d,%04d,", flower1,flower2);
  }else{
    sprintf(buf, "%04d,%04d,", 0,0);
  }
  
  flowerValue1=0;
  flowerValue2=0;
  flowerCounter=0;
  cmdMessenger.sendCmd(kFLOWER,buf);
  
  if(time_needed){
    cmdMessenger.sendCmd(kREQUEST_TIME,"request time");
    time_needed = false;
  }  
}

void get_temp(){
  uint8_t offset = 0;
  char buf[100];
  sprintf(buf+offset, "%02d,",NUM_PLATES);
  offset += 3;
  for(int i = 0; i < NUM_PLATES; i++){
    sprintf(buf+offset, "%04d,%04d,",plate[i].setTemp,plate[i].currentTemp);
    offset += 10;
  }
  cmdMessenger.sendCmd(kTEMP,buf);
  if(time_needed){
    cmdMessenger.sendCmd(kREQUEST_TIME,"request time");
    time_needed = false;
  }
}

void get_pwm(){
  uint8_t offset = 0;
  char buf[100];
  sprintf(buf+offset, "%02d,",NUM_PLATES);
  offset += 3;
  for(int i = 0; i< NUM_PLATES; i++){
    sprintf(buf+offset, "%04d,",plate[i].setPWM);
    offset += 5;
  }
  cmdMessenger.sendCmd(kPWM,buf);
  if(time_needed){
    cmdMessenger.sendCmd(kREQUEST_TIME,"request time");
    time_needed = false;
  }
}

void get_config(){
  char buf[100];
  sprintf(buf, "%04d,%04d,%04d,",plate[0].maxTemp,plate[0].minTemp,plate[0].minPWM);
  cmdMessenger.sendCmd(kCONFIG,buf);
  if(time_needed){
    cmdMessenger.sendCmd(kREQUEST_TIME,"request time");
    time_needed = false;
  }
}

void set_config(){
  while ( cmdMessenger.available() ){
    char buf[350] = {'\0'};
    cmdMessenger.copyString(buf, 350);
    if(buf[0]){ 
      uint16_t v;
      char     c;
      v=0;
      for(int i=0; i < 4; i++){   
        c = buf[i];          
        if( c >= '0' && c <= '9'){   
          v = (10 * v) + (c - '0');
        }
      }   
      plate[0].maxTemp=v;
      v=0;
      for(int i=4; i < 8; i++){   
        c = buf[i];          
        if( c >= '0' && c <= '9'){   
          v = (10 * v) + (c - '0');
        }
      }   
      plate[0].minTemp=v;
      v=0;
      for(int i=8; i < 12; i++){   
        c = buf[i];          
        if( c >= '0' && c <= '9'){   
          v = (10 * v) + (c - '0');
        }
      }   
      plate[0].minPWM=v;
      cmdMessenger.sendCmd(kACK,"config set");
      for(int i=0;i<NUM_PLATES;i++){
        EEPROM.write(i*5+0,lowByte(plate[i].minTemp));
        EEPROM.write(i*5+1,highByte(plate[i].minTemp));
        EEPROM.write(i*5+2,lowByte(plate[i].maxTemp));
        EEPROM.write(i*5+3,highByte(plate[i].maxTemp));
        EEPROM.write(i*5+4,plate[i].minPWM);
      }
    }
  }
  if(time_needed){
    cmdMessenger.sendCmd(kREQUEST_TIME,"request time");
    time_needed = false;
  }
  
}

void set_fan(){
  cmdMessenger.sendCmd(kACK,"fan set");
  if(time_needed){
    cmdMessenger.sendCmd(kREQUEST_TIME,"request time");
    time_needed = false;
  }
}

void arduino_ready(){
  cmdMessenger.sendCmd(kACK,"WBISpa ready");
}

void unknownCmd(){
  cmdMessenger.sendCmd(kERR,"Unknown command");
}

void attach_callbacks(messengerCallbackFunction* callbacks){
  int i = 0;
  int offset = kSEND_CMDS_END;
  while(callbacks[i]){
    cmdMessenger.attach(offset+i, callbacks[i]);
    i++;
  }
}

