//code for tailight arduino nano
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include <Adafruit_Pixie.h>

#define RIGHT 8
#define LEFT 12
#define FLASHER 10

bool nightMode = false;
bool tailFlash = false;
String command = "";

SoftwareSerial pixieSerial(-1, FLASHER);

Adafruit_NeoPixel rightTail(8, RIGHT, NEO_GBR + NEO_KHZ800);
Adafruit_NeoPixel leftTail(8, LEFT, NEO_GBR + NEO_KHZ800);
Adafruit_Pixie centerTail = Adafruit_Pixie(1, &pixieSerial);

void setup() {
  Serial.begin(9600);
  pixieSerial.begin(115200);
  rightTail.begin();
  leftTail.begin();
  sidesOn(0);
  centerTail.clear();
  centerTail.show();
  for(int i=0; i<8; i++) { // For each pixel...
    //rightTail.setPixelColor(i, rightTail.Color(0, 0, 255));
  }
  for(int i=0; i<8; i++) { // For each pixel...
    //leftTail.setPixelColor(i, leftTail.Color(0, 0, 255));
  }
  //centerTail.setPixelColor(0,255,0,0);

}

void loop() {
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    command = data;
    Serial.println(command);
    if(command == "nightmode"){
      Serial.println("nighttoggle");
      nightMode = true;
      sidesOn(80);
    }
    else if(command == "daymode"){
      nightMode = false;
      sidesOn(0);
    }
    else if(command == "tailFlashOn"){
      tailFlash = true;
    }
    else if(command == "tailFlashOff"){
      tailFlash = false;
    }
    else if(command == "brakeon"){
      sidesOn(255);
      if(tailFlash){
        for(int i=0; i<7; i++){
        centerTail.setPixelColor(0,255,0,0);
        centerTail.show();
        delay(60);
        centerTail.clear();
        centerTail.show();
        delay(60);
        }
      }
      else{
        centerTail.setPixelColor(0,90,0,0);
        centerTail.show();
      }
      
      //centerTail.setPixelColor(0,255,0,0);
      //centerTail.show();
    }
    else if(command == "brakeoff"){
      centerTail.clear();
      centerTail.show();
      if(nightMode == true){
        sidesOn(80);
      }
      else{
        sidesOn(0);
      }
    }
  }
  


}

void sidesOn(int brightness){
  for(int i=0; i<8; i++) { // For each pixel...
    rightTail.setPixelColor(i, rightTail.Color(0, 0, brightness));
  }
  for(int i=0; i<8; i++) { // For each pixel...
    leftTail.setPixelColor(i, leftTail.Color(0, 0, brightness));
  }
  rightTail.show();
  leftTail.show();
}
