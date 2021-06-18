#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#include <FastLED.h>
#define LED_PIN     25
#define NUM_LEDS    5
#define BRIGHTNESS  255
#define LED_TYPE    WS2812B
#define COLOR_ORDER RGB
CRGB leds[NUM_LEDS];
CRGBPalette16 currentPalette;
TBlendType    currentBlending;
#define UPDATES_PER_SECOND 120


#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "ACS712.h"

#define BUTTON1_PIN 18
#define BUTTON2_PIN 19
#define BUTTON3_PIN 23

#define AD1_PIN 32
#define AD2_PIN 33
#define AD3_PIN 34
#define AD4_PIN 35
#define AD5_PIN 36
int ADlist[5] = {AD1_PIN,AD2_PIN,AD3_PIN,AD4_PIN,AD5_PIN};
unsigned int ADRMS[5];


#define R1_PIN 13
#define R2_PIN 14
#define R3_PIN 15
#define R4_PIN 16
#define R5_PIN 17

#define MENU_POS_Y 62
#define MENU_POS_Y_HIDDEN 76

#define Trigger_current 8181 //mA

// Arduino UNO has 5.0 volt with a max ADC value of 1023 steps
// ACS712 5A  uses 185 mV per A
// ACS712 20A uses 100 mV per A
// ACS712 30A uses  66 mV per A

//ACS712  ACS(A0, 5.0, 1023, 100);
// ESP 32 example (requires resistors to step down the logic voltage)
ACS712  ACS1(AD1_PIN, 5.0, 4095, 100);
ACS712  ACS2(AD2_PIN, 5.0, 4095, 100);
ACS712  ACS3(AD3_PIN, 5.0, 4095, 100);
ACS712  ACS4(AD4_PIN, 5.0, 4095, 100);
ACS712  ACS5(AD5_PIN, 5.0, 4095, 100);


U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

//藍芽
BluetoothSerial SerialBT;

boolean PairSuccess = false;
boolean RequestPending = false;
uint32_t numValcode = 0;

unsigned long onTime = 0;

int CRMS[5];

int menuActive = 1;
int menuSelected = 2;
/*#######################################################
 *                       SETUP                          #
 ########################################################*/
void setup(void) {

  ACS1.autoMidPoint(60);
  ACS2.autoMidPoint(60);
  ACS3.autoMidPoint(60);
  ACS4.autoMidPoint(60);
  ACS5.autoMidPoint(60);

  //按鈕
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);

  //繼電器
  pinMode(R1_PIN, OUTPUT);
  pinMode(R2_PIN, OUTPUT);
  pinMode(R3_PIN, OUTPUT);
  pinMode(R4_PIN, OUTPUT);
  pinMode(R5_PIN, OUTPUT);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(  BRIGHTNESS );
  
  Serial.begin(115200);
  
  u8g2.begin();
  
  u8g2.setFont(u8g2_font_9x15B_tf);
  u8g2.setFontDirection(0);
  u8g2.firstPage();
  do {
    u8g2.setCursor(55, 20);
    u8g2.print("YU");
    u8g2.setCursor(28, 40);
    u8g2.print("WorkShop");
    
    u8g2.drawFrame(0,0,128,64);
  } while ( u8g2.nextPage() );
  
  digitalWrite(R1_PIN,LOW);
  digitalWrite(R2_PIN,LOW);
  digitalWrite(R3_PIN,LOW);
  digitalWrite(R4_PIN,LOW);
  digitalWrite(R5_PIN,LOW); 
  int sttt = millis();
  while(millis() - sttt <= 500){
    Correction(); //歸零RMS
  }
  digitalWrite(R1_PIN,HIGH);
  digitalWrite(R2_PIN,HIGH);
  digitalWrite(R3_PIN,HIGH);
  digitalWrite(R4_PIN,HIGH);
  digitalWrite(R5_PIN,HIGH);

  SerialBT.enableSSP();
  SerialBT.onConfirmRequest(BTConfirmRequestCallback);
  SerialBT.onAuthComplete(BTAuthCompleteCallback);
  SerialBT.begin("YUSmartSocket [YUSS]");; //Bluetooth device name
  
  delay(1000);
  
  u8g2.setFont(u8g2_font_6x10_tr);
  onTime = millis();
}

unsigned long OLED_Sleep_timer = millis();
bool SleepMode = false;
int Cmenu = 0; //頁面選擇
int StatusMode = 1;
int ADreads = 0;
unsigned long start_timeloop = millis();
unsigned long Menu_timeout = millis();
int nnni=0 ;

int mainStatusDisplayMode = 1;
bool alarms = false;
int overTime;

String SelectMenuItems[3];
String menuItems1[6][3] = {{"MAIN", "RESET", "SETUP"},{"WATT", "Ampere", "Close"},{"Mode","Unit","NEXT"},{"Total","AVG","ALL"},{"CORR","BT|Conf","BACK"},{"Pair!","Deny","BACK"}};

int TCount = 0; //繼電器斷電的順序 
bool RelayStatus[5] = {true,true,true,true,true};
int relayPIN[5] = {13,14,15,16,17};

/*#######################################################
 *                    L O O P                           #
 ########################################################*/
void loop(void) {
  u8g2.firstPage();
  
  do {
    for (int i=0; i <5; i++){
      RelayStatus[4-i] = digitalRead(relayPIN[i]);
      leds[i] = (RelayStatus[i] == true)? CRGB::Blue : CRGB::Red;
    }

    FastLED.show();
    int sss;
    sss = drawMenuBar(menuItems1[Cmenu]);
    if (sss!=-1 ){
      switch (Cmenu){
        case 0: //頁面0
          if (sss==2){
            digitalWrite(R1_PIN,HIGH);
            digitalWrite(R2_PIN,HIGH);
            digitalWrite(R3_PIN,HIGH);
            digitalWrite(R4_PIN,HIGH);
            digitalWrite(R5_PIN,HIGH);
            TCount=0;
          }
          if (sss==1){
            Cmenu = 2;
          }
          if (sss==3){
            Cmenu = 4;
          }
        break;
        case 1://頁面1
          if (sss==3){
            Cmenu = 0;
          }else if (sss==1){
            StatusMode = 1;
          }else if (sss==2){
            StatusMode = 2;
          }
          Cmenu = 0;
        break;
        case 2://頁面2
          if (sss==3){
            Cmenu = 0;
          }else if (sss==1){
            Cmenu = 3;
          }else if (sss==2){
            Cmenu = 1;
          }
        break;
        case 3: //頁面2
          if (sss==3){
            mainStatusDisplayMode = 3;
          }else if (sss==1){
            mainStatusDisplayMode = 1;
          }else if (sss==2){
            mainStatusDisplayMode = 2;
          }
          Cmenu = 0;
        break;
        case 4: //{"CORR","BT","BACK"}
          if (sss==3){
            Cmenu = 0;
          }else if (sss==1){
            Correction();
          }else if (sss==2){
            Cmenu = 5;
          }
        break;
        case 5: //{"Pair!","Deny","BACK"}
          if (sss==3){
            Cmenu = 0;
          }else if (sss==1){
            SerialBT.confirmReply(true);
            Cmenu = 0;
          }else if (sss==2){
            SerialBT.confirmReply(false);
            Cmenu = 0;
          }
        break;
      }
    }
    if (Cmenu != 0){
      if (millis() - Menu_timeout >10000){
        Cmenu= 0;
      }
    }else{
      Menu_timeout = millis();
    }
    ReadACS();//電流讀取

    if (Cmenu == 5){
      ShowCode();
    }else{
      switch (mainStatusDisplayMode){
        case 1:
          drawSUMStatus();//電壓電流狀態
        break;
        case 2:
          drawAVGStatus();//電壓電流狀態
        break;
        case 3:
          drawALLStatus();//電壓電流狀態
        break;
      }
    }
    //Serial.println(mainStatusDisplayMode);
    BTmsgLoop();
    Killpower();
    displayWh();
    if (millis() - OLED_Sleep_timer >= 60000){
      u8g2.setPowerSave(1);
      SleepMode = true;
    }else{
      u8g2.setPowerSave(0);
      if (digitalRead(BUTTON1_PIN) && digitalRead(BUTTON2_PIN) && digitalRead(BUTTON3_PIN)) SleepMode = false;
      
    }
    //Serial.println(analogRead(AD5_PIN));
    
  } while ( u8g2.nextPage() );
}


int dd,hh,mm,ss ;
float kwh=0;
float wh=0;
float amps;
float sumssss;

void displayWh(){

  u8g2.setFont(u8g2_font_profont11_tr);
  String text;
  ss = ((millis()-onTime)/1000);
  mm = ss / 60;
  hh = mm / 60;
  dd = hh / 24;
  
  text = "T:" + String(dd) + "." + String(hh%24) + ":" +String(mm%60)/*+":"+String(ss%60)*/;
  
  // center text
  //int textWidth = u8g2.getStrWidth(text.c_str());
  //int textX = (128 - textWidth) / 2;
  u8g2.setDrawColor(1);
  u8g2.setCursor(1,10);
  u8g2.print(text);

  static unsigned long Lasttime;
  if (millis() - Lasttime > 1000){
    kwh += ((ADRMS[0]+ADRMS[1]+ADRMS[2]+ADRMS[3]+ADRMS[4]/1000)*110)/3600000;
    Lasttime=millis();
  }
  text = String(kwh,2)+" kWh";
  int textWidth = u8g2.getStrWidth(text.c_str());
  int textX = (128 - textWidth);
  u8g2.setDrawColor(1);
  u8g2.setCursor(textX ,10);
  u8g2.print(text);
}

/*#######################################################
 *                    總順序斷電                         #
 ########################################################*/
void Killpower(){
  if (ADRMS[0]+ADRMS[1]+ADRMS[2]+ADRMS[3]+ADRMS[4] >= Trigger_current){
      if (!alarms){
        overTime = millis();
        alarms=true;
      }

      static bool TCountT;
      if (((millis()-overTime)/1000)%4 == 3 && !TCountT){
        TCount++;
        TCountT = true;
      }else if (((millis()-overTime)/1000)%4 != 3){
        TCountT = false;
      }
      
     
      switch (TCount){
        case 0:
        break;
        case 1:
          digitalWrite(R5_PIN,LOW);
          
        break;
        
        case 2:
          digitalWrite(R4_PIN,LOW);
          
        break;
        
        case 3:
          digitalWrite(R3_PIN,LOW);
          
        break;
        
        case 4:
          digitalWrite(R2_PIN,LOW);
          
        break;
        
        case 5:
          digitalWrite(R1_PIN,LOW);
          delay(500);
          Correction();
          
        break;
        
      }

    }else{
      alarms=false;
    }
}
/*#######################################################
 *                       Other                          #
 ########################################################*/
void Correction(){
  
  CRMS[0] = max(ACS1.mA_AC(60), CRMS[0]);
  CRMS[1] = max(ACS2.mA_AC(60), CRMS[0]);
  CRMS[2] = max(ACS3.mA_AC(60), CRMS[0]);
  CRMS[3] = max(ACS4.mA_AC(60), CRMS[0]);
  CRMS[4] = max(ACS5.mA_AC(60), CRMS[0]);
  
}

void restPSTimer(){
  OLED_Sleep_timer = millis();
}


/*#######################################################
 *                    ReadACS                           #
 ########################################################*/
void ReadACS(){
  if (millis() - start_timeloop >= 50){
      start_timeloop = millis();
      nnni ++;
      if (nnni >= 5){
        nnni=0;
      }
      switch (nnni){
        case 0:
          ADRMS[0] = max(ACS1.mA_AC(60) - CRMS[0], 0);
        break;
        case 1:
          ADRMS[1] = max(ACS2.mA_AC(60) - CRMS[1], 0);
        break;
        case 2:
          ADRMS[2] = max(ACS3.mA_AC(60) - CRMS[2], 0);
        break;
        case 3:
          ADRMS[3] = max(ACS4.mA_AC(60) - CRMS[3], 0);
        break;
        case 4:
          ADRMS[4] = max(ACS5.mA_AC(60) - CRMS[4], 0);
        break;
      
      }
  }
}
///RequestPending
/*#######################################################
 *                 Menu - showBLE                       #
 ########################################################*/

void ShowCode(){
  u8g2.setFont(u8g2_font_fub20_tr);
  static String text;

  text = String(numValcode);
  
  // center text
  int textWidth = u8g2.getStrWidth(text.c_str());
  int textX = (128 - textWidth) / 2;
  u8g2.setDrawColor(1);
  u8g2.setCursor(textX, 25+20);
  u8g2.print(text);
}


/*#######################################################
 *                    Menu - Mode                       #
 ########################################################*/
float sumRms=0;
void drawSUMStatus(){
  static unsigned long Lasttime;
  sumRms = ADRMS[0]+ADRMS[1]+ADRMS[2]+ADRMS[3]+ADRMS[4];
  u8g2.setFont(u8g2_font_fub20_tr);
  static String text;
  
  if (millis() - Lasttime > 500){
    Lasttime = millis();
    if (StatusMode == 1){
      text = String(sumRms/1000 *110) + "W";
    }else if (StatusMode == 2){
      text = String(sumRms/1000) + "A";
    }
  }
  
  // center text
  int textWidth = u8g2.getStrWidth(text.c_str());
  int textX = (128 - textWidth) / 2;
  u8g2.setDrawColor(1);
  u8g2.setCursor(textX, 25+20);
  u8g2.print(text);
  
  u8g2.setFont(u8g2_font_helvB08_tr);
  
  u8g2.setDrawColor(1);
  u8g2.setCursor(1, 21);
  u8g2.print("MAX Watts: " + String(((Trigger_current/1000)*110))+"W");
}
  

void drawAVGStatus(){
  float sumRms = (ADRMS[0]+ADRMS[1]+ADRMS[2]+ADRMS[3]+ADRMS[4])/5;
  u8g2.setFont(u8g2_font_fub20_tr);
  static String text;
  static unsigned long Lasttime;
  
  if (millis() - Lasttime > 1000){
    Lasttime = millis();
    if (StatusMode == 1){
      text = String(sumRms/1000 *110) + "W";
    }else if (StatusMode == 2){
      text = String(sumRms/1000) + "A";
    }
  }
  
  // center text
  int textWidth = u8g2.getStrWidth(text.c_str());
  int textX = (128 - textWidth) / 2;
  u8g2.setDrawColor(1);
  u8g2.setCursor(textX, 25+20);
  u8g2.print(text);
  
}

void drawALLStatus(){
  u8g2.setFont(u8g2_font_nokiafc22_tf );
  static String text[5];
  static unsigned long Lasttime;
  if (millis() - Lasttime > 1000){
    for (int ii; ii<=4;ii++){
      Serial.println(ii);
      if (StatusMode == 1){
        text[ii] = String(ii+1) + ": "+ String(ADRMS[ii]/1000 *110) + "W";
      }else if (StatusMode == 2){
        text[ii] = String(ii+1) + ": "+String(ADRMS[ii]*0.001) + "A";
      }
    }
    Lasttime = millis();
  }
  
  // center text
  int textWidth = u8g2.getStrWidth(text[0].c_str());
  int textX = (128/3 - textWidth) / 2;
  u8g2.setDrawColor(1);
  u8g2.setCursor(textX, 25+2);
  u8g2.print(text[0]);
  
  
  textWidth = u8g2.getStrWidth(text[1].c_str());
  textX = ((128/3)*3 - textWidth) / 2;
  u8g2.setDrawColor(1);
  u8g2.setCursor(textX, 25+2);
  u8g2.print(text[1]);
  
  
  textWidth = u8g2.getStrWidth(text[2].c_str());
  textX = ((128/3)*5 - textWidth) / 2;
  u8g2.setDrawColor(1);
  u8g2.setCursor(textX, 25+2);
  u8g2.print(text[2]);
  
  textWidth = u8g2.getStrWidth(text[3].c_str());
  textX = (128/2 - textWidth) / 2;
  u8g2.setDrawColor(1);
  u8g2.setCursor(textX, 25+15);
  u8g2.print(text[3]);
  
  textWidth = u8g2.getStrWidth(text[4].c_str());
  textX = ((128+64) - textWidth) / 2;
  u8g2.setDrawColor(1);
  u8g2.setCursor(textX, 25+15);
  u8g2.print(text[4]);
  
}
/*#######################################################
 *                 MainPage - Display                   #
 ########################################################*/
int drawMenuBar(String menuItems[3]) {
  u8g2.setDrawColor(1);
  u8g2.drawHLine(0, 12, 128);
  
  int textX = 0;
  int textY = MENU_POS_Y;
  int textWidth = 0;
  int textXPadding = 4;
  static int bPress = 0;
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setDrawColor(1);


  u8g2.drawHLine(0, textY - 11 - 2, 128);
  // center menu
  String text = menuItems[1];
  textWidth = u8g2.getStrWidth(text.c_str());
  textX = (128 - textWidth) / 2;
  if (digitalRead(BUTTON2_PIN)) {
    if (bPress==2){
      bPress=0;
      return 2;
    }
    u8g2.drawRBox(textX - textXPadding, textY + 2 - 11, textWidth + textXPadding + textXPadding, 11, 2);
    u8g2.setDrawColor(0);
  } 
  if (!digitalRead(BUTTON2_PIN)) {
    if (bPress==0 && !SleepMode ){
      bPress = 2;
    }
    restPSTimer();
    u8g2.drawRFrame(textX - textXPadding, textY + 2 - 11, textWidth + textXPadding + textXPadding, 11, 2);
    u8g2.setDrawColor(1);
  }

  u8g2.setCursor(textX, textY);
  u8g2.print(text);
  u8g2.setDrawColor(1);



  // left menu
  text = menuItems[0];
  textX = textXPadding;
  textWidth = u8g2.getStrWidth(text.c_str());
  if (digitalRead(BUTTON1_PIN)) {
    if (bPress==1){
      bPress=0;
      return 1;
    }
    u8g2.drawRBox(textX - textXPadding, textY + 2 - 11, textWidth + textXPadding + textXPadding, 11, 2);
    u8g2.setDrawColor(0);
  } 
  if (!digitalRead(BUTTON1_PIN)) {
    if (bPress==0 && !SleepMode){
      bPress = 1;
    }
    restPSTimer();
    u8g2.drawRFrame(textX - textXPadding, textY + 2 - 11, textWidth + textXPadding + textXPadding, 11, 2);
    u8g2.setDrawColor(1);
  }
  u8g2.setCursor(textX, textY);
  u8g2.print(text);
  u8g2.setDrawColor(1);



  // right menu
  text = menuItems[2];
  textWidth = u8g2.getStrWidth(text.c_str());
  textX = 128 - textWidth - textXPadding;
  if (digitalRead(BUTTON3_PIN)) {
    if (bPress==3){
      bPress=0;
      return 3;
    }
    u8g2.drawRBox(textX - textXPadding, textY + 2 - 11, textWidth + textXPadding + textXPadding, 11, 2);
    u8g2.setDrawColor(0);
  }
  if (!digitalRead(BUTTON3_PIN)) {
    if (bPress==0 && !SleepMode){
      bPress = 3;
    }
    restPSTimer();
    u8g2.drawRFrame(textX - textXPadding, textY + 2 - 11, textWidth + textXPadding + textXPadding, 11, 2);
    u8g2.setDrawColor(1);
  }
  u8g2.setCursor(textX, textY);
  u8g2.print(text);
  u8g2.setDrawColor(1);
  return -1;
}

/*#######################################################
 *                         BT                           #
 ########################################################*/
boolean confirmRequestPending = true;

void BTConfirmRequestCallback(uint32_t numVal){
  restPSTimer();
  confirmRequestPending = true;
  RequestPending = true;
  numValcode = numVal;
  Cmenu = 5;
}

void BTAuthCompleteCallback(boolean success){
  confirmRequestPending = false;
  if (success)
  {
    //Serial.println("Pairing success!!");
    Cmenu = 0;
    PairSuccess = true;
  }
  else
  {
    Cmenu = 0;
    PairSuccess = false;
  }
}

boolean stringComplete;
char cmd[1];
char inputStr[20];
int chrlen = 0;

void BTmsgLoop(){

  if (SerialBT.available()){
    char inChar = (char)SerialBT.read();
    inputStr[chrlen] = inChar;
    chrlen++;
    if (inChar == 10) {
       stringComplete = true;
       inputStr[chrlen] = '\0';
    }
  }
  if (stringComplete){
    substr(cmd, inputStr, 0, 1);
    stringComplete = false;
    Serial.print(inputStr);
    Serial.print("-");
    Serial.println(cmd[0]);
    switch (cmd[0]){
      case 'a':
        SerialBT.println("W"+String(sumRms/1000 *110));
        break;
      case 'b':
        digitalWrite(R1_PIN,HIGH);
        SerialBT.println("L11");
        break;
      case 'c':
        digitalWrite(R2_PIN,HIGH);
        SerialBT.println("L21");
        break;
      case 'd':
        SerialBT.println("L31");
        digitalWrite(R3_PIN,HIGH);
        break;
      case 'e':
        SerialBT.println("L41");
        digitalWrite(R4_PIN,HIGH);
        break;
      case 'f':
        SerialBT.println("L51");
        digitalWrite(R5_PIN,HIGH);
        break;
      case 'g':
        SerialBT.println("L10");
        digitalWrite(R1_PIN,LOW);
        break;
      case 'h':
        SerialBT.println("L20");
        digitalWrite(R2_PIN,LOW);
        break;
      case 'i':
        SerialBT.println("L30");
        digitalWrite(R3_PIN,LOW);
        break;
      case 'j':
        SerialBT.println("L40");
        digitalWrite(R4_PIN,LOW);
        break;
      case 'k':
        SerialBT.println("L50");
        digitalWrite(R5_PIN,LOW);
        break;
      case 'l':
        SerialBT.println("OK");
        digitalWrite(R1_PIN,HIGH);
        digitalWrite(R2_PIN,HIGH);
        digitalWrite(R3_PIN,HIGH);
        digitalWrite(R4_PIN,HIGH);
        digitalWrite(R5_PIN,HIGH);
        break;
    }
    chrlen = 0;
  }
  /*
   * digitalWrite(R1_PIN,HIGH);
  digitalWrite(R2_PIN,HIGH);
  digitalWrite(R3_PIN,HIGH);
  digitalWrite(R4_PIN,HIGH);
  digitalWrite(R5_PIN,HIGH);
   */
}
void substr(char *dest, const char* src, unsigned int start, unsigned int cnt) {
  strncpy(dest, src + start, cnt);
  dest[cnt] = 0;
}
