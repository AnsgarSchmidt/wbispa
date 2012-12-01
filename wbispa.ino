#include <OneWire.h>
#include <DallasTemperature.h>
#include <Time.h>
#include <CmdMessenger.h>
#include <Base64.h>
#include <Streaming.h>

////////////////// Hardware Settings //////////////////////////////////
// Dallas
DeviceAddress TEMP_INDOOR = {0x10, 0xD2, 0x4B, 0x57, 0x02, 0x08, 0x00, 0xAF};

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

////////////////////////////////////////////////////////////
// ENUM
////////////////////////////////////////////////////////////
enum{
   kCOMM_ERROR    = 000,
   kACK           = 001,
   kARDUINO_READY = 002,
   kERR           = 003,
   kREQUEST_TIME  = 004,
   k5             = 005,
   k6             = 006,
   k7             = 007,
   k8             = 010,
   k9             = 011,
   kSEND_CMDS_END,
};

messengerCallbackFunction messengerCallbacks[] = {
   set_time,   // 010
   get_time,   // 011
   get_temp,   // 012
   NULL
};

float setTemp     = 25;
float setHeat     =  0;
float currentTemp = 25;
int   loopCount   =  0;

char  field_separator   = ',';
char  command_separator = ';';

OneWire oneWire(TEMP_DATA_PIN);
DallasTemperature sensors(&oneWire);
CmdMessenger cmdMessenger = CmdMessenger(Serial, field_separator, command_separator);

////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////
void setup(){

  Serial.begin(9600);   // Slow to make sure the connection is stable
  cmdMessenger.print_LF_CR();   // Make output more readable whilst debugging in Arduino Serial Monitor
  cmdMessenger.attach(kARDUINO_READY, arduino_ready);
  cmdMessenger.attach(unknownCmd);
  attach_callbacks(messengerCallbacks);
  arduino_ready();

  sensors.begin();
  sensors.setResolution(TEMP_12_BIT); // Global
  if(checkSensor(TEMP_INDOOR)){
     sensors.setResolution(TEMP_INDOOR ,TEMP_12_BIT);
  }
  
  setSyncProvider( requestSync );
}

//////////////////////////////////////////////////////////////////////////////////
// LOOP
//////////////////////////////////////////////////////////////////////////////////
void loop(){

  cmdMessenger.feedinSerialData();
  
  currentTemp = getTemperature(TEMP_INDOOR);   

  if(currentTemp < setTemp){
    setHeat = ((setTemp-currentTemp)*25)+50;
    if(setHeat>255){
      setHeat=255;
    }
  }else{
    setHeat = 0;
  }
  
  analogWrite(HEAT_PLATE_1_PWM_PIN, 255-setHeat);
  
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

float getTemperature(DeviceAddress insensor){
  if(checkSensor(insensor)){
    sensors.requestTemperatures();
    float temp = sensors.getTempC(insensor);
    return temp;
  }else{
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
    char buf[350] = { '\0' };
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
   cmdMessenger.sendCmd(kACK,buf);
}

void get_temp(){
   char buf[100];
   int t = currentTemp * 100;
   sprintf(buf, "Temp:%04d\0",t);
   cmdMessenger.sendCmd(kACK,buf);
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

