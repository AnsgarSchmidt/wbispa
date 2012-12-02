#include <OneWire.h>
#include <DallasTemperature.h>
#include <Time.h>
#include <CmdMessenger.h>
#include <Base64.h>
#include <Streaming.h>

////////////////// Hardware Settings //////////////////////////////////
#define TEMP_SENSOR_PLATE_1 {0x10, 0xD2, 0x4B, 0x57, 0x02, 0x08, 0x00, 0xAF}
#define PI_VAL              ((3.1415926539*2)/1440)

// Com
#define COMRX_PIN                  0 // Serial Receive PREDEFINED
#define COMTX_PIN                  1 // Serial Transmit PREDEFINED

// Buttons
#define D02_PIN                    2 //
#define D03_PIN                    3 //

// Temp 2 TWI
#define TEMP_DATA_PIN              4 // DataPin for tempsensors

// LED and PWMs
#define HEAT_PLATE_1_PWM_PIN       5 // Heat Plate 1
#define HEAT_PLATE_2_PWM_PIN       6 // Heat Plate 2
#define HEAT_PLATE_1_INFO_PIN      7 // Info LED
#define HEAT_PLATE_2_INFO_PIN      8 // Info LED
#define TEMP_OK_PIN                9 // Temp OK
#define FAN_1_PWM_PIN             10 // Fan 1
#define FAN_2_PWM_PIN             11 // Fan 2
#define FAN_1_INFO_PIN            12 // Info LED
#define FAN_2_INFO_PIN            13 // Info LED

// Analog
#define SOLAR_PIN                 A0 // Solar cell
#define FLOWER_1_PIN              A1 // Flower 1
#define FLOWER_2_PIN              A2 // Flower 2
#define FLOWER_3_PIN              A3 // Flower 3
#define FLOWER_4_PIN              A4 // Flower 4
#define FLOWER_5_PIN              A5 // Flower 5

#define NUM_PLATES 1

struct Timer{
  uint16_t delay;
  uint32_t lastTime;
};

Timer sampleTimer;
Timer calcTimer;

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
  kTIME          = 010,
  k              = 011,
  kSEND_CMDS_END,
};

messengerCallbackFunction messengerCallbacks[] = {
  set_time,   // 010
  get_time,   // 011
  get_temp,   // 012
  get_pwm,    // 013
  set_config, // 014
  get_config, // 015
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
  uint8_t s[NUM_PLATES][8] = {
    TEMP_SENSOR_PLATE_1  };
  uint8_t p[NUM_PLATES]    = {
    HEAT_PLATE_1_PWM_PIN  };
  for(int i=0;i<NUM_PLATES;i++){
    for(int j=0;j<8;j++){
      plate[i].sensor[j]=s[i][j];
    }
    plate[i].pwmPin       = p[i];
    plate[i].setTemp      = 2500;
    plate[i].setPWM       = 255;
    plate[i].currentTemp  = 2500;
    plate[i].maxTemp      = 2500;
    plate[i].minTemp      = 2500;
    plate[i].minPWM       = 50;
  }
  sensors.begin();
  sensors.setResolution(TEMP_12_BIT); // Global
  for(int i=0;i<NUM_PLATES;i++){
    if(checkSensor(plate[i].sensor)){
      sensors.setResolution(plate[i].sensor ,TEMP_12_BIT);
    }
  }

  // Time
  setSyncProvider( requestSync );

  // Timers
  sampleTimer.delay    = 10000L;
  sampleTimer.lastTime = millis();
  calcTimer.delay      = 60000L;
  calcTimer.lastTime   = millis();

}

//////////////////////////////////////////////////////////////////////////////////
// LOOP
//////////////////////////////////////////////////////////////////////////////////
void loop(){
  cmdMessenger.feedinSerialData();
  samplingHandling(); 
  calc();
}

///////////////////////////////////////////////////////////////////////////////////////
///// Sampler
///////////////////////////////////////////////////////////////////////////////////////
void samplingHandling(){
  if( (millis() - sampleTimer.lastTime) > sampleTimer.delay){

    sampleTimer.lastTime = millis();

    for(int i = 0; i < NUM_PLATES; i++){ 
      plate[i].currentTemp = getTemperature(plate[i].sensor);   
      if(plate[i].currentTemp < plate[i].setTemp){
        plate[i].setPWM = 255 - ((plate[i].setTemp - plate[i].currentTemp) * 50);
        if(plate[i].setPWM < plate[i].minPWM){
          plate[i].setPWM = plate[i].minPWM;
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
    return  sensors.getTempC(insensor) * 100;
  }
  else{
    return DEVICE_DISCONNECTED;
  }
}

time_t requestSync(){
  cmdMessenger.sendCmd(kREQUEST_TIME,"request time");
  return 0;
}

//////////////////////////////////////
// Callback
//////////////////////////////////////
void set_time(){
  while ( cmdMessenger.available() ){
    char buf[350] = { 
      '\0'     };
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
    }
  }
}

void get_time(){
  char buf[100];
  sprintf(buf, "DateTime:%04d-%02d-%02d %02d:%02d:%02d\0", year(),month(),day(),hour(),minute(),second());
  cmdMessenger.sendCmd(kTIME,buf);
}

void get_temp(){
  uint8_t offset = 0;
  char buf[NUM_PLATES*10+8];
  sprintf(buf+offset, "Temp:%02d:",NUM_PLATES);
  offset += 8;
  for(int i = 0; i < NUM_PLATES; i++){
   sprintf(buf+offset, "%04d:%04d:",plate[i].setTemp,plate[i].currentTemp);
   offset += 10;
  }
  cmdMessenger.sendCmd(kTEMP,buf);
}

void get_pwm(){
  uint8_t offset = 0;
  char buf[NUM_PLATES*4+8];
  sprintf(buf+offset, "PWM:%02d:",NUM_PLATES);
  offset += 7;
  for(int i = 0; i< NUM_PLATES; i++){
    sprintf(buf+offset, "%04d:",plate[i].setPWM);
    offset += 5;
  }
  cmdMessenger.sendCmd(kPWM,buf);
}

void get_config(){
  char buf[100];
  sprintf(buf, "Config:%04d,%04d,%04d\0",plate[0].maxTemp,plate[0].minTemp,plate[0].minPWM);
  cmdMessenger.sendCmd(kCONFIG,buf);
}

void set_config(){
  while ( cmdMessenger.available() ){
    char buf[350] = { 
      '\0'     };
    cmdMessenger.copyString(buf, 350);
    if(buf[0]){ 
      cmdMessenger.sendCmd(kACK, buf);
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
    }
  }  
}

void arduino_ready(){
  cmdMessenger.sendCmd(kACK,"Arduino ready");
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


