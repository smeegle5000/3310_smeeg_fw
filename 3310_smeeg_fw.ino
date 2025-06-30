#include <U8g2lib.h>
#include <EEPROM.h>
extern "C" {
  #include "t9.h"
}
#include "fonts.h"
#include "dict_en.h"

#define DISP_PIN_SCK PB3
#define DISP_PIN_SDIN PB5
#define DISP_PIN_CS PB6
#define DISP_PIN_DC PB7
#define DISP_PIN_RST PB8

#define PIN_BACKLIGHT PA2
#define PIN_BUZZER PB13
#define PIN_VIBES PA3
#define PIN_POWER_OFF PB15  //high to power off
#define PIN_BATTERY_VOLTAGE PA1
#define PIN_BATTERY_CHARGE PA5

float BATTERY_VOLTAGE = 0;
int BATTERY_CHARGING = 0;             //no idea why but a bool or byte just does not work for this
uint8_t BATTERY_AVERAGE[8] = {0};
uint8_t BATTERY_AVERAGE_INDEX = 0;
uint8_t BATTERY_VOLTAGE_16 = 0;
uint8_t BACKLIGHT_PWM = 20;
uint8_t BACKLIGHT_FREQ = 0;   //note to self, make this x20, it would probably be better to do entirely user definable but having to tap to 128 is bad enough as it is
uint8_t BACKLIGHT_ENABLE = 0; //0 = off 1 = on 2 = always on
uint8_t VIBRATION_ON_KEYPRESS_ENABLE = 1;
uint8_t VIBRATION_ON_KEYPRESS = 200;
unsigned long VIBRATION_ON_KEYPRESS_TIMER = 0;
uint8_t KB_LIGHT_ON_PRESS = 0;  //x40 to make it up to 10 seconds
uint8_t KB_LIGHT_ON_PRESS_ENABLE = 0;
unsigned long KB_LIGHT_ON_PRESS_TIMER = 0;
unsigned long INTERNAL_TIMER_1 = 0;
uint8_t INTERNAL_TIMER_2 = 0;
uint8_t INTERNAL_FLAG_1 = 0;  //general purpose flag, currently used for backlight menu and splash screen toggle, be sure to reset before using



//keyboard stuff
/*
  C1   C2   C3
 PB12  PC3  PC1
 ---------------+
      enter     |  enter pin PD2
 Back PgDn PgUp |  R1 PA10
  1    2    3   |  R2 PA9
  4    5    6   |  R3 PA8
  7    8    9   |  R4 PC9
  *    0    #   |  R5 PC8
*/
const uint8_t KB_NUM_ROW = 5;
const uint8_t KB_NUM_COL = 3;
const char KB_KEYMAP[KB_NUM_ROW][KB_NUM_COL] = {
  {'B', 'D', 'U'}, // Back, PgDn, PgUp | enter = E (hardcoded elsewere)
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
const char* KB_TAPMAP[10] = {
  " 0",     // 0
  "",      // 1 (unused or punctuation if needed)
  "abcABC2",   // 2
  "defDEF3",   // 3
  "ghiGHI4",   // 4
  "jklJKL5",   // 5
  "mnoMNO6",   // 6
  "pqrsPQRS7",  // 7
  "tuvTUV8",   // 8
  "wxyzWXYZ9"   // 9
};
#define KB_BUTTON_ENTER PD2
const uint8_t KB_ROW_PINS[] = {PA10, PA9, PA8, PC9, PC8};
const uint8_t KB_COL_PINS[] = {PB12, PC3, PC1};
bool KB_KEYSTATE[KB_NUM_ROW][KB_NUM_COL]; //low = pressed
bool KB_LAST_STATE[KB_NUM_ROW][KB_NUM_COL];
//secret tool for later
char KB_BUFFER = 0;
char KB_BUFFER_PREVIOUS = 0;
bool KB_BEEN_READ = true;
bool KB_HOLDSTATE = false;
float KB_HOLD_DURATION = 0;
uint8_t KB_HOLD_REPEAT = true;
uint8_t KB_HOLD_REPEAT_SPEED = 80; //miliseconds
uint8_t KB_HOLD_REPEAT_DELAY = 75; //miliseconds, x20
uint8_t KB_DOUBLE_PRESS_TIME = 500;  //miliseconds
uint8_t KB_DOUBLE_PRESS_ENABLED = 0;
uint8_t KB_DOUBLE_PRESS_EVENT_COUNT = 0;
bool KB_DEBOUNCING_ACTIVE = true;
uint8_t KB_DEBOUNCING_WINDOW = 5; //miliseconds
uint8_t KB_TAP_OR_T9 = 0; //0 = tap 1 = t9
//
//internal use on keyboard scanning
bool KB_KEY_MATCHING = false;
bool KB_IS_ANYTHING_PRESSED = false;  //strobes true and false while checking the holdstate
unsigned long KB_HOLD_START_TIME = 0;  //stores timestamp when the key was first held
unsigned long KB_LAST_PRESSED = 0;      //stores timestamp when a key was last pressed (used in double press)
unsigned int KB_HOLD_REPEAT_TIMER = 0; //stores timestamp for repeating held keys
//
//



char DISP_TEXT_BUFFER[32] = "";
uint8_t DISP_TEXT_BUFFER_INDEX = 0;
uint8_t DISP_PAGE_A = 0;
uint8_t DISP_PAGE_B = 0;
uint8_t DISP_PAGE_C = 0;
uint8_t DISP_PAGE_D = 0;           //just one more display variable bro, i swear thats it bro
uint8_t DISP_SCROLL_OFFSET = 0;    //💀
uint8_t DISP_NUM_OPTIONS = 1;
uint8_t UNREAD_MESSAGE_COUNT = 0;
char DISP_TITLE_TEXT[16] = "";
uint8_t DISP_TITLE_TEXT_INDEX = 0;
uint8_t DISP_CENTER_POINT = 0;
char DISP_INTERNAL_BUFFER[16] = "";
char DISP_INTERNAL_BUFFER_2[32] = "";
uint8_t DISP_CONTRAST = 110;
char DISP_MESSAGE_BUFFER[1024] = "";
uint16_t DISP_MESSAGE_BUFFER_INDEX = 0;
uint8_t DISP_MESSAGE_LENGTH = 0;
uint8_t DISP_MESSAGE_PRINT_X = 0;
uint8_t DISP_MESSAGE_PRINT_Y = 0;

//extra stuff while i figure out pointers and such
int a = 0;
int b = 0;
int c = 0;
const char* CURRENT_CHAR_POINTER = DISP_MESSAGE_BUFFER;


// Using software SPI on these pins for nokia 3310 display
U8G2_PCD8544_84X48_F_4W_SW_SPI u8g2(U8G2_R0, DISP_PIN_SCK, DISP_PIN_SDIN, DISP_PIN_CS, DISP_PIN_DC, DISP_PIN_RST);

int RF_RSSI = 69;             //temp value, no code to actually update this
uint8_t RF_RSSI_8 = 0;
uint8_t RF_TXRX_STATE = 0;    //temp value, no code to actually update this
float RF_TX_FREQ = 433.4750;  //temp value, no code to actually update this
float RF_RX_FREQ = 433.4750;  //temp value, no code to actually update this



void setup() {
  //setup display, set contrast, display splash screen
  u8g2.begin();


  EEPROM.get(0, INTERNAL_TIMER_2);   //i'm not this rn so i may as well save defining another variable for this
  if (INTERNAL_TIMER_2 != 69) {
    u8g2.clearBuffer();
    u8g2.setContrast(110); //"safe" default value
    u8g2.setFont(NokiaSmallPlain);
    u8g2.drawXBM(13, 0, 59, 31, icon_m17_splash);
    u8g2.drawStr(11, 39, "First time setup");
    u8g2.drawStr(17, 48, "Please wait...");
    u8g2.sendBuffer();
    EEPROM.put(0, 69);    //random value to just see if its a first time startup or not
    EEPROM.put(1, 110);   //display contrast value
    EEPROM.put(2, 2);     //display backlight start up state (0 = off, 1 = kb 2 = on)
    EEPROM.put(3, 20);    //display backlight PWM value
    EEPROM.put(4, 128);   //display backlight frequency
    EEPROM.put(5, 1);     //do we delay startup to show the boot screen
    EEPROM.put(6, 0);     //no lights on keypress
    EEPROM.put(7, 0);     //no time for lights on keypress
    EEPROM.put(8, 0);     //disable vibes
    EEPROM.put(9, 0);     //0 vibe time
    //10 audio feedback on/off
    //11 audio feedback frequency
    //12 audio feedback length/volume
    EEPROM.put(13, 1);    //debouncing enabled
    EEPROM.put(14, 5);    //5ms debounce settle time
    EEPROM.put(15, 1);    //double tap enabled
    EEPROM.put(16, 13);    //double tap window (x40)
    EEPROM.put(17, 1);    //hold repeat enable
    EEPROM.put(18, 75);   //repeat delay (x20)
    EEPROM.put(19, 80);   //hold repeat speed

  }                       //for some reason the eeprom writing appears to be doing an entire erase/write cycle for each address
                          //in the short term this is probably fine but in the long term it would be good to find an alternitive way to do this

  EEPROM.get(1, DISP_CONTRAST);
  EEPROM.get(2, BACKLIGHT_ENABLE);
  EEPROM.get(3, BACKLIGHT_PWM);
  EEPROM.get(4, BACKLIGHT_FREQ);
  EEPROM.get(5, INTERNAL_FLAG_1);     //startup splash enable
  EEPROM.get(6, KB_LIGHT_ON_PRESS_ENABLE);
  EEPROM.get(7, KB_LIGHT_ON_PRESS);
  EEPROM.get(8, VIBRATION_ON_KEYPRESS_ENABLE);
  EEPROM.get(9, VIBRATION_ON_KEYPRESS);
  EEPROM.get(13, KB_DEBOUNCING_ACTIVE);
  EEPROM.get(14, KB_DEBOUNCING_WINDOW);
  EEPROM.get(15, KB_DOUBLE_PRESS_ENABLED);
  EEPROM.get(16, KB_DOUBLE_PRESS_TIME);
  EEPROM.get(17, KB_HOLD_REPEAT);    
  EEPROM.get(18, KB_HOLD_REPEAT_DELAY);
  EEPROM.get(19, KB_HOLD_REPEAT_SPEED);

  
  u8g2.setContrast(DISP_CONTRAST);
  if (INTERNAL_FLAG_1 == 1) {
    u8g2.clearBuffer();
    u8g2.drawXBM(13, 9, 59, 31, icon_m17_splash);
    u8g2.sendBuffer();
  }

  //setup keyboard matrix
    for (uint8_t i = 0; i < KB_NUM_ROW; i++) {  //increment through all row pins, define as input pins, hardware pull down resistors are in place
    pinMode(KB_ROW_PINS[i], INPUT);
  }
    for (uint8_t j = 0; j < KB_NUM_COL; j++) {  //increment through all col pins, define as output pins, and set to high
    pinMode(KB_COL_PINS[j], OUTPUT);
  }


  pinMode(KB_BUTTON_ENTER, INPUT);       //define enter pin, hardware contains pullup
  pinMode(PIN_BATTERY_VOLTAGE, INPUT_ANALOG);
  pinMode(PIN_BATTERY_CHARGE, INPUT);
  pinMode(PIN_VIBES, OUTPUT);
  pinMode(PIN_POWER_OFF, OUTPUT);
  //


  DISP_TEXT_BUFFER[0] = '\0';
  DISP_MESSAGE_BUFFER[0] = '\0';

  analogWriteResolution(8);
  analogWriteFrequency(BACKLIGHT_FREQ * 20);
  if (BACKLIGHT_ENABLE == 2) {
    analogWrite(PIN_BACKLIGHT, BACKLIGHT_PWM);
  } else {
    analogWrite(PIN_BACKLIGHT, 0);
  }
  analogReadResolution(8);
  
  

  BATTERY_AVERAGE[BATTERY_AVERAGE_INDEX] = analogRead(PIN_BATTERY_VOLTAGE);       //populates battery voltage average values so it doesnt slowly rise on boot
  BATTERY_AVERAGE_INDEX++;                                                        //because it only measures vbat every .5s to slow down the jitter on the hundredths digit
  BATTERY_AVERAGE[BATTERY_AVERAGE_INDEX] = analogRead(PIN_BATTERY_VOLTAGE);       //i should probably make a for loop but cant be arsed
  BATTERY_AVERAGE_INDEX++;
  BATTERY_AVERAGE[BATTERY_AVERAGE_INDEX] = analogRead(PIN_BATTERY_VOLTAGE);
  BATTERY_AVERAGE_INDEX++;
  BATTERY_AVERAGE[BATTERY_AVERAGE_INDEX] = analogRead(PIN_BATTERY_VOLTAGE);
  BATTERY_AVERAGE_INDEX++;
  BATTERY_AVERAGE[BATTERY_AVERAGE_INDEX] = analogRead(PIN_BATTERY_VOLTAGE);
  BATTERY_AVERAGE_INDEX++;
  BATTERY_AVERAGE[BATTERY_AVERAGE_INDEX] = analogRead(PIN_BATTERY_VOLTAGE);
  BATTERY_AVERAGE_INDEX++;
  BATTERY_AVERAGE[BATTERY_AVERAGE_INDEX] = analogRead(PIN_BATTERY_VOLTAGE);
  BATTERY_AVERAGE_INDEX++;

  strcpy(DISP_TITLE_TEXT, "M17 IARU R1");                                         //TEMP UNTIL I MAKE THIS USER DEFINABLE


  if (INTERNAL_FLAG_1 == 1) {                                                     //boot delay so we can show the goods (m17 boot splash)
    delay(2000);
  }

  KB_TAP_OR_T9 = 1;
}

void loop() {
  u8g2.clearBuffer();
  kb_scan();   //call "void kb_scan()" to be run, return here when done

  if (BACKLIGHT_ENABLE == 1) {
    if (KB_LIGHT_ON_PRESS_ENABLE == 1 && KB_BEEN_READ == false) {
      KB_LIGHT_ON_PRESS_TIMER = (millis() + (KB_LIGHT_ON_PRESS * 40));
    }

    if (millis() < KB_LIGHT_ON_PRESS_TIMER) {
      analogWrite(PIN_BACKLIGHT, BACKLIGHT_PWM);
    } else {
      analogWrite(PIN_BACKLIGHT, 0);
    }
  }

  if (VIBRATION_ON_KEYPRESS_ENABLE == 1) {
    if (KB_BEEN_READ == false) {
      VIBRATION_ON_KEYPRESS_TIMER = (millis() + (VIBRATION_ON_KEYPRESS));
    }

    if (millis() < VIBRATION_ON_KEYPRESS_TIMER) {
      digitalWrite(PIN_VIBES, HIGH);
    } else {
      digitalWrite(PIN_VIBES, LOW);
    }
  }


  if (millis() - INTERNAL_TIMER_1 >= 500) {                                                   //run every half second
    INTERNAL_TIMER_1 = millis();
    INTERNAL_TIMER_2++;

    BATTERY_AVERAGE[BATTERY_AVERAGE_INDEX] = analogRead(PIN_BATTERY_VOLTAGE);
    BATTERY_AVERAGE_INDEX++;

    if (BATTERY_AVERAGE_INDEX > 8) {                                                            //run every 8 readings of the voltage
      BATTERY_AVERAGE_INDEX = 0;                                                                //reset index to 0 so new values can fill in the old to average
    }

    for (int i = 0; i < 8; i++) {
      BATTERY_VOLTAGE = BATTERY_VOLTAGE + BATTERY_AVERAGE[i];                                   //average together the 8 battery voltage readings
    }
    BATTERY_VOLTAGE = (BATTERY_VOLTAGE / 8) * 0.02588;                                          //calculate decimal voltage reading from the average resistor voltage divider input
    //BATTERY_VOLTAGE_16 = constrain((BATTERY_VOLTAGE - 3.2) * (16.0 / (4.2 - 3.2)), 0, 16);    //create 16 level battery indicator for fancy battery meter
    BATTERY_VOLTAGE_16 = ((BATTERY_VOLTAGE - 3.2) * 16);

    //RF_RSSI_8 = constrain((RF_RSSI * 8.0) / 100, 0, 8);                                       //create 8 level rssi indicator for fancy signal strength meter
    RF_RSSI_8 = (8 / 100) * RF_RSSI;

    BATTERY_CHARGING = !digitalRead(PIN_BATTERY_CHARGE);
    
  }

  if (INTERNAL_TIMER_2 >= 4) {                                                     //run every 2 seconds
    INTERNAL_TIMER_2 = 0;

  }
  
     

  if (DISP_PAGE_A == 0) {
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(0, 7, "M17");                                                //taskbar icon

    u8g2.setFont(u8g2_font_NokiaLargeBold_tr);
    DISP_CENTER_POINT = u8g2_GetStrWidth(u8g2.getU8g2(), DISP_TITLE_TEXT);    //get the offset of the text to centre it on the screen
    u8g2.setCursor(42 - (DISP_CENTER_POINT / 2), 23);                         //42 = display horizontal midpoint, 23 = vertical point on screen i want to draw at
    u8g2.print(DISP_TITLE_TEXT);

    u8g2.setFont(NokiaSmallPlain);
    dtostrf(RF_TX_FREQ, 8, 4, DISP_INTERNAL_BUFFER);                          //this is a float so i need to convert it to a string before u8g2 can get its size
    DISP_CENTER_POINT = u8g2_GetStrWidth(u8g2.getU8g2(), DISP_INTERNAL_BUFFER);
    u8g2.setCursor(35 - (DISP_CENTER_POINT / 2), 33);                         //manually offset to account for the size of the "tx: " string
    u8g2.print("tx: ");
    u8g2.print(RF_TX_FREQ, 4);                                                //print with 4 decimals of precision

    dtostrf(RF_RX_FREQ, 8, 4, DISP_INTERNAL_BUFFER);
    DISP_CENTER_POINT = u8g2_GetStrWidth(u8g2.getU8g2(), DISP_INTERNAL_BUFFER);
    u8g2.setCursor(34 - (DISP_CENTER_POINT / 2), 42);
    u8g2.print("rx: ");
    u8g2.print(RF_RX_FREQ, 4);

    if (RF_TXRX_STATE == 1)  {
      u8g2.setCursor(23, 0);
      u8g2.drawXBM(23, 0, 5, 7, icon_tx);
    }
    if (RF_TXRX_STATE == 2) {
      u8g2.setCursor(23, 0);
      u8g2.drawXBM(23, 0, 5, 7, icon_rx);
    }

    if (DISP_PAGE_B == 0) {                                               //verbose mode taskbar
      u8g2.setFont(NokiaSmallPlain);
      if (RF_TXRX_STATE > 0) {
        u8g2.setCursor(29, 7);
        u8g2.print("-");
        u8g2.print(RF_RSSI);
        u8g2.print("db");
      }
      u8g2.setCursor(62, 7);
      u8g2.print(BATTERY_VOLTAGE);
      if (BATTERY_CHARGING == 1) {
       u8g2.drawXBM(79, 0, 5, 7, icon_battery_charging);
      } else{
        u8g2.print("V");
      }

    }
    if (DISP_PAGE_B == 1) {                                                 //"fancy" mode taskbar
      u8g2.setFont(NokiaSmallPlain);
      if (BATTERY_CHARGING == 1) {                                          //if the battery charge state is true, move the battery icon over and draw the plug symbol
        u8g2.drawXBM(79, 0, 5, 7, icon_battery_charging);
        u8g2.drawXBM(58, 0, 19, 7, icon_battery);
        u8g2.drawBox(59, 1, BATTERY_VOLTAGE_16, 5);
      } else {                                                              //default state with the battery icon in the corner
        u8g2.drawXBM(65, 0, 19, 7, icon_battery);
        u8g2.drawBox(66, 1, BATTERY_VOLTAGE_16, 5);
      }
      if (RF_TXRX_STATE > 0) {  
        if (RF_RSSI_8 >= 1) u8g2.setCursor(30, 7), u8g2.print(".");       //drawing the fancy rssi bar graph, 
        if (RF_RSSI_8 >= 2) u8g2.drawLine(32, 5, 32, 6);
        if (RF_RSSI_8 >= 3) u8g2.drawLine(34, 4, 34, 6);
        if (RF_RSSI_8 >= 4) u8g2.drawLine(36, 3, 36, 6);
        if (RF_RSSI_8 >= 5) u8g2.drawLine(38, 2, 38, 6);
        if (RF_RSSI_8 >= 6) u8g2.drawLine(40, 1, 40, 6);
        if (RF_RSSI_8 >= 7) u8g2.drawLine(42, 6, 42, 0);
        if (RF_RSSI_8 >= 8) u8g2.drawXBM(30, 0, 3, 3, icon_rssi_max);
      }
    }
  }
  
  if (DISP_PAGE_A == 100) {
    u8g2.setFont(NokiaSmallPlain);
    u8g2.setCursor(64, 17);
    u8g2.print(DISP_PAGE_A);
    u8g2.print("-");
    u8g2.print(DISP_PAGE_B);
  }

  if (DISP_PAGE_A == 1) {                   //main menu
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(16, 7, "Main Menu");
    u8g2.setFont(NokiaSmallPlain);
    u8g2.drawStr(1, 17, "Messaging");
    u8g2.drawStr(1, 27, "Inbox");
    u8g2.drawStr(1, 37, "Settings");
    u8g2.drawStr(1, 47, "Info");
    u8g2.setDrawColor(2);
    if (DISP_PAGE_B == 0) u8g2.drawBox(0, 9, 84, 10);
    if (DISP_PAGE_B == 3) u8g2.drawBox(0, 19, 84, 10);
    if (DISP_PAGE_B == 2) u8g2.drawBox(0, 29, 84, 10);
    if (DISP_PAGE_B == 1) u8g2.drawBox(0, 39, 84, 9);
    u8g2.setDrawColor(1);
  }
  if (DISP_PAGE_A == 10) {                   //messaging submenu
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(17, 7, "Messaging");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.drawStr(5, 28, "(work in progress)");
  }
  if (DISP_PAGE_A == 11) {                    //inbox submenu
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(29, 7, "Inbox");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.drawStr(5, 28, "(work in progress)");
  }
  if (DISP_PAGE_A == 12) {                   //settings submenu
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(23, 7, "Settings");
    
    u8g2.setFont(NokiaSmallPlain);
    u8g2.drawStr(1, 17, "Display");
    u8g2.drawStr(1, 27, "Keyboard");
    u8g2.drawStr(1, 37, "Radio");
    u8g2.drawStr(1, 47, "Debug");
    u8g2.setDrawColor(2);
    if (DISP_PAGE_B == 0) u8g2.drawBox(0, 9, 84, 10);
    if (DISP_PAGE_B == 3) u8g2.drawBox(0, 19, 84, 10);
    if (DISP_PAGE_B == 2) u8g2.drawBox(0, 29, 84, 10);
    if (DISP_PAGE_B == 1) u8g2.drawBox(0, 39, 84, 9);
    u8g2.setDrawColor(1);
  }
  if (DISP_PAGE_A == 13) {                   //info submenu
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(33, 7, "Info");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.drawStr(1, 15, "3310 smeeg fw v0.4");
    u8g2.drawStr(1, 23, "based on sp5wwp's");
    u8g2.drawStr(1, 31, "M17_3310-fw.  Also");
    u8g2.drawStr(1, 39, "Implements spwwp's");
    u8g2.drawStr(1, 47, "T9 typing library");
  }
  if (DISP_PAGE_A == 20) {                                              //display
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(25, 7, "Display");
    u8g2.setFont(NokiaSmallPlain);
    u8g2.drawStr(1, 17, "Contrast");
    u8g2.drawStr(1, 27, "Backlight");
    u8g2.drawStr(1, 37, "Mode");
    u8g2.setDrawColor(2);
    if (DISP_PAGE_B == 0) u8g2.drawBox(0, 9, 84, 10);
    if (DISP_PAGE_B == 2) u8g2.drawBox(0, 19, 84, 10);
    if (DISP_PAGE_B == 1) u8g2.drawBox(0, 29, 84, 10);
    u8g2.setDrawColor(1);
  }
  if (DISP_PAGE_A == 21) {       //keyboard               
    
    if (DISP_PAGE_B == 1) {
      DISP_SCROLL_OFFSET = 20;
    } else if (DISP_PAGE_B == 2) {
      DISP_SCROLL_OFFSET = 10;
    } else {
      DISP_SCROLL_OFFSET = 0;
    }


    u8g2.setFont(NokiaSmallPlain);
    u8g2.drawStr(1, (17 - DISP_SCROLL_OFFSET), "Lights");
    u8g2.drawStr(1, (27 - DISP_SCROLL_OFFSET), "Feedback");
    u8g2.drawStr(1, (37 - DISP_SCROLL_OFFSET), "Debouncing");
    u8g2.drawStr(1, (47 - DISP_SCROLL_OFFSET), "Double tap");
    u8g2.drawStr(1, (57 - DISP_SCROLL_OFFSET), "Hold repeat");
    u8g2.drawStr(1, (67 - DISP_SCROLL_OFFSET), "Typing");

    u8g2.drawBox(0, 0, 84, 9);      //draw two boxes on top of each other to clear out anything that may be scrolled into this space
    u8g2.setDrawColor(2);
    u8g2.drawBox(0, 0, 84, 9);      //one is normal, the one on top is inverted, to create a box of "clear"
    if (DISP_PAGE_B == 0) u8g2.drawBox(0, 9, 84, 10);
    if (DISP_PAGE_B == 5) u8g2.drawBox(0, 19, 84, 10);
    if (DISP_PAGE_B == 4) u8g2.drawBox(0, 29, 84, 10);
    if (DISP_PAGE_B == 3) u8g2.drawBox(0, 39, 84, 10);
    if (DISP_PAGE_B == 2) u8g2.drawBox(0, (49 - DISP_SCROLL_OFFSET), 84, 10);
    if (DISP_PAGE_B == 1) u8g2.drawBox(0, (59 - DISP_SCROLL_OFFSET), 84, 10);
    u8g2.setDrawColor(1);
    
    if (DISP_PAGE_B == 1) {
      u8g2.drawXBM(73, 0, 5, 3, icon_arrow_up);
    } else if (DISP_PAGE_B == 2) {
      u8g2.drawXBM(73, 5, 5, 3, icon_arrow_down);
      u8g2.drawXBM(73, 0, 5, 3, icon_arrow_up);
    } else {
      u8g2.drawXBM(73, 5, 5, 3, icon_arrow_down);
    }

    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(18, 7, "Keyboard");
  }

  if (DISP_PAGE_A == 22) {                                                   //radio
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(29, 7, "Radio");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.drawStr(5, 28, "(work in progress)");
    
  }


  if (DISP_PAGE_A == 23) {                                      //debug
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.setCursor(27, 7);
    u8g2.print("Debug");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.setCursor(1, 17);
    
    
    //u8g2.print(DISP_MESSAGE_BUFFER);
    DISP_MESSAGE_LENGTH = (u8g2.getUTF8Width(String(DISP_MESSAGE_BUFFER).c_str()) + 1);
    u8g2.setCursor (0, 15);

      int start = 0;
      int end = 0;
      int wordcount = 0;
      int wordcount2[16] = {0};
      int wordlength = 0;
      int cursor_y = 15;
      int cursor_x = 0;
      int a = 0;
      const char* t9word = "";

    while(DISP_MESSAGE_BUFFER[start] != '\0') {

      end = start;

        while (DISP_MESSAGE_BUFFER[end] != ' ' && DISP_MESSAGE_BUFFER[end] != '\0') {

          ++end;
          
        }
      int length = end - start;
      strncpy(DISP_INTERNAL_BUFFER_2, DISP_MESSAGE_BUFFER + start, length);
      //DISP_INTERNAL_BUFFER_2[length] = ' ';
      DISP_INTERNAL_BUFFER_2[length + 0] = '\0';

      int wordwidth = u8g2.getUTF8Width(DISP_INTERNAL_BUFFER_2);
      int spacewidth = u8g2.getUTF8Width(" ");

      if ((cursor_x + wordwidth) > 84) {
        cursor_x = 0;
        cursor_y += 8;
      }
      if ((cursor_x + wordwidth + strlen(getWord(dict_en, DISP_TEXT_BUFFER))) > 72) {
        a = 1;
      } else {
        a = 0;
      }


      u8g2.setCursor(cursor_x, (cursor_y - DISP_SCROLL_OFFSET));
      u8g2.print(DISP_INTERNAL_BUFFER_2);
      cursor_x += wordwidth;

      if (DISP_MESSAGE_BUFFER[end] == ' ') {
        u8g2.setCursor(cursor_x, (cursor_y - DISP_SCROLL_OFFSET));
        u8g2.print(" ");
        cursor_x += spacewidth;
        end++;
      }

      start = end;
    }

    if (cursor_y >= 48) {
      DISP_SCROLL_OFFSET = cursor_y - 48;
    }

    if ((KB_BUFFER >= '1' && KB_BUFFER <= '9' || KB_BUFFER == '*') && KB_BEEN_READ == false) {
      DISP_TEXT_BUFFER[DISP_TEXT_BUFFER_INDEX] = KB_BUFFER;
      DISP_TEXT_BUFFER_INDEX++;
      DISP_TEXT_BUFFER[DISP_TEXT_BUFFER_INDEX] = '\0';
    }
    if (DISP_TEXT_BUFFER_INDEX == 16) {
      DISP_TEXT_BUFFER_INDEX = 0;
    }

    u8g2.drawBox(0, 0, 84, 8);
    u8g2.setDrawColor(2);
    u8g2.drawBox(0, 0, 84, 8);
    u8g2.setDrawColor(1);
    u8g2.drawLine(0, 7, 83, 7);
/*

        
      wordcount2[wordcount] = (u8g2.getUTF8Width(DISP_INTERNAL_BUFFER_2) - 0);
      wordcount++;

      if (DISP_MESSAGE_BUFFER[end] == ' ') {
        end++;
      }
      start = end;

      wordlen = 0; 
      for ( int i = 0; i < wordcount; i++ ) wordlen += wordcount2[ i ];
      wordlen = (wordlen - 3);
        
      if (wordlen > 84) {
        u8g2.setCursor(0, (cursor_offset + 8));

      }
      u8g2.print(DISP_INTERNAL_BUFFER_2);
    }

    for ( int i = 0; i < wordcount; i++ ) wordlen += wordcount2[ i ];
*/
/*
    DISP_MESSAGE_PRINT_X = 1;
    DISP_MESSAGE_PRINT_Y = 27;
    DISP_INTERNAL_BUFFER_2[1] = '\0';
    for (int i = 0; i <= 10; i++) {
      DISP_INTERNAL_BUFFER_2[0] = DISP_MESSAGE_BUFFER[i];
      if (DISP_MESSAGE_PRINT_X + (u8g2.getUTF8Width(String(DISP_INTERNAL_BUFFER_2).c_str()) + 1) > 82) {
        DISP_MESSAGE_PRINT_X = 0;
        DISP_MESSAGE_PRINT_Y += 10;
      }
      u8g2.setCursor(DISP_MESSAGE_PRINT_X, DISP_MESSAGE_PRINT_Y);
      u8g2.print(DISP_INTERNAL_BUFFER_2);
      DISP_MESSAGE_PRINT_X += (u8g2.getUTF8Width(DISP_INTERNAL_BUFFER_2) + 1);
    }

    for (int i = 0; i < strlen(DISP_MESSAGE_BUFFER); i++) {

     if (u8g2.getUTF8Width(DISP_MESSAGE_BUFFER) > 82) {
      DISP_INTERNAL_BUFFER_2 = DISP_MESSAGE_BUFFER + 20;
      u8g2.drawStr(1, 33, offsetText);
     } 
    }

    for (int i = 0; i < strlen(DISP_MESSAGE_BUFFER); i++) {
      DISP_INTERNAL_BUFFER_2[0] = DISP_MESSAGE_BUFFER[i];
      DISP_INTERNAL_BUFFER_2[1] = '\0';
      DISP_CENTER_POINT = (DISP_CENTER_POINT + u8g2.getUTF8Width(DISP_INTERNAL_BUFFER_2));

      if (DISP_CENTER_POINT > 82) {
        u8g2.setCursor(DISP_CENTER_POINT, 33);
      } else {
        u8g2.setCursor(DISP_CENTER_POINT, 23);
      }
      
      u8g2.print(DISP_INTERNAL_BUFFER_2);
    }
*/  
    
    if (a == 1) u8g2.setCursor(37, 7);

    if (KB_TAP_OR_T9 == 1) {

    u8g2.setFont(u8g2_font_nokiafc22_tr);
    if (KB_BUFFER >= '1' && KB_BUFFER <= '9') {
      u8g2.print(KB_TAPMAP[KB_BUFFER - '0'][KB_DOUBLE_PRESS_EVENT_COUNT - 1]);
      DISP_INTERNAL_BUFFER[0] = (KB_TAPMAP[KB_BUFFER - '0'][KB_DOUBLE_PRESS_EVENT_COUNT - 1]);
      
    }
    if (KB_BUFFER == '0') {
      u8g2.print(">");
      DISP_INTERNAL_BUFFER[0] = ' ';
    }
    if (KB_BEEN_READ == false && KB_BUFFER == '#') {
      DISP_MESSAGE_BUFFER[DISP_MESSAGE_BUFFER_INDEX] = DISP_INTERNAL_BUFFER[0];
      DISP_MESSAGE_BUFFER_INDEX++;
      INTERNAL_FLAG_1++;
      DISP_MESSAGE_BUFFER[DISP_MESSAGE_BUFFER_INDEX] = '\0';
    }

    if ((KB_TAPMAP[KB_BUFFER - '0'][KB_DOUBLE_PRESS_EVENT_COUNT - 1]) == '\0') KB_DOUBLE_PRESS_EVENT_COUNT = 0;
    
    if (KB_BUFFER == '0' && KB_BEEN_READ == false) {
      DISP_TEXT_BUFFER[0] = '\0';
      DISP_TEXT_BUFFER_INDEX = 0;
    }
    u8g2.drawStr(69, 7, "tap");

    } else {
      u8g2.setFont(u8g2_font_nokiafc22_tr);
      t9word = getWord(dict_en, DISP_TEXT_BUFFER);
      wordlength = strlen(t9word);
      u8g2.print(t9word);
      u8g2.drawStr(72, 7, "T9");
      if (KB_BEEN_READ == false && KB_BUFFER == '0') {
        strncpy(&DISP_MESSAGE_BUFFER[DISP_MESSAGE_BUFFER_INDEX], t9word, wordlength);
        DISP_MESSAGE_BUFFER_INDEX += wordlength;
        DISP_MESSAGE_BUFFER[DISP_MESSAGE_BUFFER_INDEX] = ' ';
        DISP_MESSAGE_BUFFER_INDEX++;
        DISP_MESSAGE_BUFFER[DISP_MESSAGE_BUFFER_INDEX] = '\0';
        DISP_TEXT_BUFFER[0] = '\0';
        DISP_TEXT_BUFFER_INDEX = 0;
      }

      if (KB_BUFFER == '#' && KB_BEEN_READ == false) {
        DISP_TEXT_BUFFER[0] = '\0';
        DISP_TEXT_BUFFER_INDEX = 0;
      }
    }


    u8g2.setCursor(0, 7);
    u8g2.print(DISP_MESSAGE_BUFFER_INDEX);
    u8g2.print("/820");

    

    /*
    u8g2.setCursor(1, 37);
    u8g2.print(".");
    u8g2.print(wordlen);
    u8g2.print(".");
    u8g2.print(DISP_MESSAGE_BUFFER_INDEX);
    u8g2.print(INTERNAL_FLAG_1);

    u8g2.print(u8g2.getUTF8Width(DISP_MESSAGE_BUFFER));

    u8g2.setCursor(u8g2.getUTF8Width(DISP_MESSAGE_BUFFER), 17);
    u8g2.print(".");
    */
  }



  if (DISP_PAGE_C == 1) {                                                                 //contrast setting
    u8g2.clearBuffer();                                                                   //to stop the menu from drawing below this menu, inefficient, i know
    DISP_CONTRAST = DISP_PAGE_B;
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(21, 7, "Contrast");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.drawStr(11, 15, "sets the display");
    u8g2.drawStr(2, 23, "contrast / darkness");
    u8g2.drawStr(27, 32, "(0-255)");
    u8g2.setCursor(68, 38);
    u8g2.print(DISP_CONTRAST);
    u8g2.drawFrame(0, 40, 84, 8);
    u8g2.drawBox(1, 41, ((DISP_CONTRAST * 82) / 255), 6);
    u8g2.setContrast(DISP_CONTRAST);
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false) EEPROM.put(1, DISP_CONTRAST);
  }


  if (DISP_PAGE_C == 2) {                                                                 //backlight setting
    if (KB_BUFFER == 'E' && KB_BEEN_READ == false) {                                      //doing these functions again for this submenu because it has toggles AND options
      if(DISP_PAGE_D == 0) {
        if (BACKLIGHT_ENABLE >= 1) {
          BACKLIGHT_ENABLE = 0;
        } else {
          BACKLIGHT_ENABLE = 1;
        }
      }
      if(DISP_PAGE_D == 3) {
        if (BACKLIGHT_ENABLE == 2) {
          BACKLIGHT_ENABLE = 1;
        } else {
          BACKLIGHT_ENABLE = 2;
        }
      }
      if(DISP_PAGE_D == 2) {
        INTERNAL_FLAG_1 = 1;
        DISP_PAGE_B = BACKLIGHT_PWM;
        DISP_NUM_OPTIONS = 255;
      }
      if(DISP_PAGE_D == 1) {
        INTERNAL_FLAG_1 = 2;
        DISP_PAGE_B = BACKLIGHT_FREQ;
        DISP_NUM_OPTIONS = 255;
      }
      KB_BEEN_READ = true;
    }
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false) {
      if (INTERNAL_FLAG_1 == 1) {
        DISP_PAGE_B = 2;
        KB_BEEN_READ = true;
      }
      if (INTERNAL_FLAG_1 == 2) {
        DISP_PAGE_B = 1;
        KB_BEEN_READ = true;
      }
      DISP_NUM_OPTIONS = 3;
      INTERNAL_FLAG_1 = 0;
    }
    
    if (INTERNAL_FLAG_1 == 0) {
      DISP_PAGE_D = DISP_PAGE_B;
    }
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(20, 8, "Backlight");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    if (DISP_PAGE_D == 1) {
      DISP_SCROLL_OFFSET = 10;
    } else {
      DISP_SCROLL_OFFSET = 0;
    }
    if (DISP_PAGE_D != 1) u8g2.drawStr(1, 17, "Backlight on/off");
    u8g2.drawStr(1, (27 - DISP_SCROLL_OFFSET), "Backlight always");
    u8g2.drawStr(1, (37 - DISP_SCROLL_OFFSET), "Backlight PWM");
    if (DISP_PAGE_D == 1) {
      u8g2.drawStr(1, 37, "Backlight Freq");
      sprintf(DISP_INTERNAL_BUFFER, "%d", (BACKLIGHT_FREQ * 20));
      DISP_CENTER_POINT = u8g2_GetStrWidth(u8g2.getU8g2(), DISP_INTERNAL_BUFFER);
      u8g2.setCursor(73 - (DISP_CENTER_POINT / 2), 37);
      u8g2.print(BACKLIGHT_FREQ * 20);
    }
    sprintf(DISP_INTERNAL_BUFFER, "%d", BACKLIGHT_PWM);
    DISP_CENTER_POINT = u8g2_GetStrWidth(u8g2.getU8g2(), DISP_INTERNAL_BUFFER);
    u8g2.setCursor(73 - (DISP_CENTER_POINT / 2), (37 - DISP_SCROLL_OFFSET));
    u8g2.print(BACKLIGHT_PWM);
    u8g2.setDrawColor(2);
    if (INTERNAL_FLAG_1 == 1) {
      u8g2.drawBox(63, 29, 21, 9);
      BACKLIGHT_PWM = DISP_PAGE_B;
    } else if (INTERNAL_FLAG_1 == 2) {
      u8g2.drawBox(63, 29, 21, 9);
      BACKLIGHT_FREQ = DISP_PAGE_B;
    } else {
      if (DISP_PAGE_D == 0) u8g2.drawBox(0, 9, 67, 10);
      if (DISP_PAGE_D == 3) u8g2.drawBox(0, 19, 67, 10);
      if (DISP_PAGE_D == 2) u8g2.drawBox(0, 29, 63, 10);
      if (DISP_PAGE_D == 1) u8g2.drawBox(0, 29, 63, 10);
    }
    u8g2.setDrawColor(1);

    if (DISP_PAGE_D == 1) {
      u8g2.drawXBM(73, 0, 5, 3, icon_arrow_up);
    } else {
      u8g2.drawXBM(73, 5, 5, 3, icon_arrow_down);
    }
    
    if (DISP_PAGE_D != 1) {
      if (BACKLIGHT_ENABLE >= 1) {
        u8g2.drawStr(73, 17, "X");
      }
      u8g2.drawFrame(71, 10, 9, 7);
    }
    if (BACKLIGHT_ENABLE == 2) {
      u8g2.drawStr(73, (27 - DISP_SCROLL_OFFSET), "X");
    }
    u8g2.drawFrame(71, (20 - DISP_SCROLL_OFFSET), 9, 7);
    u8g2.drawFrame(0, 40, 84, 8);
    if (DISP_PAGE_D == 1) {
      u8g2.drawBox(1, 41, ((BACKLIGHT_FREQ * 82) / 255), 6);
    } else {
      u8g2.drawBox(1, 41, ((BACKLIGHT_PWM * 82) / 255), 6);
    }

    if (BACKLIGHT_ENABLE >= 1) {
      analogWriteFrequency(BACKLIGHT_FREQ * 20);
      analogWrite(PIN_BACKLIGHT, BACKLIGHT_PWM);
    } else {
      analogWrite(PIN_BACKLIGHT, 0);
    }

    if (KB_BUFFER == 'B' && KB_BEEN_READ == false) EEPROM.put(2, BACKLIGHT_ENABLE), EEPROM.put(3, BACKLIGHT_PWM), EEPROM.put(4, BACKLIGHT_FREQ);
  }


  if (DISP_PAGE_C == 3) {                                                                 //mode setting

    if (KB_BUFFER == 'E' && KB_BEEN_READ == false) {
      if (INTERNAL_FLAG_1 >= 1) {
          INTERNAL_FLAG_1 = 0;
        } else {
          INTERNAL_FLAG_1 = 1;
        }
      KB_BEEN_READ = true;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(29, 7, "Mode");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.drawStr(1, 17, "Splash screen");
    u8g2.setDrawColor(2);
    u8g2.drawBox(0, 9, 67, 10);
    u8g2.setDrawColor(1);
    if (INTERNAL_FLAG_1 == 1) {
      u8g2.drawStr(73, 17, "X");
    }
    u8g2.drawFrame(71, 10, 9, 7);
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false) EEPROM.put(5, INTERNAL_FLAG_1);
  }

  if (DISP_PAGE_C == 4) {                             //keyboard lights

    if (KB_BUFFER == 'E' && KB_BEEN_READ == false) {
      if (DISP_PAGE_B == 0) {
        if (KB_LIGHT_ON_PRESS_ENABLE >= 1) {
          KB_LIGHT_ON_PRESS_ENABLE = 0;
        } else {
          KB_LIGHT_ON_PRESS_ENABLE = 1;
        }
      }
      if (DISP_PAGE_B == 1) {
        INTERNAL_FLAG_1 = 1;
        DISP_PAGE_B = KB_LIGHT_ON_PRESS;
        DISP_NUM_OPTIONS = 255;
      }
      KB_BEEN_READ = true;
    }
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false && INTERNAL_FLAG_1 == 1) {
      INTERNAL_FLAG_1 = 0;
      DISP_PAGE_B = 1;
      DISP_NUM_OPTIONS = 1;
      KB_BEEN_READ = true;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(28, 7, "Lights");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.drawStr(1, 17, "Light on press");
    u8g2.drawStr(1, 27, "Light time");
    sprintf(DISP_INTERNAL_BUFFER, "%lu.%03lus", (KB_LIGHT_ON_PRESS * 40) / 1000, (KB_LIGHT_ON_PRESS * 40) % 1000);
    DISP_CENTER_POINT = u8g2_GetStrWidth(u8g2.getU8g2(), DISP_INTERNAL_BUFFER);
    u8g2.setCursor(66 - (DISP_CENTER_POINT / 2), 27);
    u8g2.print(DISP_INTERNAL_BUFFER);
    if (KB_LIGHT_ON_PRESS_ENABLE == 1) u8g2.drawStr(73, 17, "X"); //check
    u8g2.drawFrame(71, 10, 9, 7); //checkbox
    u8g2.setDrawColor(2);
    if (INTERNAL_FLAG_1 == 1) {
      u8g2.drawBox(50, 19, 34, 10); //number
      KB_LIGHT_ON_PRESS = DISP_PAGE_B;
    } else {
    if (DISP_PAGE_B == 0) u8g2.drawBox(0, 9, 67, 10); //toggle
    if (DISP_PAGE_B == 1) u8g2.drawBox(0, 19, 50, 10);  //ms
    }
    u8g2.setDrawColor(1);
    u8g2.drawFrame(0, 40, 84, 8); //box
    u8g2.drawBox(1, 41, ((KB_LIGHT_ON_PRESS* 82) / 255), 6); //bar
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false) EEPROM.put(6, KB_LIGHT_ON_PRESS_ENABLE), EEPROM.put(7, KB_LIGHT_ON_PRESS);
  }

  if (DISP_PAGE_C == 5) {                               //keyboard vibration new version (mostly copied from backlight)

    if (KB_BUFFER == 'E' && KB_BEEN_READ == false) {
      if (DISP_PAGE_B == 0) {
        if (VIBRATION_ON_KEYPRESS_ENABLE >= 1) {
          VIBRATION_ON_KEYPRESS_ENABLE = 0;
          digitalWrite(PIN_VIBES, LOW);
        } else {
          VIBRATION_ON_KEYPRESS_ENABLE = 1;
        }
      }
      if (DISP_PAGE_B == 1) {
        INTERNAL_FLAG_1 = 1;
        DISP_PAGE_B = VIBRATION_ON_KEYPRESS;
        DISP_NUM_OPTIONS = 255;
      }
      KB_BEEN_READ = true;
    }
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false && INTERNAL_FLAG_1 == 1) {
      INTERNAL_FLAG_1 = 0;
      DISP_PAGE_B = 1;
      DISP_NUM_OPTIONS = 1;
      KB_BEEN_READ = true;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(19, 7, "Feedback");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.drawStr(1, 17, "Keypress \"Vibes\"");
    u8g2.drawStr(1, 27, "Vibe duration");
    sprintf(DISP_INTERNAL_BUFFER, "%d", VIBRATION_ON_KEYPRESS);
    DISP_CENTER_POINT = u8g2_GetStrWidth(u8g2.getU8g2(), DISP_INTERNAL_BUFFER);
    u8g2.setCursor(76 - (DISP_CENTER_POINT / 2), 27);
    u8g2.print(DISP_INTERNAL_BUFFER);
    if (VIBRATION_ON_KEYPRESS_ENABLE == 1) u8g2.drawStr(75, 17, "X"); //check
    u8g2.drawFrame(73, 10, 9, 7); //checkbox
    u8g2.setDrawColor(2);
    if (INTERNAL_FLAG_1 == 1) {
      u8g2.drawBox(68, 19, 16, 10); //number
      VIBRATION_ON_KEYPRESS = DISP_PAGE_B;
    } else {
    if (DISP_PAGE_B == 0) u8g2.drawBox(0, 9, 71, 10); //toggle
    if (DISP_PAGE_B == 1) u8g2.drawBox(0, 19, 68, 10);  //ms
    }
    u8g2.setDrawColor(1);
    u8g2.drawFrame(0, 40, 84, 8); //box
    u8g2.drawBox(1, 41, ((VIBRATION_ON_KEYPRESS * 82) / 255), 6); //bar
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false) digitalWrite(PIN_VIBES, LOW), EEPROM.put(8, VIBRATION_ON_KEYPRESS_ENABLE), EEPROM.put(9, VIBRATION_ON_KEYPRESS);
  }

  if (DISP_PAGE_C == 6) {                               //debouncing

    if (KB_BUFFER == 'E' && KB_BEEN_READ == false) {
      if (DISP_PAGE_B == 0) {
        if (KB_DEBOUNCING_ACTIVE >= 1) {
          KB_DEBOUNCING_ACTIVE = 0;
        } else {
          KB_DEBOUNCING_ACTIVE = 1;
        }
      }
      if (DISP_PAGE_B == 1) {
        INTERNAL_FLAG_1 = 1;
        DISP_PAGE_B = KB_DEBOUNCING_WINDOW;
        DISP_NUM_OPTIONS = 255;
      }
      KB_BEEN_READ = true;
    }
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false && INTERNAL_FLAG_1 == 1) {
      INTERNAL_FLAG_1 = 0;
      DISP_PAGE_B = 1;
      DISP_NUM_OPTIONS = 1;
      KB_BEEN_READ = true;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(14, 7, "Debouncing");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.drawStr(1, 17, "Debouncing on");
    u8g2.drawStr(1, 27, "Debounce time");
    sprintf(DISP_INTERNAL_BUFFER, "%d", KB_DEBOUNCING_WINDOW);
    DISP_CENTER_POINT = u8g2_GetStrWidth(u8g2.getU8g2(), DISP_INTERNAL_BUFFER);
    u8g2.setCursor(76 - (DISP_CENTER_POINT / 2), 27);
    u8g2.print(DISP_INTERNAL_BUFFER);
    if (KB_DEBOUNCING_ACTIVE == 1) u8g2.drawStr(75, 17, "X"); //check
    u8g2.drawFrame(73, 10, 9, 7); //checkbox
    u8g2.setDrawColor(2);
    if (INTERNAL_FLAG_1 == 1) {
      u8g2.drawBox(68, 19, 16, 10); //number
      KB_DEBOUNCING_WINDOW = DISP_PAGE_B;
    } else {
    if (DISP_PAGE_B == 0) u8g2.drawBox(0, 9, 71, 10); //toggle
    if (DISP_PAGE_B == 1) u8g2.drawBox(0, 19, 68, 10);  //ms
    }
    u8g2.setDrawColor(1);
    u8g2.drawFrame(0, 40, 84, 8); //box
    u8g2.drawBox(1, 41, ((KB_DEBOUNCING_WINDOW * 82) / 255), 6); //bar
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false) EEPROM.put(13, KB_DEBOUNCING_ACTIVE), EEPROM.put(14, KB_DEBOUNCING_WINDOW);
  }


  if (DISP_PAGE_C == 7) {                               //double press

    if (KB_BUFFER == 'E' && KB_BEEN_READ == false) {
      if (DISP_PAGE_B == 0) {
        if (KB_DOUBLE_PRESS_ENABLED >= 1) {
          KB_DOUBLE_PRESS_ENABLED = 0;
        } else {
          KB_DOUBLE_PRESS_ENABLED = 1;
        }
      }
      if (DISP_PAGE_B == 1) {
        INTERNAL_FLAG_1 = 1;
        DISP_PAGE_B = KB_DOUBLE_PRESS_TIME;
        DISP_NUM_OPTIONS = 255;
      }
      KB_BEEN_READ = true;
    }
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false && INTERNAL_FLAG_1 == 1) {
      INTERNAL_FLAG_1 = 0;
      DISP_PAGE_B = 1;
      DISP_NUM_OPTIONS = 1;
      KB_BEEN_READ = true;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(16, 7, "Double tap");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.drawStr(1, 17, "Enable / Disable");
    u8g2.drawStr(1, 27, "Time window");
    sprintf(DISP_INTERNAL_BUFFER, "%lu.%03lus", (KB_DOUBLE_PRESS_TIME * 40) / 1000, (KB_DOUBLE_PRESS_TIME * 40) % 1000);
    DISP_CENTER_POINT = u8g2_GetStrWidth(u8g2.getU8g2(), DISP_INTERNAL_BUFFER);
    u8g2.setCursor(70 - (DISP_CENTER_POINT / 2), 27);
    u8g2.print(DISP_INTERNAL_BUFFER);
    if (KB_DOUBLE_PRESS_ENABLED == 1) u8g2.drawStr(75, 17, "X"); //check
    u8g2.drawFrame(73, 10, 9, 7); //checkbox
    u8g2.setDrawColor(2);
    if (INTERNAL_FLAG_1 == 1) {
      u8g2.drawBox(58, 19, 26, 10); //number
      KB_DOUBLE_PRESS_TIME = DISP_PAGE_B;
    } else {
    if (DISP_PAGE_B == 0) u8g2.drawBox(0, 9, 71, 10); //toggle
    if (DISP_PAGE_B == 1) u8g2.drawBox(0, 19, 57, 10);  //ms
    }
    u8g2.setDrawColor(1);
    u8g2.drawFrame(0, 40, 84, 8); //box
    u8g2.drawBox(1, 41, ((KB_DOUBLE_PRESS_TIME * 82) / 255), 6); //bar
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false) EEPROM.put(15, KB_DOUBLE_PRESS_ENABLED), EEPROM.put(16, KB_DOUBLE_PRESS_TIME);
  }



  if (DISP_PAGE_C == 8) {                               //hold repeat

    if (KB_BUFFER == 'E' && KB_BEEN_READ == false) {
      if (DISP_PAGE_B == 0) {
        if (KB_HOLD_REPEAT >= 1) {
          KB_HOLD_REPEAT = 0;
        } else {
          KB_HOLD_REPEAT = 1;
        }
      }
      if (DISP_PAGE_B == 1) {
        INTERNAL_FLAG_1 = 1;
        DISP_PAGE_B = KB_HOLD_REPEAT_SPEED;
        DISP_NUM_OPTIONS = 255;
      }
      if (DISP_PAGE_B == 2) {
        INTERNAL_FLAG_1 = 2;
        DISP_PAGE_B = KB_HOLD_REPEAT_DELAY;
        DISP_NUM_OPTIONS = 255;
      }
      KB_BEEN_READ = true;
    }

    if (KB_BUFFER == 'B' && KB_BEEN_READ == false) {
      if (INTERNAL_FLAG_1 == 1) {
        INTERNAL_FLAG_1 = 0;
        DISP_PAGE_B = 1;
        DISP_NUM_OPTIONS = 2;
        KB_BEEN_READ = true;
      }
      if (INTERNAL_FLAG_1 == 2) {
        INTERNAL_FLAG_1 = 0;
        DISP_PAGE_B = 2;
        DISP_NUM_OPTIONS = 2;
        KB_BEEN_READ = true;
      }      
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    //u8g2.drawStr(13, 7, "Hold Repeat");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.drawStr(1, 17, "Enable / Disable");
    u8g2.drawStr(1, 27, "Hold delay");
    u8g2.drawStr(1, 37, "Repeat delay");
    sprintf(DISP_INTERNAL_BUFFER, "%lu.%03lus", (KB_HOLD_REPEAT_DELAY * 20) / 1000, (KB_HOLD_REPEAT_DELAY * 20) % 1000);
    DISP_CENTER_POINT = u8g2_GetStrWidth(u8g2.getU8g2(), DISP_INTERNAL_BUFFER);
    u8g2.setCursor(70 - (DISP_CENTER_POINT / 2), 27);
    u8g2.print(DISP_INTERNAL_BUFFER);
    sprintf(DISP_INTERNAL_BUFFER, "%dms", KB_HOLD_REPEAT_SPEED);
    DISP_CENTER_POINT = u8g2_GetStrWidth(u8g2.getU8g2(), DISP_INTERNAL_BUFFER);
    u8g2.setCursor(71 - (DISP_CENTER_POINT / 2), 37);
    u8g2.print(DISP_INTERNAL_BUFFER);
    if (KB_HOLD_REPEAT == 1) u8g2.drawStr(75, 17, "X"); //check
    u8g2.drawFrame(73, 10, 9, 7); //checkbox
    u8g2.setDrawColor(2);
    if (INTERNAL_FLAG_1 == 1) {
      u8g2.drawBox(58, 29, 26, 10); //number
      KB_HOLD_REPEAT_SPEED = DISP_PAGE_B;
    } else if (INTERNAL_FLAG_1 == 2) {
      u8g2.drawBox(57, 19, 27, 10);
      KB_HOLD_REPEAT_DELAY = DISP_PAGE_B;
    } else {
    if (DISP_PAGE_B == 0) u8g2.drawBox(0, 9, 71, 10); //toggle
    if (DISP_PAGE_B == 2) u8g2.drawBox(0, 19, 57, 10);  //hd
    if (DISP_PAGE_B == 1) u8g2.drawBox(0, 29, 58, 10);  //rd
    }
    u8g2.setDrawColor(1);
    u8g2.drawFrame(0, 40, 84, 8); //box
    u8g2.drawBox(1, 41, ((KB_HOLD_REPEAT_DELAY * 82) / 255), 6); //bar
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false) EEPROM.put(17, KB_HOLD_REPEAT), EEPROM.put(28, KB_HOLD_REPEAT_DELAY), EEPROM.put(19, KB_HOLD_REPEAT_SPEED);

    u8g2.setCursor(1, 7);
    u8g2.print(DISP_PAGE_B);
    u8g2.print(".");
    u8g2.print(INTERNAL_FLAG_1);
    u8g2.print(".");
    u8g2.print(KB_HOLD_REPEAT);
    u8g2.print(".");
    u8g2.print(KB_HOLD_REPEAT_SPEED);
    u8g2.print(".");
    u8g2.print(KB_HOLD_REPEAT_DELAY);
    u8g2.print(".");
    EEPROM.get(18, BATTERY_CHARGING);
    u8g2.print(BATTERY_CHARGING);
  }

  if (DISP_PAGE_C == 9) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(21, 7, "keyboard");
    u8g2.setCursor(0, 20);
    u8g2.print("entewr to toggle");
    u8g2.setCursor(0, 30);
    if (KB_TAP_OR_T9 == 1) u8g2.print("gui wip, am sleepy");
    if (KB_BUFFER == 'E' && KB_BEEN_READ == false) {
      if (DISP_PAGE_B == 0) {
        if (KB_TAP_OR_T9 >= 1) {
          KB_TAP_OR_T9 = 0;
        } else {
          KB_TAP_OR_T9 = 1;
        }
      }
      KB_BEEN_READ = true;
    }
    u8g2.setCursor(0, 40);
    u8g2.print("affects debug menu");
  }

/*
  if (DISP_PAGE_C == 5) {                               //keyboard vibration old version to steal parts from
    if (KB_BUFFER == 'E' && KB_BEEN_READ == false) {
      if (INTERNAL_FLAG_1 >= 1) {
          INTERNAL_FLAG_1 = 0;
        } else {
          INTERNAL_FLAG_1 = 1;
        }
      KB_BEEN_READ = true;
    }
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_nokiafc22_tr);
    u8g2.drawStr(19, 7, "Vibration");
    u8g2.setFont(u8g2_font_NokiaSmallPlain_tr);
    u8g2.drawStr(1, 17, "\"Vibes\" on keypress");
    u8g2.setDrawColor(2);
    u8g2.drawBox(0, 9, 84, 10);
    u8g2.drawStr(30, 31, "off on");
    u8g2.setDrawColor(1);
    if (INTERNAL_FLAG_1 == 1) {
      u8g2.drawBox(41, 32, 9, 7);
      u8g2.drawFrame(33, 32, 9, 7);
    } else {
      u8g2.drawBox(33, 32, 9, 7);
      u8g2.drawFrame(41, 32, 9, 7);
    }
    VIBRATION_ON_KEYPRESS_ENABLE = INTERNAL_FLAG_1;
    if (KB_BUFFER == 'B' && KB_BEEN_READ == false) EEPROM.put(8, INTERNAL_FLAG_1);
  }
*/
  

  
  
  u8g2.sendBuffer();
  
  if (KB_BUFFER == 'E' && KB_BEEN_READ == false) {
    if (DISP_PAGE_A == 0) {
      DISP_PAGE_A = 1;
      DISP_PAGE_B = 0;
      DISP_NUM_OPTIONS = 3;
    } else if (DISP_PAGE_A == 1) {
      if (DISP_PAGE_B == 0) DISP_PAGE_A = 10;  //messaging
      if (DISP_PAGE_B == 3) DISP_PAGE_A = 11;  //inbox
      if (DISP_PAGE_B == 2) DISP_PAGE_A = 12;  //settings
      if (DISP_PAGE_B == 1) DISP_PAGE_A = 13;  //info
      DISP_PAGE_B = 0;
    } else if (DISP_PAGE_A == 12) {
      if (DISP_PAGE_B == 0) DISP_PAGE_A = 20, DISP_NUM_OPTIONS = 2;  //display
      if (DISP_PAGE_B == 3) DISP_PAGE_A = 21, DISP_NUM_OPTIONS = 5;  //keyboard
      if (DISP_PAGE_B == 2) DISP_PAGE_A = 22;                        //radio
      if (DISP_PAGE_B == 1) DISP_PAGE_A = 23, DISP_TEXT_BUFFER[0] = '\0', DISP_TEXT_BUFFER_INDEX = 0, DISP_SCROLL_OFFSET = 0;                        //debug
      DISP_PAGE_B = 0;
    } else if (DISP_PAGE_A == 20) {
      if (DISP_PAGE_B == 0) DISP_PAGE_C = 1, DISP_NUM_OPTIONS = 255, DISP_PAGE_B = DISP_CONTRAST;                            //contrast
      if (DISP_PAGE_B == 2) DISP_PAGE_C = 2, DISP_NUM_OPTIONS = 3, DISP_PAGE_B = 0, INTERNAL_FLAG_1 = 0;                     //backlight
      if (DISP_PAGE_B == 1) DISP_PAGE_C = 3, DISP_NUM_OPTIONS = 0, EEPROM.get(5, INTERNAL_FLAG_1);          //mode, i should think of a better name
    } else if (DISP_PAGE_A == 21) {
      if (DISP_PAGE_B == 0) DISP_PAGE_C = 4, DISP_NUM_OPTIONS = 1, INTERNAL_FLAG_1 = 0;  //Lights
      if (DISP_PAGE_B == 5) DISP_PAGE_C = 5, DISP_PAGE_B = 0, DISP_NUM_OPTIONS = 1, INTERNAL_FLAG_1 = 0;  //Vibration
      if (DISP_PAGE_B == 4) DISP_PAGE_C = 6, DISP_PAGE_B = 0, DISP_NUM_OPTIONS = 1, INTERNAL_FLAG_1 = 0;  //Debouncing
      if (DISP_PAGE_B == 3) DISP_PAGE_C = 7, DISP_PAGE_B = 0, DISP_NUM_OPTIONS = 1, INTERNAL_FLAG_1 = 0;  //Double tap
      if (DISP_PAGE_B == 2) DISP_PAGE_C = 8, DISP_PAGE_B = 0, DISP_NUM_OPTIONS = 2, INTERNAL_FLAG_1 = 0;  //hold repeat
      if (DISP_PAGE_B == 1) DISP_PAGE_C = 9, DISP_PAGE_B = 0, DISP_NUM_OPTIONS = 1, INTERNAL_FLAG_1 = 0;  //typing
    }
  }
  
  if (KB_BUFFER == 'B' && KB_BEEN_READ == false) {
    if (DISP_PAGE_C == 0) {
      if (DISP_PAGE_A == 1) DISP_PAGE_A = 0, DISP_PAGE_B = 0, DISP_NUM_OPTIONS = 1;
      if (DISP_PAGE_A >= 10 && DISP_PAGE_A <= 19) DISP_PAGE_A = 1, DISP_PAGE_B = 0, DISP_NUM_OPTIONS = 3;
      if (DISP_PAGE_A >= 20 && DISP_PAGE_A <= 29) DISP_PAGE_A = 12, DISP_PAGE_B = 0, DISP_NUM_OPTIONS = 3;
    }
    if (DISP_PAGE_C == 1) DISP_PAGE_C = 0, DISP_PAGE_B = 0, DISP_NUM_OPTIONS = 2;   //contrast
    if (DISP_PAGE_C == 2) DISP_PAGE_C = 0, DISP_PAGE_B = 2, DISP_NUM_OPTIONS = 2;   //backlight
    if (DISP_PAGE_C == 3) DISP_PAGE_C = 0, DISP_PAGE_B = 1, DISP_NUM_OPTIONS = 2;   //mode
    if (DISP_PAGE_C == 4) DISP_PAGE_C = 0, DISP_PAGE_B = 0, DISP_NUM_OPTIONS = 5;   //(kb)lights
    if (DISP_PAGE_C == 5) DISP_PAGE_C = 0, DISP_PAGE_B = 5, DISP_NUM_OPTIONS = 5;   //vibration
    if (DISP_PAGE_C == 6) DISP_PAGE_C = 0, DISP_PAGE_B = 4, DISP_NUM_OPTIONS = 5;   //debounce
    if (DISP_PAGE_C == 7) DISP_PAGE_C = 0, DISP_PAGE_B = 3, DISP_NUM_OPTIONS = 5;   //double tap
    if (DISP_PAGE_C == 8) DISP_PAGE_C = 0, DISP_PAGE_B = 2, DISP_NUM_OPTIONS = 5;   //hold repeat
    if (DISP_PAGE_C == 9) DISP_PAGE_C = 0, DISP_PAGE_B = 1, DISP_NUM_OPTIONS = 5;   //typing
  }

  if (KB_BUFFER == 'U' && KB_BEEN_READ == false && DISP_NUM_OPTIONS != 0) {
    if (DISP_PAGE_B >= DISP_NUM_OPTIONS) {
      DISP_PAGE_B = 0;
    } else {
      DISP_PAGE_B++;
    }
  }
  
  if (KB_BUFFER == 'D' && KB_BEEN_READ == false && DISP_NUM_OPTIONS != 0) {
    if (DISP_PAGE_B == 0) {
      DISP_PAGE_B = DISP_NUM_OPTIONS;
    } else {
      DISP_PAGE_B--; 
    }
  }

  KB_BEEN_READ = true;
}

void kb_scan() {
  KB_IS_ANYTHING_PRESSED = false;                     //will be triggered if any key was held, donno if this is still needed but it works for now
  KB_KEY_MATCHING = false;                            //will be triggered if the same key has been held


  for (uint8_t col = 0; col < KB_NUM_COL; col++) {   //increment through all col pins, set as high one at a time, set as low again once row check has finished
    digitalWrite(KB_COL_PINS[col], HIGH);

    for (uint8_t row = 0; row < KB_NUM_ROW; row++) {       //check each row pin and see if a voltage is present
      
      KB_KEYSTATE[row][col] = digitalRead(KB_ROW_PINS[row]) == HIGH;    //the checkening
      
      if (KB_KEYSTATE[row][col]) {             //if the current part of the array is true
        
        KB_IS_ANYTHING_PRESSED = true;
        if (KB_BUFFER == KB_KEYMAP[row][col]) {    //if the keyboard buffer and the currently pressed key match
          KB_KEY_MATCHING = true;                  //flag the rest of the code to update the hold duration
        }

        KB_BUFFER_PREVIOUS = KB_BUFFER;
        KB_BUFFER = KB_KEYMAP[row][col];            //set to the buffer the corresponding key in the keymap
      }
    }

    digitalWrite(KB_COL_PINS[col], LOW);                   //the setting of low in question
  }

  if (digitalRead(KB_BUTTON_ENTER) == HIGH) {              //if enter key is pressed
    KB_IS_ANYTHING_PRESSED = true;                         //flag something has been pressed
    
    if (KB_BUFFER == 'E') {                                //if its the same key that was seen last time
      KB_KEY_MATCHING = true;                              //flag that this is a hold
    }
    
    KB_BUFFER_PREVIOUS = KB_BUFFER;                        //store the last pressed key into the buffer for double press detection
    KB_BUFFER = 'E';                                       //write E (enter) to buffer
  }

  if (KB_IS_ANYTHING_PRESSED == true && KB_HOLDSTATE == false) {        //leading edge detection of keypress
    if (KB_BUFFER == KB_BUFFER_PREVIOUS) {                              //if the last key pressed was the same key (this branch is called if there was a release inbetween)
      if (millis() - KB_LAST_PRESSED > KB_DEBOUNCING_WINDOW || KB_DEBOUNCING_ACTIVE == false) { //ignore the button press if its been under 5ms since the last press, as an attempt to catch button bounce
  
        KB_BEEN_READ = false;                                           //set the "has been read" flag to false, if working correctly this should only be set once per keypress for psudo debouncing
        
        if (millis() - KB_LAST_PRESSED > (KB_DOUBLE_PRESS_TIME * 40)) {        //if this is a new double press, but after the double press time has expired
         KB_DOUBLE_PRESS_EVENT_COUNT = 1;                               //reset the count
        }
        if (millis() - KB_LAST_PRESSED <= (KB_DOUBLE_PRESS_TIME * 40)) {       //check how long ago the key was pressed, if it was less than the value of kb double press time
          KB_DOUBLE_PRESS_EVENT_COUNT++;                                //increment the counter of how many taps have been done
        }
      }
    }
    KB_LAST_PRESSED = millis();                                         //if this is a new key but not the same key, reset the clock
  }
  
  if (KB_BUFFER != KB_BUFFER_PREVIOUS) {                                //if this is a new key but not the same key
    KB_DOUBLE_PRESS_EVENT_COUNT = 0;                                    //reset how many double presses have occured
  }

  if (KB_HOLD_DURATION >= (KB_HOLD_REPEAT_DELAY * 20) && KB_HOLD_REPEAT != 0) {  //if the keyboard hold duration is greater than default 1 second
    if (KB_HOLD_DURATION - KB_HOLD_REPEAT_TIMER >= KB_HOLD_REPEAT_SPEED) {        //if default 300ms has passed since the kb hold repeat timer has been set
      KB_BEEN_READ = false;                                                   //set the keyboard as unread, effectively a new key
      KB_HOLD_REPEAT_TIMER = KB_HOLD_DURATION;                                  //store the kb hold timer 
    }
  }


  if (KB_IS_ANYTHING_PRESSED && KB_KEY_MATCHING) {          //if the same key has been seen for another round of scanning
    KB_HOLDSTATE = true;                                    //flag the key as held
    KB_HOLD_DURATION = millis() - KB_HOLD_START_TIME;       //update how long the key has been held for so far
  } else {                                                  //if nothing was detected while looping through the keys
    KB_HOLDSTATE = false;                                   //the key mustve been released
    KB_HOLD_DURATION = 0;                                   //reset all the timers
    KB_HOLD_START_TIME = millis();
    KB_HOLD_REPEAT_TIMER = 0; 
  }

}