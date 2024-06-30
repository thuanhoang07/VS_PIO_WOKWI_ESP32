#include <AiEsp32RotaryEncoder.h>
#include <Wire.h>
#include <WiFi.h>
#include "time.h"
#include <I2C_RTC.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
//#include <Adafruit_SH110X.h>

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
void connectToWiFi();
void updateTimeFromNTP();
void displayTimeFromRTC();
void TaskUpdateTime();
void TaskDisplayStars();

TaskHandle_t Task1;
TaskHandle_t Task2;

String menus[] = {"Information", "Contact", "Education", "Menu 4", "Menu 5", "Sleep MODE"};
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

int previousRotaryValue = 0;
int currentRotaryValue = 0;

unsigned long lastUpdateTime = 0; // Biến để theo dõi thời gian cập nhật cuối cùng
const unsigned long updateInterval = 60*1000; // Khoảng thời gian giữa các lần cập nhật (5000ms = 5s)

const unsigned char image_paint_0_bits[] = {0xFF};
static const unsigned char PROGMEM image_paint_0_bits_edit[] = {0xe1,0x48,0x10,0x00,0x81,0x1c,0x11,0x40,0xcf,0x48,0x1d,0xc0,0x89,0x4a,0x14,0x40,0xef,0x4e,0x1d,0xc0};
static const unsigned char PROGMEM image_DolphinNice_bits[] = {0x00};
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
    icons[f][DELTAY] = random(5) + 1;
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
        icons[f][DELTAY] = random(5) + 1;
      }
    }

    
  }

  display.clearDisplay();
  display.display();
  displayMenu(); // Display the main menu when exiting sleep mode
}


void drawH() {
  // Vertical line left
  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(0, y, image_paint_0_bits, 3, 3, 1);
    display.display();
    //
  }

  // Vertical line right
  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(10, y, image_paint_0_bits, 3, 3, 1);
    display.display();
    //delay(1);
  }

  // Horizontal line middle
  for (int x = 0; x <= 10; x += 2) {
    display.drawBitmap(x, 6, image_paint_0_bits, 3, 3, 1);
    display.display();
    //delay(1);
  }
}

void drawE() {
  // Vertical line
  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(15, y, image_paint_0_bits, 3, 3, 1); // Adjusted for spacing
    display.display();
    //delay(1);
  }

  // Horizontal line top
  for (int x = 15; x <= 25; x += 2) {
    display.drawBitmap(x, 0, image_paint_0_bits, 3, 3, 1);
    display.display();
    //delay(1);
  }

  // Horizontal line middle
  for (int x = 15; x <= 25; x += 2) {
    display.drawBitmap(x, 6, image_paint_0_bits, 3, 3, 1);
    display.display();
    //delay(1);
  }

  // Horizontal line bottom
  for (int x = 15; x <= 25; x += 2) {
    display.drawBitmap(x, 13, image_paint_0_bits, 3, 3, 1);
    display.display();
    //delay(1);
  }
}

void drawL1() {
  // Vertical line
  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(30, y, image_paint_0_bits, 3, 3, 1); // Adjusted for spacing
    display.display();
    //delay(1);
  }

  // Horizontal line bottom
  for (int x = 30; x <= 40; x += 2) {
    display.drawBitmap(x, 13, image_paint_0_bits, 3, 3, 1);
    display.display();
    //delay(1);
  }
}

void drawL2() {
  // Vertical line
  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(45, y, image_paint_0_bits, 3, 3, 1); // Adjusted for spacing
    display.display();
    //delay(1);
  }

  // Horizontal line bottom
  for (int x = 45; x <= 55; x += 2) {
    display.drawBitmap(x, 13, image_paint_0_bits, 3, 3, 1);
    display.display();
    //delay(1);
  }
}

void drawO() {
  // Vertical line left
  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(60, y, image_paint_0_bits, 3, 3, 1); // Adjusted for spacing
    display.display();
    //delay(1);
  }

  // Vertical line right
  for (int y = 0; y <= 13; y += 2) {
    display.drawBitmap(70, y, image_paint_0_bits, 3, 3, 1);
    display.display();
    //delay(1);
  }

  // Horizontal line top
  for (int x = 60; x <= 70; x += 2) {
    display.drawBitmap(x, 0, image_paint_0_bits, 3, 3, 1);
    display.display();
    //delay(1);
  }

  // Horizontal line bottom
  for (int x = 60; x <= 70; x += 2) {
    display.drawBitmap(x, 13, image_paint_0_bits, 3, 3, 1);
    display.display();
    //delay(1);
  }
}

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

  if (currentTime - lastUpdateTime >= updateInterval || lastUpdateTime == 0) { // Kiểm tra nếu đã đủ 5 giây hoặc lần đầu tiên
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }

    int year = timeinfo.tm_year + 1900;
    int month = timeinfo.tm_mon + 1;
    int day = timeinfo.tm_mday;
    int hour = timeinfo.tm_hour;
    int minute = timeinfo.tm_min;
    int second = timeinfo.tm_sec;
    int dayofweek = timeinfo.tm_wday;

    RTC.setWeek(dayofweek);
    RTC.setDate(day, month, year);
    RTC.setTime(hour, minute, second);

    Serial.println("Updated from NTP");
    Serial.print("NTP: ");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

    lastUpdateTime = currentTime; // Cập nhật thời gian cập nhật cuối cùng
  }
}

void displayTimeFromRTC() {
  static const char* daysOfWeek[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  int weekDay = RTC.getWeek();

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  // Display day of the week
  display.setCursor(0, 0);
  
  display.println(daysOfWeek[weekDay]);

  display.setTextSize(2);
  // Display day, month, and year
  display.setCursor(0, 20);
  
  display.print(RTC.getDay());
  display.print("-");
  display.print(RTC.getMonth());
  display.print("-");
  display.println(RTC.getYear());

  display.setTextSize(2);
  // Display day, month, and year
  display.setCursor(0, 40);
  display.print(RTC.getMonth());
  display.print(":");
  display.print(RTC.getMinutes());
  display.print(":");
  display.println(RTC.getSeconds());

  // Optionally display AM/PM
  /*
  if (RTC.getHourMode() == CLOCK_H12) {
    display.print(RTC.getMeridiem() == HOUR_AM ? " AM" : " PM");
  }
  */

  display.display();

  // Print to Serial Monitor as well
  Serial.print(daysOfWeek[weekDay]);
  Serial.print(" ");
  Serial.print(RTC.getDay());
  Serial.print("-");
  Serial.print(RTC.getMonth());
  Serial.print("-");
  Serial.print(RTC.getYear());
  Serial.print(" ");
  Serial.print(RTC.getHours());
  Serial.print(":");
  Serial.print(RTC.getMinutes());
  Serial.print(":");
  Serial.println(RTC.getSeconds());

  /*
  if (RTC.getHourMode() == CLOCK_H12) {
    Serial.print(RTC.getMeridiem() == HOUR_AM ? " AM" : " PM");
  }
  Serial.println();
  */
}


void TaskUpdateTime(void *pvParameters) {
  while (1) {
    if (inSleepModeActive) {
      updateTimeFromNTP();
      displayTimeFromRTC();
      vTaskDelay(100 / portTICK_PERIOD_MS); // Tạm dừng 5 giây
    } else {
      vTaskDelete(NULL);
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
    }
  }
}

void taskall() {
  if (!inSleepMode) {
    return;
  }
  inSleepModeActive = true; // Kích hoạt cờ

  xTaskCreatePinnedToCore(
    TaskUpdateTime,    // Hàm thực thi task
    "TaskUpdateTime",  // Tên task
    10000,             // Kích thước stack
    NULL,              // Tham số truyền vào task
    1,                 // Độ ưu tiên của task
    &Task1,            // Task handle
    0);                // Core mà task sẽ chạy

  xTaskCreatePinnedToCore(
    TaskDisplayStars,   // Hàm thực thi task
    "TaskDisplayStars", // Tên task
    10000,              // Kích thước stack
    NULL,               // Tham số truyền vào task
    1,                  // Độ ưu tiên của task
    &Task2,             // Task handle
    1);                 // Core mà task sẽ chạy
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

  

  display.clearDisplay();
  /*
  drawH();
  drawE();
  drawL1();
  drawL2();
  drawO();
  */
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 53);
  display.print("Thuan Hoang");
  display.display();
  // Draw the bitmap
  display.drawBitmap(50, 2, image_DolphinNice_bits, 96, 59, 1);
  display.display();
  display.drawBitmap(15, 38, image_paint_0_bits_edit, 26, 5, 1);
  display.display();
  delay(1000);
  
  display.clearDisplay();

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setBoundaries(-99999, 99999, true);
  rotaryEncoder.disableAcceleration();
  
  connectToWiFi();
  // Lấy thông tin thời gian từ NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  displayMenu();
}



void loop() {
  rotary_loop();
  handle_rotary_button();
}

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

void displayMenu() {
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
      display.drawRoundRect(0, i * 16, 128, 16 , 1, SSD1306_WHITE);
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
      if (currentRotaryValue > previousRotaryValue) {
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
  bool isEncoderButtonDown = rotaryEncoder.isEncoderButtonDown();

  if (isEncoderButtonDown && !buttonPressed) {
    onButtonShortPress();
    buttonPressed = true;
  } else if (!isEncoderButtonDown && buttonPressed) {
    buttonPressed = false;
  }
}

void onButtonShortPress() {
  if (initemSubMenu1) {
    if (itemsubMenus1[menuIndex_itemsubMenu] == "Exit") {  // Kiểm tra điều kiện "exit"
      initemSubMenu1 = false;
      displaySubMenu1();
    }
  } else if (inSubMenu1) {
    if (subMenus1[menuIndex_subMenu] == "Birthday") {
      initemSubMenu1 = true;
      menuIndex_itemsubMenu = 0;
      displayitemSubMenu1();
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
    inSleepModeActive = false; // Dừng task
    delay(100);
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
    } else if (menus[menuIndex] == "Sleep MODE") {
      inSleepMode = true;
      taskall();
    }
  }
}

