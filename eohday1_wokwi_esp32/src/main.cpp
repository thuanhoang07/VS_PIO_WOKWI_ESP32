#include <AiEsp32RotaryEncoder.h>
#include <Wire.h>
#include <WiFi.h>
#include "time.h"
#include <I2C_RTC.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include "bitmap.h"
#include <Shape.hpp>
#include <cstring>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
//#include <Adafruit_SH110X.h>

String openWeatherMapApiKey = "b222c1993fa613598c88bc304b7e8828";

String lat = "10.84";
String lon = "106.76";

String jsonBuffer;

unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

String httpGETRequest(const char* serverName);

static DS1307 RTC;
const char* ssid = "eoh.io";
const char* password = "Eoh@2020";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 0;


#define ENCODER_CLK 25
#define ENCODER_DT 26
#define ENCODER_SW 27
#define ENCODER_VCC -1
#define ENCODER_STEPS 4

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2

#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16

#define i2c_Address 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
//Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
AiEsp32RotaryEncoder rotaryEncoder(ENCODER_CLK, ENCODER_DT, ENCODER_SW, ENCODER_VCC, ENCODER_STEPS);

void IRAM_ATTR readEncoderISR();
void displayMenu();
void displaySubMenu1();
void displaySubMenu2();
void rotary_loop();
void handle_rotary_button();
void onButtonShortPress();
//void onButtonLongPress();
void connectToWiFi();
void updateTimeFromNTP();
void displayTimeFromRTC();
void TaskUpdateTime();
void TaskDisplayStars();
void updateRTCFromNTP();


TaskHandle_t Task1;
TaskHandle_t Task2;

String menus[] = {"Information", "Contact", "Education", "Photo", "QR", "Night mode"};
String subMenus1[] = {"Name", "Birthday", "Countryside","Exit"};
String subMenus2[] = {"Phone Number", "Social Media", "Address", "Exit"};
String itemsubMenus1[] = {"a", "b", "Exit"};

int menuIndex = 0;
int menuIndex_subMenu = 0;
int menuIndex_itemsubMenu = 0;
int numMenus = 6;
int numsubMenu1 = 4;
int numsubMenu2 = 4;
int numitemsubMenu1 = 3;

bool inSleepMode = false;
bool inSleepModeActive = false;
bool inSubMenu1 = false;
bool inSubMenu2 = false;
bool initemSubMenu1 = false;
bool inqrcode = false;
bool inphoto = false;
bool inName = false;
bool inEducation = false;


int previousRotaryValue = 0;
int currentRotaryValue = 0;

uint16_t year;
uint8_t month;
uint8_t day;
uint8_t hour;
uint8_t minute;
uint8_t second;
uint8_t dayofweek;

bool initialUpdateDone = false;
bool halfSecondUpdateDone = false;
unsigned long initialTime = 0;

unsigned long lastUpdateTime = 0; // Biến để theo dõi thời gian cập nhật cuối cùng
const unsigned long updateInterval = 60*1000; // Khoảng thời gian giữa các lần cập nhật (5000ms = 5s)



//-----------------------------------------------------------------------------
const char STR_LOADSCREEN_APP_NAME_LINE1[] = "Flappy";
const char STR_LOADSCREEN_APP_NAME_LINE2[] = "Bird!";
const char STR_PRESS_FLAP_TO_PLAY[] = "rotary to play";
const char STR_GAME_OVER[] = "Game Over!";

// Define I/O pins
const int FLAP_BUTTON_INPUT_PIN = 4;

// for tracking fps
unsigned long _frameCount = 0;
float _fps = 0;
unsigned long _fpsStartTimeStamp = 0;

// status bar
const boolean _drawFrameCount = false; // change to show/hide frame count
const int DELAY_LOOP_MS = 5;
const int LOAD_SCREEN_SHOW_MS = 750;

class Bird : public Rectangle {
  public:
    Bird(int x, int y, int width, int height) : Rectangle(x, y, width, height)
    {
    }
};

class Pipe : public Rectangle {
  protected:
    bool _hasPassedBird = false;

  public:
    Pipe(int x, int y, int width, int height) : Rectangle(x, y, width, height)
    {
    }

    bool getHasPassedBird() {
      return _hasPassedBird;
    }

    void setHasPassedBird(bool hasPassedBird) {
      _hasPassedBird = hasPassedBird;
    }
};

const int BIRD_HEIGHT = 5;
const int BIRD_WIDTH = 10;
const int NUM_PIPES = 3;

const int MIN_PIPE_WIDTH = 8;
const int MAX_PIPE_WIDTH = 18; // in pixels
const int MIN_PIPE_X_SPACING_DISTANCE = BIRD_WIDTH * 3; // in pixels
const int MAX_PIPE_X_SPACING_DISTANCE = 100; // in pixels
const int MIN_PIPE_Y_SPACE = BIRD_HEIGHT * 3;
const int MAX_PIPE_Y_SPACE = SCREEN_HEIGHT - BIRD_HEIGHT * 2;

int _pipeSpeed = 2;
int _gravity = 1; // can't apply gravity every frame, apply every X time
int _points = 0;
unsigned long _gameOverTimestamp = 0;

const int IGNORE_INPUT_AFTER_GAME_OVER_MS = 500; //ignores input for 500ms after game over

// Initialize top pipe and bottom pipe arrays. The location/sizes don't matter
// at this point as we'll set them in setup()
Pipe _topPipes[NUM_PIPES] = { Pipe(0, 0, 0, 0),
                              Pipe(0, 0, 0, 0),
                              Pipe(0, 0, 0, 0)
                            };

Pipe _bottomPipes[NUM_PIPES] = { Pipe(0, 0, 0, 0),
                                 Pipe(0, 0, 0, 0),
                                 Pipe(0, 0, 0, 0)
                               };

Bird _bird(5, SCREEN_HEIGHT / 2, BIRD_WIDTH, BIRD_HEIGHT);

enum GameState {
  NEW_GAME,
  PLAYING,
  GAME_OVER,
};

GameState _gameState = NEW_GAME;

// This is necessary for the game to work on the ESP32
// See: 
//  - https://github.com/espressif/arduino-esp32/issues/1734
//  - https://github.com/Bodmer/TFT_eSPI/issues/189
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif



void nonGamePlayLoop();
void initializeGameEntities();
void gamePlayLoop();
void showLoadScreen();
void drawStatusBar();
void calcFrameRate();
//---------------------------------------------------------------------------












const unsigned char image_paint_0_bits[] = {0xFF};
static const unsigned char PROGMEM image_paint_0_bits_edit[] = {0xe1,0x48,0x10,0x00,0x81,0x1c,0x11,0x40,0xcf,0x48,0x1d,0xc0,0x89,0x4a,0x14,0x40,0xef,0x4e,0x1d,0xc0};
static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000
};


void testdrawbitmap(const uint8_t *bitmap, uint8_t w, uint8_t h) {
  display.clearDisplay();
  uint8_t icons[NUMFLAKES][3];

  // Initialize
  for (uint8_t f = 0; f < NUMFLAKES; f++) {
    icons[f][XPOS] = random(display.width());
    icons[f][YPOS] = 0;
    icons[f][DELTAY] = random(5) + 3; //speed
  }

  while (inSleepMode) {
    // Draw each icon
    for (uint8_t f = 0; f < NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, SSD1306_WHITE);
    }
    display.display();
    
    // Erase it + move it
    for (uint8_t f = 0; f < NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, SSD1306_BLACK);
      // Move it
      icons[f][YPOS] += icons[f][DELTAY];
      // If it's gone, reinit
      if (icons[f][YPOS] > display.height()) {
        icons[f][XPOS] = random(display.width());
        icons[f][YPOS] = 0;
        icons[f][DELTAY] = random(5) + 3;
      }
    }
  }
  display.clearDisplay();
  display.display();
  displayMenu();
}

//--------------------------------HELLO--------------------------------
void drawH() {
  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(0, y, image_paint_0_bits, 3, 3, 1);
    display.display();
  }

  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(10, y, image_paint_0_bits, 3, 3, 1);
    display.display();
  }

  for (int x = 0; x <= 10; x += 2) {
    display.drawBitmap(x, 6, image_paint_0_bits, 3, 3, 1);
    display.display();
  }
}

void drawE() {
  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(15, y, image_paint_0_bits, 3, 3, 1);
    display.display();
  }

  for (int x = 15; x <= 25; x += 2) {
    display.drawBitmap(x, 0, image_paint_0_bits, 3, 3, 1);
    display.display();
  }

  for (int x = 15; x <= 25; x += 2) {
    display.drawBitmap(x, 6, image_paint_0_bits, 3, 3, 1);
    display.display();
  }

  for (int x = 15; x <= 25; x += 2) {
    display.drawBitmap(x, 13, image_paint_0_bits, 3, 3, 1);
    display.display();
  }
}

void drawL1() {
  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(30, y, image_paint_0_bits, 3, 3, 1);
    display.display();
  }

  for (int x = 30; x <= 40; x += 2) {
    display.drawBitmap(x, 13, image_paint_0_bits, 3, 3, 1);
    display.display();
  }
}

void drawL2() {
  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(45, y, image_paint_0_bits, 3, 3, 1);
    display.display();
  }

  for (int x = 45; x <= 55; x += 2) {
    display.drawBitmap(x, 13, image_paint_0_bits, 3, 3, 1);
    display.display();
  }
}

void drawO() {
  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(60, y, image_paint_0_bits, 3, 3, 1);
    display.display();
  }

  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(70, y, image_paint_0_bits, 3, 3, 1);
    display.display();
  }

  for (int x = 60; x <= 70; x += 2) {
    display.drawBitmap(x, 0, image_paint_0_bits, 3, 3, 1);
    display.display();
  }

  for (int x = 60; x <= 70; x += 2) {
    display.drawBitmap(x, 13, image_paint_0_bits, 3, 3, 1);
    display.display();
  }
}
//-----------------------------------------------------------------------------------------

void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  //WiFi.begin(ssid, password);
  WiFi.begin("Wokwi-GUEST", "", 6);


  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
}


void updateTimeFromNTP() {
  unsigned long currentTime = millis(); // Lấy thời gian hiện tại

  // Cập nhật ngay lập tức tại thời điểm bắt đầu (0s)
  if (!initialUpdateDone) {
    initialTime = currentTime;
    initialUpdateDone = true;
    halfSecondUpdateDone = false;
    updateRTCFromNTP();
  }

  // Cập nhật tại 500ms sau lần cập nhật đầu tiên
  if (!halfSecondUpdateDone && currentTime - initialTime >= 500) {
    halfSecondUpdateDone = true;
    updateRTCFromNTP();
  }

  // Cập nhật mỗi 5s sau lần cập nhật cuối cùng
  if (currentTime - initialTime >= 5000) {
    initialTime = currentTime; // Cập nhật lại thời gian gốc
    updateRTCFromNTP();
  }
}

void updateRTCFromNTP() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  year = timeinfo.tm_year + 1900;
  month = timeinfo.tm_mon + 1;
  day = timeinfo.tm_mday;
  hour = timeinfo.tm_hour;
  minute = timeinfo.tm_min;
  second = timeinfo.tm_sec;
  dayofweek = timeinfo.tm_wday;

  RTC.setWeek(dayofweek);
  RTC.setDate(day, month, year);
  RTC.setTime(hour, minute, second);

  Serial.print(dayofweek);
  Serial.println(" Updated from NTP");
  Serial.print("NTP: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}


void displayTimeFromRTC() {
  updateTimeFromNTP();
  static const char* daysOfWeek[] = {"Monday", "Tuesday", "Wednesday", "Thurday", "Friday", "Saturday", "Sunday"};
  int weekDay = RTC.getWeek();
  Serial.println(weekDay);
  display.clearDisplay();

  
  display.setTextColor(SSD1306_WHITE);
  //---------------day-----------------------

  if (weekDay == dayofweek){
    display.setTextSize(1);
    if (strlen(daysOfWeek[weekDay]) < 7) {
      display.setCursor(45, 0);
    } else if ((strlen(daysOfWeek[weekDay]) == 7)) {
      display.setCursor(43, 0);
    } else if ((strlen(daysOfWeek[weekDay]) == 8)) {
      display.setCursor(41, 0);
    } else {
      display.setCursor(39, 0);
    }
    display.println(daysOfWeek[weekDay]);
  }
  //-----------------------------------------


  //---------------date-----------------------
  display.setTextSize(1);
  display.setCursor(36, 20);
  
  if (RTC.getDay() < 10){
    display.print("0");
    display.print(RTC.getDay());
    display.print("/");
  } else {
    display.print(RTC.getDay());
    display.print("/");
  }

  if (RTC.getMonth() < 10){
    display.print("0");
    display.print(RTC.getMonth());
    display.print("/");
  } else {
    display.print(RTC.getMonth());
    display.print("/");
  }

  display.println(RTC.getYear());
  //--------------------------------------

  //---------------time-----------------------
  display.setTextSize(2);
  display.setCursor(18, 40);
  if (RTC.getHours() < 10){
    display.print("0");
    display.print(RTC.getHours());
    display.print(":");
  } else {
    display.print(RTC.getHours());
    display.print(":");
  }

  if (RTC.getMinutes() < 10){
    display.print("0");
    display.print(RTC.getMinutes());
    display.print(":");
  } else {
    display.print(RTC.getMinutes());
    display.print(":");
  }
  
  if (RTC.getSeconds() < 10){
    display.print("0");
    display.print(RTC.getSeconds());
  } else {
    display.println(RTC.getSeconds());
  }
  //--------------------------------------
  display.display();
}

void TaskUpdateTime(void *pvParameters) {
  while (1) {
    if (inSleepModeActive) {
      displayTimeFromRTC();
      vTaskDelay(100 / portTICK_PERIOD_MS); // Tạm dừng 5 giây
    } else {
      vTaskDelete(NULL);
      break;
    }
  }
}

void TaskDisplayStars(void *pvParameters) {
  while (1) {
    if (inSleepModeActive) {
      testdrawbitmap(logo16_glcd_bmp, LOGO16_GLCD_HEIGHT, LOGO16_GLCD_WIDTH);
      vTaskDelay(100 / portTICK_PERIOD_MS); // Tạm dừng 1 giây
    } else {
      vTaskDelete(NULL);
      break;
    }
  }
}

void taskall() {
  if (!inSleepMode) {
    return;
  }
  inSleepModeActive = true; // Kích hoạt cờ

  xTaskCreatePinnedToCore(
    TaskUpdateTime,    
    "TaskUpdateTime",  
    10000,             
    NULL,             
    1,                
    &Task1,            
    0);                

  xTaskCreatePinnedToCore(
    TaskDisplayStars,   // Hàm thực thi task
    "TaskDisplayStars", // Tên task
    10000,              // Kích thước stack
    NULL,               // Tham số truyền vào task
    1,                  // Độ ưu tiên
    &Task2,             // Task handle
    1);                 // Core
}


void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  // if (!display.begin(i2c_Address, true)) {
  //   Serial.println(F("SH1106 allocation failed"));
  //   for (;;);
  // }

  Wire.begin();
  RTC.begin();
  RTC.setHourMode(CLOCK_H12);
  if (RTC.getHourMode() == CLOCK_H12) {
    RTC.setMeridiem(HOUR_PM);
  }

  pinMode(FLAP_BUTTON_INPUT_PIN, INPUT_PULLUP);

  display.clearDisplay();
  
  // drawH();
  // drawE();
  // drawL1();
  // drawL2();
  // drawO();
  
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 53);
  display.print("Thuan Hoang");
  display.display();
  display.drawBitmap(50, 2, image_DolphinNice_bits, 96, 59, 1);
  display.display();
  display.drawBitmap(15, 38, image_paint_0_bits_edit, 26, 5, 1);
  display.display();
  delay(500);
  
  display.clearDisplay();

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setBoundaries(-99999, 99999, true);
  rotaryEncoder.disableAcceleration();
  
  connectToWiFi();
  // Lấy thông tin thời gian từ NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  displayMenu();

  _fpsStartTimeStamp = millis();
}



void loop() {
  rotary_loop();
  handle_rotary_button();
  


  
}

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

void displayMenu() {
  if (!inSleepMode){
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    int startIndex = menuIndex - 2;
    int endIndex = menuIndex + 1;

    if (startIndex < 0) {
      startIndex = 0;
      endIndex = 3;
    } else if (endIndex >= 6) {
      endIndex = 5;
      startIndex = endIndex - 3;
    }

    for (int i = startIndex; i <= endIndex; i++) {
        if (i == menuIndex) {
          display.drawRoundRect(0, (i - startIndex) * 16, 126, 15, 1, SSD1306_WHITE);
        }
        display.setCursor(4, (i - startIndex) * 16 + 5);
        display.print(menus[i]);
      }
      display.display();
  }
}

void displaySubMenu1() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  for (int i = 0; i < numsubMenu1; i++) {
    if (i == menuIndex_subMenu) {
      display.drawRoundRect(0, i * 16, 126, 16, 1, SSD1306_WHITE);
    }
    display.setCursor(4, i * 16 + 5);
    display.print(subMenus1[i]);
  }
  display.display();
}

void displaySubMenu2() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  for (int i = 0; i < numsubMenu2; i++) {
    if (i == menuIndex_subMenu) {
      display.drawRoundRect(0, i * 16, 128, 16, 1, SSD1306_WHITE);
    }
    display.setCursor(4, i * 16 + 5);
    display.print(subMenus2[i]);
  }
  display.display();
}

void displayitemSubMenu1(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  for (int i = 0; i < numitemsubMenu1; i++){
    if (i == menuIndex_itemsubMenu){
      display.drawRoundRect(0, i * 16, 126, 16 , 1, SSD1306_WHITE);
    }
    display.setCursor(4, i * 16 + 5);
    display.print(itemsubMenus1[i]);
  }
  display.display();
}

void rotary_loop() {
  if (rotaryEncoder.encoderChanged()) {
    currentRotaryValue = rotaryEncoder.readEncoder();
    if (initemSubMenu1) {  
      if (currentRotaryValue > previousRotaryValue) {
        menuIndex_itemsubMenu = (menuIndex_itemsubMenu + 1) % numitemsubMenu1;
      } else {
        menuIndex_itemsubMenu = (menuIndex_itemsubMenu - 1 + numitemsubMenu1) % numitemsubMenu1;
      }
      displayitemSubMenu1();
    } else if (inSubMenu1) {  
      if (currentRotaryValue > previousRotaryValue) {
        menuIndex_subMenu = (menuIndex_subMenu + 1) % numsubMenu1;
      } else {
        menuIndex_subMenu = (menuIndex_subMenu - 1 + numsubMenu1) % numsubMenu1;
      }
      displaySubMenu1();
    } else if (inSubMenu2) {  
      if (currentRotaryValue > previousRotaryValue) {
        menuIndex_subMenu = (menuIndex_subMenu + 1) % numsubMenu2;
      } else {
        menuIndex_subMenu = (menuIndex_subMenu - 1 + numsubMenu2) % numsubMenu2;
      }
      displaySubMenu2();
    } else {
      if ((currentRotaryValue > previousRotaryValue)) {
        menuIndex = (menuIndex + 1) % numMenus;
      } else {
        menuIndex = (menuIndex - 1 + numMenus) % numMenus;
      }
      displayMenu();
    }
    previousRotaryValue = currentRotaryValue;
  }
}

void handle_rotary_button() {
  static bool buttonPressed = false;
  static unsigned long lastPressTime = 0;
  const unsigned long pressInterval_short = 500;


  bool isEncoderButtonDown = rotaryEncoder.isEncoderButtonDown();

  if (isEncoderButtonDown && !buttonPressed && (millis() - lastPressTime > pressInterval_short)) {
    onButtonShortPress();
    buttonPressed = true;
    lastPressTime = millis();
  } else if (!isEncoderButtonDown && buttonPressed) {
    buttonPressed = false;
  }
}

void onButtonShortPress() {
  if (initemSubMenu1) {
    if (itemsubMenus1[menuIndex_itemsubMenu] == "Exit") {  // Check "exit"
      initemSubMenu1 = false;
      displaySubMenu1();
    }
  } else if (inSubMenu1) {
    if (subMenus1[menuIndex_subMenu] == "Birthday") {
      initemSubMenu1 = true;
      menuIndex_itemsubMenu = 0;
      displayitemSubMenu1();
    } else if (subMenus1[menuIndex_subMenu] == "Name") {
      if (inName) {
        inName = false;
        displaySubMenu1();  // Return to subMenus1 display
      } else {
        inName = true;
        display.clearDisplay();
        display.setTextColor(1);
        display.setCursor(1, 7);
        display.print("Hoang Tran Minh Thuan");
        display.display();
        display.drawBitmap(8, 4, signature, 111, 60, 1);
        display.display();
      }
    } else if (subMenus1[menuIndex_subMenu] == "Exit") {
      inSubMenu1 = false;
      displayMenu();
    }
  } else if (inSubMenu2) {
    if (subMenus2[menuIndex_subMenu] == "Exit") {
      inSubMenu2 = false;
      displayMenu();
    }
  } else if (inSleepMode) {
    inSleepMode = false;
    inSleepModeActive = false;
  } else if (inqrcode) {
    inqrcode = false;
    displayMenu();
  } else if (inphoto) {
    inphoto = false;
    displayMenu();
  } else if (inEducation) {
    inEducation = false;
    displayMenu();
  } else {
    if (menus[menuIndex] == "Information") {
      inSubMenu1 = true;
      menuIndex_subMenu = 0;
      displaySubMenu1();
    } else if (menus[menuIndex] == "Contact") {
      inSubMenu2 = true;
      menuIndex_subMenu = 0;
      displaySubMenu2();
    } else if (menus[menuIndex] == "Education") {
      inEducation = true;
      menuIndex_subMenu = 0;
      randomSeed(analogRead(A5));
      showLoadScreen();
      initializeGameEntities();
      while(inEducation){
        if (rotaryEncoder.isEncoderButtonDown()){
          inEducation = false;
          displayMenu();
          break;
        }
        display.clearDisplay();
        drawStatusBar();
        if (_gameState == NEW_GAME || _gameState == GAME_OVER) {
          nonGamePlayLoop();
        } else if (_gameState == PLAYING) {
          gamePlayLoop();
        }
        calcFrameRate();
        display.display();
        if (DELAY_LOOP_MS > 0) {
          delay(DELAY_LOOP_MS);
        }
      }
    } else if (menus[menuIndex] == "Photo") {
      inphoto = true;
      display.clearDisplay();
      display.drawBitmap(60, 0, me_avtar, 60, 62, 1);
      display.display();
    } else if (menus[menuIndex] == "QR") {
      inqrcode = true;
      display.clearDisplay();
      display.drawBitmap(60, 0, qrcode, 64, 64, 1);
      display.display();
    } else if (menus[menuIndex] == "Night mode") {
      inSleepMode = true;
      taskall();
    }
  }
}


void nonGamePlayLoop() {
  for (int i = 0; i < NUM_PIPES; i++) {
    _topPipes[i].draw(display);
    _bottomPipes[i].draw(display);
  }

  int16_t x1, y1;
  uint16_t w, h;
  int flapButtonVal = digitalRead(FLAP_BUTTON_INPUT_PIN);
  if (_gameState == NEW_GAME) {

    display.getTextBounds(STR_PRESS_FLAP_TO_PLAY, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(display.width() / 2 - w / 2, 15);
    display.print(STR_PRESS_FLAP_TO_PLAY);

    if (rotaryEncoder.encoderChanged()) {
      _gameState = PLAYING;
    }
  } else if (_gameState == GAME_OVER) {
    display.setTextSize(2);
    display.getTextBounds(STR_GAME_OVER, 0, 0, &x1, &y1, &w, &h);
    int yText = 15;
    display.setCursor(display.width() / 2 - w / 2, yText);
    display.print(STR_GAME_OVER);

    yText = yText + h + 2;
    display.setTextSize(1);
    display.getTextBounds(STR_PRESS_FLAP_TO_PLAY, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(display.width() / 2 - w / 2, yText);
    display.print(STR_PRESS_FLAP_TO_PLAY);

    // We ignore input a bit after game over so that user can see end game screen
    // and not accidentally start a new game
    if (rotaryEncoder.encoderChanged() && millis() - _gameOverTimestamp >= IGNORE_INPUT_AFTER_GAME_OVER_MS) {
      // if the current state is game over, need to reset
      initializeGameEntities();
      _gameState = PLAYING;
    }
  }

  _bird.draw(display);
}

void initializeGameEntities() {
  _points = 0;

  _bird.setY(display.height() / 2 - _bird.getHeight() / 2);
  _bird.setDrawFill(true);

  const int minStartXPipeLocation = display.width() / 2;
  int lastPipeX = minStartXPipeLocation;
  for (int i = 0; i < NUM_PIPES; i++) {

    int pipeX = lastPipeX + random(MIN_PIPE_X_SPACING_DISTANCE, MAX_PIPE_X_SPACING_DISTANCE);
    int pipeWidth = random(MIN_PIPE_WIDTH, MAX_PIPE_WIDTH);

    int yGapBetweenPipes = random(MIN_PIPE_Y_SPACE, MAX_PIPE_Y_SPACE);

    int topPipeY = 0;
    int topPipeHeight = random(0, SCREEN_HEIGHT - yGapBetweenPipes);

    int bottomPipeY = topPipeHeight + yGapBetweenPipes;
    int bottomPipeHeight = SCREEN_HEIGHT - bottomPipeY;

    _topPipes[i].setLocation(pipeX, topPipeY);
    _topPipes[i].setDimensions(pipeWidth, topPipeHeight);
    _topPipes[i].setDrawFill(false);

    _bottomPipes[i].setLocation(pipeX, bottomPipeY);
    _bottomPipes[i].setDimensions(pipeWidth, bottomPipeHeight);
    _topPipes[i].setDrawFill(false);

    lastPipeX = _topPipes[i].getRight();
  }
}

void gamePlayLoop() {
  int flapButtonVal = digitalRead(FLAP_BUTTON_INPUT_PIN);
  _bird.setY(_bird.getY() + _gravity);


  if (rotaryEncoder.encoderChanged()) {
    _bird.setY(_bird.getY() - 3);
  }
  _bird.forceInside(0, 0, display.width(), display.height());

  // xMaxRight tracks the furthest right pixel of the furthest right pipe
  // which we will use to reposition pipes that go off the left part of screen
  int xMaxRight = 0;

  // Iterate through pipes and check for collisions and scoring
  for (int i = 0; i < NUM_PIPES; i++) {

    _topPipes[i].setX(_topPipes[i].getX() - _pipeSpeed);
    _bottomPipes[i].setX(_bottomPipes[i].getX() - _pipeSpeed);

    _topPipes[i].draw(display);
    _bottomPipes[i].draw(display);

    Serial.println(_topPipes[i].toString());

    // Check if the bird passed by the pipe
    if (_topPipes[i].getRight() < _bird.getLeft()) {

      // If we're here, the bird has passed the pipe. Check to see
      // if we've marked it as passed yet. If not, then increment the score!
      if (_topPipes[i].getHasPassedBird() == false) {
        _points++;
        _topPipes[i].setHasPassedBird(true);
        _bottomPipes[i].setHasPassedBird(true);
      }
    }

    // xMaxRight is used to track future placements of pipes once
    // they go off the left part of the screen
    if (xMaxRight < _topPipes[i].getRight()) {
      xMaxRight = _topPipes[i].getRight();
    }

    // Check for collisions and end of game
    if (_topPipes[i].overlaps(_bird)) {
      _topPipes[i].setDrawFill(true);
      _gameState = GAME_OVER;
      _gameOverTimestamp = millis();
    } else {
      _topPipes[i].setDrawFill(false);
    }

    if (_bottomPipes[i].overlaps(_bird)) {
      _bottomPipes[i].setDrawFill(true);
      _gameState = GAME_OVER;
      _gameOverTimestamp = millis();
    } else {
      _bottomPipes[i].setDrawFill(false);
    }
  }

  // Check for pipes that have gone off the screen to the left
  // and reset them to off the screen on the right
  xMaxRight = max(xMaxRight, display.width());
  for (int i = 0; i < NUM_PIPES; i++) {
    if (_topPipes[i].getRight() < 0) {
      int pipeX = xMaxRight + random(MIN_PIPE_X_SPACING_DISTANCE, MAX_PIPE_X_SPACING_DISTANCE);
      int pipeWidth = random(MIN_PIPE_WIDTH, MAX_PIPE_WIDTH);

      int yGapBetweenPipes = random(MIN_PIPE_Y_SPACE, MAX_PIPE_Y_SPACE);

      int topPipeY = 0;
      int topPipeHeight = random(0, SCREEN_HEIGHT - yGapBetweenPipes);

      int bottomPipeY = topPipeHeight + yGapBetweenPipes;
      int bottomPipeHeight = SCREEN_HEIGHT - bottomPipeY;

      _topPipes[i].setLocation(pipeX, topPipeY);
      _topPipes[i].setDimensions(pipeWidth, topPipeHeight);
      _topPipes[i].setHasPassedBird(false);

      _bottomPipes[i].setLocation(pipeX, bottomPipeY);
      _bottomPipes[i].setDimensions(pipeWidth, bottomPipeHeight);
      _bottomPipes[i].setHasPassedBird(false);

      xMaxRight = _topPipes[i].getRight();
    }
  }

  _bird.draw(display);
}


void showLoadScreen() {
  // Clear the buffer
  display.clearDisplay();

  // Show load screen
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);

  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(1);

  int yText = 10;
  display.setTextSize(2);
  yText = yText + h + 1;
  display.getTextBounds(STR_LOADSCREEN_APP_NAME_LINE1, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(display.width() / 2 - w / 2, yText);
  display.print(STR_LOADSCREEN_APP_NAME_LINE1);

  yText = yText + h + 1;
  display.getTextBounds(STR_LOADSCREEN_APP_NAME_LINE2, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(display.width() / 2 - w / 2, yText);
  display.print(STR_LOADSCREEN_APP_NAME_LINE2);

  display.display();
  delay(LOAD_SCREEN_SHOW_MS);
  display.clearDisplay();
  display.setTextSize(1);

}

/**
 * Call this every frame to calculate frame rate
 */
void calcFrameRate() {
  unsigned long elapsedTime = millis() - _fpsStartTimeStamp;
  _frameCount++;
  if (elapsedTime > 1000) {
    _fps = _frameCount / (elapsedTime / 1000.0);
    _fpsStartTimeStamp = millis();
    _frameCount = 0;
  }
}

/**
 * Draws the status bar at top of screen with points and fps
 */
void drawStatusBar() {
  // Draw accumulated points
  display.setTextSize(1);
  display.setCursor(0, 0); // draw points
  display.print(_points);

  // Draw frame count
  if (_drawFrameCount) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds("XX.XX fps", 0, 0, &x1, &y1, &w, &h);
    display.setCursor(display.width() - w, 0);
    display.print(_fps);
    display.print(" fps");
  }
}