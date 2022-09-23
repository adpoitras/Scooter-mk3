//Speedometer MK3 revision B

//libary includes
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include "VescUart.h"
#include <EEPROM.h>
#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_HX8357.h"

//port definitions
// digital outputs for spi (DC - data command tft only)
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 8

#define BLINKPIN 14 //data pin for blinker neopixels

#define HEADLIGHT 4 //headlight control pin
#define AUX_ENABLE 6 //auxillary 5v power control pin

#define THUMB_X A3 //thumbsticsk analog input pin
#define THUMB_Y A6

//module initilization
VescUart vesc;

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
// miso, mosi, clk are hardware
Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

SoftwareSerial brakeSerial(3, 5); //software serial for the brakelight nano

//blinker neopixel initilization
#define NUMPIXELS 14 //0-6 right, 7-14 left
Adafruit_NeoPixel blinkers(NUMPIXELS, BLINKPIN, NEO_RGBW + NEO_KHZ800);

//variables
//color definitions
#define BLACK 0x0000
#define WHITE 0xFFFF
#define CYAN 0x1B1B
#define ORANGE 0xE2E2
#define RED 0xE0E0
#define YELLOW 0xEFEF
#define GREEN 0x0505

//vesc ingest vars
float speedo = 0;
int speedom = 0;
int previousSpeed = 0;
float voltage = 0;
float current = 0;
float amphours = 0;
float tempMotor = 0;
int rpm = 0;

//calculation vars
float odometer = 0;
float tripOdometer = 0;
float distanceTraveled = 0;
float predictedRange = 0;
float avgPredictedRange = 0;
float efficiency = 0;
int batteryPercentage = 0;
int batteryGaugeMap = 0;
float remainingAh = 0;
float amphourCorrection = 0;
float previousPredictedRange = 0;
float avgPreviousPredictedRange = 0;

//brakelight vars
float rpms = 0;
int rpmp = 0;
int prevRpm = 0;
bool brakeToggle = false; //used to control brakelight

//headlight and blinker vars
bool headlightState = false; //used for headlight
bool rightBlinkerToggle = false;
bool leftBlinkerToggle = false;
bool blinkerSwitch = false;

//control toggles
int startup = 1;
bool toggleX = false; //used for thumbstick event handling
bool toggleY = false;
bool nightToggle = false; //night day mode toggle
bool batteryFullToggle = false;
bool refresh = true;
int16_t textColor = BLACK;
int16_t backgroundColor = WHITE;
int thumbXValue = 0;
int thumbYValue = 0;
bool modMenuToggle = false;//thumbstick modifier menu
bool tailFlash = false;

//time management vars
unsigned long thumbTimeX = 0;
unsigned long thumbTimeY = 0;
int thumbTimeElapsedX = 0;
int thumbTimeElapsedY = 0;
unsigned long currentTime = 0;
unsigned long previousTime = 0;
unsigned long blinkerTime = 0;
int blinkerTimeElapsed = 0;
int timeElapsed = 0;
float timeElapsedFloat = 0;
unsigned long textTime = 0;
int textRefreshTime = 0;
unsigned long rangeTime = 0;
int rangeRefreshTime = 0;

//eeprom vars
struct storageObject{//what gets stored in eeprom
  float odometer;
  float amphours;
  float efficiency;
};
storageObject scooterEprom = {0.0,0.0,0.0};
int eeAddress = 0;
bool eepromToggle = true;

void setup() {
  //uncomment to reset eeprom values
  //EEPROM.put(eeAddress, scooterEprom); 
  EEPROM.get(eeAddress, scooterEprom);//retrieve values
  
  Serial.begin(115200); //debug usb serial
  Serial1.begin(115200); //vesc serial connection
  vesc.setSerialPort(&Serial1);

  brakeSerial.begin(9600); //software serial for brake nano

  tft.begin();
  tft.setRotation(2);
  //tft.setTextColor(HX8357_BLACK, HX8357_WHITE);
  //tft.setTextSize(10);

  blinkers.begin();
  blinkers.clear();
  blinkers.show();

  pinMode(HEADLIGHT, OUTPUT); //headlight and auxillary power initilization
  pinMode(AUX_ENABLE, OUTPUT);//enable is pulled down
  digitalWrite(HEADLIGHT, LOW);
  digitalWrite(AUX_ENABLE, HIGH);

  tft.fillScreen(WHITE); //initial screen draw
  startupLogo(false);
  tft.setTextColor(BLACK, WHITE);
  
}

void loop() {
  previousTime = currentTime; //gets time elapsed for each loop
  currentTime = millis();
  timeElapsed = (currentTime - previousTime);
  timeElapsedFloat = float(timeElapsed);

  if (vesc.getVescValues() ) { //vesc polling
    if(startup == 1){
      startup = 0;
    }
    voltage = vesc.data.inpVoltage; //rpm/14 /5 * 8 *pi*60/63360
    speedo = vesc.data.rpm * 0.000731; //mixed belt makeup
    speedom = int(speedo); //speedometer integer
    speedom = constrain(speedom, 0, 99);
    current = vesc.data.avgInputCurrent;
    amphours = vesc.data.ampHours;
    tempMotor = vesc.data.tempMotor;
    rpms = vesc.data.rpm * 0.0714;//brake calculations
    rpm = int(rpms);
    if (rpm < 30) {
      rpm = 0;
    }
    //Serial.println(rpm);
  }
  if(startup == 1){
    return;
  }
  else if(startup == 0){
    delay(2000);
    startupLogo(true);
    speedoBackground(RED);
    rangeBackground(BLACK);
    batteryGauge(batteryGaugeMap, GREEN);
    startup = 2;
  }

  //amp/range calculations

  if(voltage > 50.0 && batteryFullToggle == false){//resets bar and range if battery is charged
    scooterEprom.amphours = 0;
    EEPROM.put(eeAddress, scooterEprom);
    batteryFullToggle = true;
    refresh = true;
  }
  
  remainingAh = 12.5 - (scooterEprom.amphours + (amphours - amphourCorrection)); //assuming usable ah is 12.5
  
  batteryPercentage = map(remainingAh*100, 0,1200,0,100);//multiply by 100 to increase precison with integer math
  batteryGaugeMap = map(remainingAh * 100, 0, 1200, 0,320);

  current = constrain(current, 0.1,100.0);
  efficiency = (1.0/current)*speedo;//in amphours per mile
  predictedRange = efficiency*remainingAh;//instantaneos prediction
  predictedRange = constrain(predictedRange, 0, 99.0);
  
  avgPredictedRange = scooterEprom.efficiency*remainingAh;
  avgPredictedRange = constrain(avgPredictedRange, 0, 99.0);
  
  rangeRefreshTime = currentTime - rangeTime;
  if(rangeRefreshTime > 10000 && speedom > 5){//averages the efficency every 10s
    scooterEprom.efficiency = (efficiency + scooterEprom.efficiency)*0.5;
    rangeTime = currentTime;
  }
  
  //odometer/trip calculations
  distanceTraveled = speedo*1.467;//ft/s
  distanceTraveled = distanceTraveled*(timeElapsedFloat*0.001);//ft
  distanceTraveled = distanceTraveled/5280; //mi
  
  tripOdometer = tripOdometer + distanceTraveled;
  scooterEprom.odometer = scooterEprom.odometer + distanceTraveled;
  if(tripOdometer < 0){
    tripOdometer = 0;
  }


  //begin text drawing
  tft.setTextColor(textColor, backgroundColor);
  
  //constand update text(every 3 seconds) or when refresh is called
  textRefreshTime = currentTime - textTime;
  if(textRefreshTime > 3000 || refresh){
    tft.setTextSize(3);  //milage readout
    tft.setCursor(5, 275);
    tft.print(tripOdometer, 2);
    tft.print("mi");
    tft.setCursor(5, 310);
    tft.print(scooterEprom.odometer, 2);

    tft.setCursor(2, 357);//voltage readout
    tft.setTextSize(4);
    tft.print(voltage, 1);
    tft.print("V");

    //tft.setCursor(160,340);//battery gauge
   // tft.setTextSize(6);
    //tft.print(batteryPercentage);
    //tft.print("%");
    
    tft.setTextSize(3); //range icons
    tft.setCursor(200, 382);
    tft.print(predictedRange, 1);
    tft.print("mi");
     if(predictedRange < 10.0 && previousPredictedRange > 10.0){
      tft.fillRect(290,382,18,24,backgroundColor);
    }
    previousPredictedRange = predictedRange;
    
    tft.setCursor(200, 330); 
    //tft.print(avgPredictedRange, 1);
    //tft.print("mi");
    //if(avgPredictedRange < 10.0 && avgPreviousPredictedRange > 10.0){
      //tft.fillRect(290,330,18,24,backgroundColor);
    //}
    avgPreviousPredictedRange = avgPredictedRange;
   //temp monitor now
   tft.print(tempMotor, 1);
   tft.print("C");

    
    if(batteryGaugeMap < 150){
      batteryGauge(batteryGaugeMap, ORANGE);
    }
    else{
      batteryGauge(batteryGaugeMap, GREEN);
    }
    
    textRefreshTime = 0;
    textTime = currentTime;
  }

  
  //speedo number draw
  if(modMenuToggle){
    modMenu();
  }
  else{
    if (speedom != previousSpeed || refresh) {
      tft.setTextSize(20);
      tft.setCursor(50, 30);
      if (speedom < 10) {
        tft.print(0);
      }
      tft.print(speedom);
      previousSpeed = speedom;
    }
  }
  //end text drawing
  if(refresh){
    refresh = false;
  }


  //eeprom handler
  if(speedom < 2 && eepromToggle == false){
    scooterEprom.amphours = scooterEprom.amphours + (amphours - amphourCorrection);
    amphourCorrection = amphours; //amphourCorrection corrects for vesc, when stored vesc amp hours need to be adjusted to zero
    EEPROM.put(eeAddress, scooterEprom);
    saveIndicator(ORANGE);
    eepromToggle = true;
  }
  else if(speedom > 2){
    saveIndicator(backgroundColor);
    eepromToggle = false;
  }



  //thumbstick event handlers
  thumbXValue = map(analogRead(THUMB_X), 50, 750, -1, 1); //mapping analog joysticks to directions
  thumbYValue = map(analogRead(THUMB_Y), 50, 750, -1, 1); //right = up = 1, left = down = -1
  //Serial.println(" ");
  //Serial.println(analogRead(THUMB_X));
  //Serial.println(thumbXValue);
  //Serial.println(thumbTimeElapsedX);
  //Serial.println(" ");
  //tft.setCursor(207, 280);
  //tft.print(thumbXValue);
  
    thumbTimeElapsedY = currentTime - thumbTimeY;
    thumbTimeElapsedX = currentTime - thumbTimeX;

  if (thumbYValue == 1 && toggleY == false && thumbTimeElapsedY > 500) { //500ms deadzone(deadtime?)
    if (nightToggle == false) { //toggles day/night mode
      brakeSerial.print("nightmode");
      brakeSerial.print('\n');
      nightToggle = true;
      tft.fillScreen(BLACK);
      textColor = WHITE;
      backgroundColor = BLACK;
      rangeBackground(WHITE);
      speedoBackground(RED);
      headlightIndicator(CYAN);
      if(headlightState == false){
        updateHeadlight("onOff");
      }
      updateBlinkers("both", "white");
      refresh = true;
    }
    else {
      brakeSerial.print("daymode");
      brakeSerial.print('\n');
      nightToggle = false;
      tft.fillScreen(WHITE);
      textColor = BLACK;
      backgroundColor = WHITE;
      rangeBackground(BLACK);
      speedoBackground(RED);
      headlightIndicator(WHITE);
      updateBlinkers("both", "clear");
      updateHeadlight("onOff");
      refresh = true;
    }
    toggleY = true;
    thumbTimeY = currentTime;
    //Serial.println("headtoggle");
    
  }

  else if (thumbYValue == -1 && toggleY == false && thumbTimeElapsedY > 200) {//lighting sub menu
    toggleY = true;
    modMenuToggle = !modMenuToggle;
    tft.fillRect(30, 30, 280, 140, backgroundColor);
    refresh = true;
    thumbTimeY = currentTime;
    //Serial.println("modetoggle");
  }
  else if(thumbYValue == 0){
    thumbTimeY = currentTime;
    toggleY = false;
  }

  
  if (thumbXValue == 1 && toggleX == false && thumbTimeElapsedX > 200) {
    if(modMenuToggle){ //modifier menu for light controls
      tailFlash = !tailFlash; //toggles tailight flasher for nano
      if(tailFlash){
        brakeSerial.print("tailFlashOn");
      }
      else{
        brakeSerial.print("tailFlashOff");
      }
    }
    else{
      if (rightBlinkerToggle) { //when blinkng ends
        if (nightToggle) {
          updateBlinkers("both", "white");
        }
        else {
          updateBlinkers("both", "clear");
        }
        blinkerIndicator("right", backgroundColor);
      }
      rightBlinkerToggle = !rightBlinkerToggle;
      blinkerTime = currentTime - 600;
    }
    toggleX = true;
  }
  else if (thumbXValue == -1 && toggleX == false && thumbTimeElapsedX > 200) {
    if(modMenuToggle){
      if(headlightState){
        updateHeadlight("mode");
      }
    }
    else{
      if (leftBlinkerToggle) {
        if (nightToggle) {
          updateBlinkers("both", "white");
        }
        else {
          updateBlinkers("both", "clear");
        }
        blinkerIndicator("left", backgroundColor);
      }
      leftBlinkerToggle = !leftBlinkerToggle;
      blinkerTime = currentTime - 600;
    }
    toggleX = true;
  }
  else if (thumbXValue == 0) {
    thumbTimeX = currentTime;//prevent int overflow after 32 seconds
    toggleX = false;
  }


  //brakelight control
  rpmp = prevRpm - 20; //brake rpm hysterisis
  if (rpm < rpmp && brakeToggle == false) {//uses adjusted rpm for greater responsiveness
    brakeToggle = true;
    brakeSerial.print("brakeon");
    brakeSerial.print('\n');
  }
  else if (rpm > prevRpm && brakeToggle == true) {
    brakeToggle = false;
    brakeSerial.print("brakeoff");
    brakeSerial.print('\n');
  }
  prevRpm = rpm;


  //blinker control
  if (rightBlinkerToggle) {
    blinkerTimeElapsed = currentTime - blinkerTime;
    Serial.println(blinkerTimeElapsed);
    if (blinkerTimeElapsed > 500) { //blinking period
      blinkerTime = currentTime;
      if (blinkerSwitch) {
        updateBlinkers("right", "orange");
        blinkerIndicator("right", GREEN);
      }
      else {
        updateBlinkers("right", "clear");
        blinkerIndicator("right", backgroundColor);
      }
      blinkerSwitch = !blinkerSwitch;
    }

  }
  if (leftBlinkerToggle) {
    blinkerTimeElapsed = currentTime - blinkerTime;
    if (blinkerTimeElapsed > 500) { //blinking period
      blinkerTime = currentTime;
      if (blinkerSwitch) {
        updateBlinkers("left", "orange");
        blinkerIndicator("left", GREEN);
      }
      else {
        updateBlinkers("left", "clear");
        blinkerIndicator("left", backgroundColor);
      }
      blinkerSwitch = !blinkerSwitch;
    }
  }

}

//graphics drawing functions
void speedoBackground(int16_t accentColor) {//border surrounding speed
  tft.fillRect(0, 0, 320, 10, accentColor); //top
  tft.fillRect(0, 0, 10, 200, accentColor); //left
  tft.fillRect(310, 0, 10, 294, accentColor); //right
  tft.fillRect(0, 200, 67, 10, accentColor); //left bottom
  drawPolygon(66, 200, 157, 290, 66, 214, 157, 304, accentColor); //diagonal
  tft.fillRect(156, 294, 164, 10, accentColor); //rightbottom
  tft.fillTriangle(62, 210, 66, 210, 66, 214, accentColor);
  tft.fillTriangle(156, 290, 160, 294, 156, 294, accentColor);
}
void rangeBackground(int16_t accentColor) {
  drawPolygon(0, 210, 63, 210, 0, 218, 60, 218, accentColor); //lefttop
  drawPolygon(63, 210, 157, 304, 60, 218, 151, 310, accentColor); //topdiagonal
  drawPolygon(157, 304, 320, 304, 160, 312, 320, 312, accentColor); //righttop
  drawPolygon(122, 339, 157, 304, 130, 342, 160, 312, accentColor); //secondtopdiagonal
  drawPolygon(122, 392, 122, 339, 130, 392, 130, 342, accentColor); //centervertical
  drawPolygon(160, 422, 320, 422, 157, 430, 320, 430, accentColor); //bottomright
  drawPolygon(130, 392, 160, 422, 127, 400, 157, 430, accentColor); //bottomdiagonal
  drawPolygon(0, 392, 130, 392, 0, 400, 127, 400, accentColor); //rightbottom
  tft.fillRect(0, 339, 122, 8, accentColor); //rightsecondbottom
  tft.fillRect(0, 300, 100, 5, accentColor); //rightthindivider
  tft.fillRect(193,365,127,5,accentColor); //leftthindivider
  
  tft.setTextSize(3);//text drawing
  tft.setTextColor(textColor, backgroundColor);
  tft.setCursor(5, 237);
  tft.print("Trip");
  tft.setCursor(145, 330);
  tft.print("Tmp");
  tft.setCursor(145, 380);
  tft.print("Rng");
}

void modMenu(){
  tft.fillCircle(170,100,15,textColor);
  tft.fillTriangle(158,120,170,132,182,120,RED);
  tft.fillTriangle(138,100,150,88,150,112,textColor);
  tft.fillTriangle(190,88,202,100,190,112,textColor);
  tft.setTextSize(3);
  tft.setCursor(133,147);
  tft.print("Exit");
  tft.setCursor(57,76);
  tft.print("Beam");
  tft.setCursor(57,106);
  tft.print("mode");
  tft.setCursor(210,76);
  tft.print("Tail");
  tft.setCursor(210,106);
  tft.print("flash");
  tft.setCursor(210,136);
  if(tailFlash){
    tft.print("(on)");
  }
  else{
    tft.print("(off)");
  }
  
}

void headlightIndicator(int16_t accentColor) {
  tft.fillRect(127, 200, 40, 41, accentColor);
  tft.fillCircle(167, 220, 20, accentColor);
  tft.fillRect(127, 208, 20, 6, backgroundColor);
  tft.fillRect(127, 226, 20, 6, backgroundColor);
}

void blinkerIndicator(String side, int16_t accentColor) {
  if (side == "right") {
    tft.fillTriangle(251, 200, 286, 220, 251, 240, accentColor);
  }
  else if (side == "left") {
    tft.fillTriangle(197, 220, 231, 200, 231, 240, accentColor);
  }
  else {
    tft.fillTriangle(251, 200, 286, 220, 251, 240, accentColor);
    tft.fillTriangle(197, 220, 231, 200, 231, 240, accentColor);
  }
}

void saveIndicator(int16_t accentColor){
  tft.fillRect(160,245,35,40,accentColor);
  tft.fillTriangle(166,285,178,270,189,285,backgroundColor);
}

void batteryGauge(int state, int16_t accentColor){
  if(refresh){
    drawPolygon(320,430, 0 ,430,320,480, 0 ,480,accentColor);
    drawPolygon(127,400, 0 , 400,157,430, 0 , 430,accentColor);
  }
  drawPolygon(320,430,state,430,320,480,state,480,backgroundColor);
  if(state < 157){
    drawPolygon(127,400, state - 30 , 400,157,430, state , 430,backgroundColor);
  }
  tft.fillRect(15,425,3,55,textColor);//tick marks
  tft.fillRect(63,425,3,55,textColor);
  tft.fillRect(111,425,3,55,textColor);
  tft.fillRect(159,455,3,25,textColor);
  tft.fillRect(207,455,3,25,textColor);
  tft.fillRect(255,455,3,25,textColor);
  tft.fillRect(303,455,3,25,textColor);
}

void startupLogo(bool wipe){
  tft.fillRect(36,190,248,101,WHITE);
  if(wipe){
    return;
  }
  drawPolygon(36,290,83,190,46,290,93,190,CYAN);
  drawPolygon(56,290,103,190,66,290,113,190,CYAN);
  drawPolygon(104,290,125,244,114,290,135,244,BLACK);
  drawPolygon(125,244,125,235,135,244,137,240,BLACK);
  drawPolygon(102,235,125,235,97,244,125,244,BLACK);
  drawPolygon(113,210,125,235,118,199,137,240,BLACK);
  drawPolygon(131,190,178,290,141,190,188,290,RED);
  drawPolygon(151,190,198,290,161,190,208,290,RED);
  drawPolygon(131,290,155,239,141,290,159,250,RED);
  drawPolygon(151,290,164,261,161,290,169,271,RED);
  drawPolygon(177,190,169,207,187,190,174,218,RED);
  drawPolygon(197,190,179,229,207,190,184,239,RED);
  drawPolygon(202,239,225,190,207,250,230,199,BLACK);
  drawPolygon(225,190,268,190,230,199,262,199,BLACK);
  drawPolygon(268,190,276,208,262,199,266,208,BLACK);
  drawPolygon(253,235,266,208,259,244,276,208,BLACK);
  drawPolygon(224,235,253,235,230,244,259,244,BLACK);
  drawPolygon(212,261,224,235,217,272,230,244,BLACK);
}

//function that enables drawing quadrilaterals by drawing two adjacent triangles
void drawPolygon(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, int16_t accentColor) {
  tft.fillTriangle(x1, y1, x3, y3, x4, y4, accentColor);
  tft.fillTriangle(x1, y1, x2, y2, x4, y4, accentColor);
}

void updateBlinkers(String side, String command) {
  int i = 0;
  int j = 0;
  int a = 0;//workaround for dealing with neopixel colors
  int b = 0;
  int c = 0;
  int d = 0;
  if (side == "right") {
    i = 0;
    j = 7;
  }
  else if (side == "left") {
    i = 7;
    j = 14;
  }
  else if (side == "both") {
    i = 0;
    j = 14;
  }
  if (command == "white") {
    d = 200;
  }
  else if (command == "orange") {
    a = 80;
    b = 255;
  }

  for (i; i < j; i++) { // For each pixel...
    blinkers.setPixelColor(i, blinkers.Color(a, b, c, d));
  }
  blinkers.show();
}

void updateHeadlight(String command) {
  if (command == "onOff") { //uses delays because it doesn't needtime
    digitalWrite(HEADLIGHT, HIGH);
    delay(1000);
    digitalWrite(HEADLIGHT, LOW);
    headlightState = !headlightState; //toggle boolean
  }
  if (command == "mode") {
    digitalWrite(HEADLIGHT, HIGH);
    delay(200);
    digitalWrite(HEADLIGHT, LOW);
  }
}  
