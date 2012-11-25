#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

////////////////// Hardware Settings ///////////////////////////////////////////////////////////////////////
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



void setup(){

  Serial.begin(9600);   // Slow to make sure the connection is stable
  Serial.println("Serial communication established now setting up");

}

void loop(){
  for (int i=0; i <= 255; i++){
    analogWrite(PWMpin, i);
    delay(10);
  } 
}

