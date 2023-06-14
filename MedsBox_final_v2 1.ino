#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <MedsBox.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"
#define buzzer 19 

//Configurações de wifi
#define WIFI_SSID "buchala"
#define WIFI_PASSWORD "gabriel321"

//Configurações do Firebase
#define API_KEY "AIzaSyAfSy5TOtpCLtof3yJk3MOMz8IoXvJ5FCg"
#define USER_EMAIL "buchalabjj@gmail.com"
#define USER_PASSWORD "123456789"
#define DATABASE_URL "https://medsbox-961ad-default-rtdb.firebaseio.com/"

// Defininção dos objetos do Firebase
FirebaseData fbdo;
FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;

// Path do bando de dados medications
String listenerPath = "medications/";

// Configurações do NTP para atualização do horário
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -10800, 30000);

LiquidCrystal_I2C lcd(0x27,16,2); 

// Criação do array de objetos MedsBox
MedsBox boxes[7] = {
  MedsBox(1,13,15), //boxes[0]
  MedsBox(2,12,4),
  MedsBox(3,14,16),
  MedsBox(4,26,17),
  MedsBox(5,25,5),
  MedsBox(6,33,18),
  MedsBox(7,32,23),
};

bool buzzerAlarm = false;
bool buzzerState = false;

unsigned long lastTimeClientUpdate;
unsigned long lastTimeVerification;
unsigned long lastTimeAlarm;

// Função para conectar no wifi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  lcd.setCursor(0,0);
  lcd.print("Conectando em...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    lcd.setCursor(0,1);
    lcd.print(WIFI_SSID);
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

// Função callback executada quando algum dado for atualizado no real time databse do Firebase
void streamCallback(FirebaseStream data){
  updateFirebaseData();
  printBoxesData();
}

// Função que atualiza os objetos MedsBox
void updateFirebaseData(){
  String ref = "medications/";
  for (int i = 1; i < 8; i++) {
    String boxRef = String(ref + String(i));
    Firebase.RTDB.getJSON(&fbdo, boxRef);
    FirebaseJson json = fbdo.jsonObject();
    size_t count = json.iteratorBegin();
    for (size_t j = 0; j < count; j++){
      FirebaseJson::IteratorValue value = json.valueAt(j);
      if(value.key.equals("remedy")){
        boxes[i-1].setRemedy(String(value.value));
      }else if (value.key.equals("description")){
        boxes[i-1].setDescription(String(value.value));
      }else if(value.key.equals("periodicity")){
        boxes[i-1].setPeriodicity(value.value.toInt());
      }else if(value.key.equals("hour")){
        boxes[i-1].setHour(value.value.toInt());
      }else if(value.key.equals("minutes")){
        boxes[i-1].setMinutes(value.value.toInt());
      }else if(value.key.equals("active")){
        boxes[i-1].setActive(value.value);
      }
    }
    json.iteratorEnd(); // required for free the used memory in iteration (node data collection)
  }
}

// Função para printar no serial os objetos MedsBox
void printBoxesData(){
  for (int i = 0; i < 7; i++) {
    Serial.println("==============================================");
    Serial.println(boxes[i].getRemedy());
    Serial.println(boxes[i].getDescription());
    Serial.println(boxes[i].getPeriodicity());
    Serial.println(boxes[i].getHour());
    Serial.println(boxes[i].getMinutes());
    Serial.println(boxes[i].getOutput());
    Serial.println(boxes[i].getMicroSwitch());
    Serial.println(boxes[i].getSelected());
    Serial.println(boxes[i].getActive());
  }
}

// Função de timeoutCalback do Firebase
void streamTimeoutCallback(bool timeout){
  if (timeout)
    Serial.println("stream timeout, resuming...\n");
  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

void setup(){
  pinMode (buzzer, OUTPUT);
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  initWiFi();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("MEDSBOX    ");
  lcd.setCursor(11,0);
  lcd.print("--:--");
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  config.max_token_generation_retry = 5;
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);
  if (!Firebase.RTDB.beginStream(&stream, listenerPath.c_str()))
    Serial.printf("stream begin error, %s\n\n", stream.errorReason().c_str());
  Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
  timeClient.begin();
  delay(2000);
  lastTimeClientUpdate = millis();
  lastTimeVerification = millis();
  lastTimeAlarm = millis();
}

void updateTimeClient(){
  if((millis() - lastTimeClientUpdate) > 60000) {
    timeClient.update();
    Serial.print("Horário atual: ");
    Serial.print(timeClient.getHours());
    Serial.print(":");
    Serial.println(timeClient.getMinutes());
    lcd.setCursor(11,0);
    lcd.print("     ");
    lcd.setCursor(11,0);
    lcd.print(timeClient.getHours());
    lcd.setCursor(13,0);
    lcd.print(":");
    lcd.setCursor(14,0);
    lcd.print(timeClient.getMinutes());
    lastTimeClientUpdate = millis();
  }
}

void verification() {
  if(buzzerAlarm){
     intermittentBuzzer();
  }
  for(int k = 0; k < 7; k++){
    //Testa se um box selecionado foi aberto, então para alarme
    if(digitalRead(boxes[k].getMicroSwitch()) && boxes[k].getSelected()){
      boxes[k].off();
      boxes[k].setSelected(false);
      buzzerAlarm = false;
      digitalWrite(buzzer, LOW);
      lcd.setCursor(0,1);
      lcd.print("                ");
    }

    if((millis() - lastTimeVerification)>60000){
      for(int i = 0; i < 7; i++){
        if(boxes[i].getActive() == true){
          if(boxes[i].compare(timeClient.getHours(), timeClient.getMinutes()) == true){
            boxes[i].setSelected(true);
            boxes[i].on();
            buzzerAlarm = true;
            String boxNumber = String(boxes[i].getBox());
            lcd.setCursor(0,1);
            lcd.print(boxes[i].getBox());
            lcd.setCursor(2,1);
            lcd.print(boxes[i].getRemedy());
          }
        }
      } 
    lastTimeVerification = millis();
    } 
  }
}

// Função para ativar buzzer de forma intermitente
void intermittentBuzzer(){
  if((millis() - lastTimeAlarm) > 600){
    if(buzzerState){
      digitalWrite(buzzer, HIGH);
    }else{
      digitalWrite(buzzer, LOW);
    }
    buzzerState = !buzzerState;
    lastTimeAlarm = millis();
  }
}

void loop(){
  if (Firebase.isTokenExpired()){
    Firebase.refreshToken(&config);
    Serial.println("Refresh token");
  }
  updateTimeClient();
  verification();
}
