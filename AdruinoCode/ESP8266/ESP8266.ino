#include <LiquidCrystal.h>

#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
// Khai báo LCD, RTC, SERVO
LiquidCrystal_I2C lcd(0x27, 16, 2); 
RTC_DS1307 RTC;
DateTime currentTime;
Servo myservo;  
// LED, BUTTON, BUZZER, SERVO PIN
const int LED_PIN = 13; //D7
const int ALARM_PIN = 12; //D6 (BUZZER)
const int SERVO_PIN = 14; //D5
//const int BTN_LED = 2; //
const int BTN_SERVO = 2; //D4 
//const int BTN_ALARM = 2; //
const int BTN_MENU = 0; // D3
int servoAngle; //pos
// Biến kiểm tra trạng thái của LED, SERVO, ALARM
bool isMenu = false;
bool ledStatus;
bool servoStatus = false;
bool alarmStatus = false;

int field = 0; // 0: ledStatus, 1: servoStatus, 2: alarmStatus, 3: clock
DateTime ledAlarm(2000, 0, 0, 0, 0, 0);
DateTime doorAlarm(2000, 0, 0, 0, 0, 0);
DateTime alarmUp(2000, 0, 0, 0, 0, 0);
// Khai báo WIFI kết nối, địa chỉ WebSocket
const char* ssid = "TIGER 12";
const char* password = "123456789";
// để đưa đoạn code HTML vào chương trình Arduino, cần chuyển đổi code HTML sang dạng char
const char index_html[] PROGMEM = ""
                                  "<!DOCTYPE HTML>"
                                  "<html>"
                                  "<head>"
                                  " <title>ESP8266 WebSocket</title>"
                                  "</head>"
                                  "<body>"
                                  " <div> Webscoket status <span id=\"status\" style=\"font-weight: bold;\"> disconnected </span> </div>"
                                  " <div> ESP8266 Button Status <input type=\"checkbox\" id=\"btn\" name=\"btn\" /> </div>"
                                  " <div> Control LED <input type=\"checkbox\" id=\"led\" name=\"led\" disabled=\"true\" /> </div>"
                                  " <script type=\"text/javascript\">"
                                  " var button = document.getElementById('btn');"
                                  " var led = document.getElementById('led');"
                                  " var status = document.getElementById('status');"
                                  " var url = window.location.host;"
                                  " var ws = new WebSocket('ws://' + url + '/ws');"
                                  " console.log('ws://' + url + '/ws');"
                                  " ws.onopen = function()"
                                  " {"
                                  " status.text = 'Connected';"
                                  " led.disabled = false;"
                                  " };"
                                  " ws.onmessage = function(evt)"
                                  " {"
                                  " if(evt.data == 'BTN_PRESSED') {"
                                  " button.checked = true;"
                                  " } else if(evt.data == 'BTN_RELEASE') {"
                                  " button.checked = false;"
                                  " }"
                                  " };"
                                  " ws.onclose = function() {"
                                  " led.disabled = true;"
                                  " status.text = 'Disconnected';"
                                  " };"
                                  " led.onchange = function() {"
                                  " var status = 'LED_OFF';"
                                  " if (led.checked) {"
                                  " status = 'LED_ON';"
                                  " }"
                                  " ws.send(status)"
                                  " }"
                                  " </script>"
                                  "</body>"
                                  "</html>";
AsyncWebServer server(8000);
AsyncWebSocket ws("/ws");
// Hàm xử lý sự kiện trên Server khi client là browser phát sự kiện
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data,
               size_t len) {
  // type: loại sự kiện mà server nhận được. Nếu sự kiện nhận được là từ websocket thì bắt đầu xử lí
  if(type == WS_EVT_CONNECT){ //Sự kiện Connect
      Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
      ledStatus? ws.textAll("LEDStatus_ON") : ws.textAll("LEDStatus_OFF");
      servoStatus? ws.textAll("DOORStatus_ON") : ws.textAll("DOORStatus_OFF");
      alarmStatus? ws.textAll("ALARMStatus_ON") : ws.textAll("ALARMStatus_OFF");
  } 
  if (type == WS_EVT_DATA && len > 0) { //Sự kiện nhận được DATA
    data[len] = 0;
    String data_str = String((char*)data); // ép kiểu, đổi từ kiểu char sang String
    if (data_str == "LED_ON") {
        // Khi server phát sự kiện "LED_ON" thì server sẽ bật LED
        digitalWrite(LED_PIN, HIGH);
        ledStatus = true;
        Serial.println("LED_ON");
        ws.textAll("LEDStatus_ON");
    } else if (data_str == "LED_OFF") {
        // Khi server phát sự kiện "LED_OFF" thì server sẽ tắt LED
        digitalWrite(LED_PIN, LOW);
        ledStatus = false;
        Serial.println("LED_OFF");
        ws.textAll("LEDStatus_OFF");
    }
    if (data_str == "DOOR_ON") {
        // Khi server phát sự kiện "DOOR_ON" thì server sẽ bật SERVO
        myservo.write(180);
        servoStatus = true;
        Serial.println("DOOR_ON");
        ws.textAll("DOORStatus_ON");
    } else if (data_str == "DOOR_OFF") {
        // Khi server phát sự kiện "DOOR_OFF" thì server sẽ tắt SERVO
        myservo.write(0);
        servoStatus = false;
        Serial.println("DOOR_OFF");
        ws.textAll("DOORStatus_OFF");
    }
    if (data_str == "ALARM_ON") {
      // Khi client phát sự kiện "ALARM_ON" thì server sẽ tắt BUZZER
      alarmStatus = true;
      Serial.println("ALARM_ON");
      ws.textAll("ALARMStatus_ON");
    }
    if (data_str == "ALARM_OFF") {
    // Khi client phát sự kiện "ALARM_OFF" thì server sẽ tắt BUZZER
    alarmStatus = false;
    Serial.println("ALARM_OFF");
    ws.textAll("ALARMStatus_OFF");
    }
    if (data_str.startsWith("LEDAlarm_TIME:")) {
      String time_str = data_str.substring(14); // Lấy phần của chuỗi từ vị trí 15 trở đi
      Serial.println(data_str);
      Serial.println(time_str);
      int year = time_str.substring(0, 4).toInt();
      int month = time_str.substring(4, 6).toInt();
      int day = time_str.substring(6, 8).toInt();
      int hour = time_str.substring(8, 10).toInt();
      int minute = time_str.substring(10, 12).toInt();
      int second = time_str.substring(12, 14).toInt();
      ledAlarm = DateTime(year, month, day, hour, minute, second);
    }
    if (data_str.startsWith("DOORAlarm_TIME:")) {
      String time_str = data_str.substring(15); // Lấy phần của chuỗi từ vị trí 17 trở đi
      Serial.println(data_str);
      Serial.println(time_str);
      int year = time_str.substring(0, 4).toInt();
      int month = time_str.substring(4, 6).toInt();
      int day = time_str.substring(6, 8).toInt();
      int hour = time_str.substring(8, 10).toInt();
      int minute = time_str.substring(10, 12).toInt();
      int second = time_str.substring(12, 14).toInt();
      doorAlarm = DateTime(year, month, day, hour, minute, second);
    }
    if (data_str.startsWith("ALARM_TIME:")) {
      String time_str = data_str.substring(11); // Lấy phần của chuỗi từ vị trí 12 trở đi
      Serial.println(data_str);
      Serial.println(time_str);
      int year = time_str.substring(0, 4).toInt();
      int month = time_str.substring(4, 6).toInt();
      int day = time_str.substring(6, 8).toInt();
      int hour = time_str.substring(8, 10).toInt();
      int minute = time_str.substring(10, 12).toInt();
      int second = time_str.substring(12, 14).toInt();
      alarmUp = DateTime(year, month, day, hour, minute, second);
    }
    if (data_str.startsWith("DATETIME:")) {
      String time_str = data_str.substring(9); // Lấy phần của chuỗi từ vị trí 12 trở đi
      Serial.println(data_str);
      Serial.println(time_str);
      int year = time_str.substring(0, 4).toInt();
      int month = time_str.substring(4, 6).toInt();
      int day = time_str.substring(6, 8).toInt();
      int hour = time_str.substring(8, 10).toInt();
      int minute = time_str.substring(10, 12).toInt();
      int second = time_str.substring(12, 14).toInt();
      DateTime newTime(year, month, day, hour, minute, second);
      Serial.print(newTime.year());
      Serial.print("/");
      Serial.print(newTime.month());
      Serial.print("/");
      Serial.print(newTime.day());
      Serial.print("/");
      Serial.print(newTime.hour());
      Serial.print(":");
      Serial.print(newTime.minute());
      Serial.print(":");
      Serial.println(newTime.second());
      RTC.adjust(newTime);
    }
  }
}     
void setup()
{
  // Khai báo PIN
  pinMode(LED_PIN, OUTPUT); //LED
  //pinMode(BTN_LED, INPUT_PULLUP); // LED BUTTON
  myservo.attach(SERVO_PIN); //SERVO
  myservo.write(0);
  pinMode(BTN_SERVO, INPUT_PULLUP); // SERVO BUTTON
  pinMode(ALARM_PIN, OUTPUT); //BUZZER
  //pinMode(BTN_ALARM, INPUT_PULLUP); // ALARM BUTTON
  pinMode(BTN_MENU, INPUT_PULLUP); // MENU BUTTON
  //Thiết lập màn hình LCD
  ledAlarm = DateTime(2000, 0, 0, 0, 0, 0);
  doorAlarm = DateTime(2000, 0, 0, 0, 0, 0);
  alarmUp = DateTime(2000, 0, 0, 0, 0, 0);
  field = 0;
  lcd.init();
  lcd.backlight();
  Serial.begin(115200); // Bật cổng Serial
  Wire.begin();
  Wire.beginTransmission(0x68);
  Wire.write(0x07);
  Wire.write(0x10);
  Wire.endTransmission();
  //Thiết lập đồng hồ RTC
  RTC.begin();
  if (RTC.isrunning()) {
    Serial.println("RTC is running!");
  } else {
    Serial.println("RTC lost power");
    RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Kết nối WIFI và WebSocket
  Serial.setDebugOutput(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("STA: Failed!\n");
    WiFi.disconnect(false);
    delay(1000);
    WiFi.begin(ssid, password);
  }
  ws.onEvent(onWsEvent); // gọi hàm onWsEvent
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html); // trả về file index.html trên giao diện browser khi browser truy cập vào IP của server
  });
  server.begin(); // khởi động server
}
void loop()
{
  ledStatus = digitalRead(LED_PIN);
  currentTime = RTC.now();
  // Serial.print(currentTime.year());
  // Serial.print("/");
  // Serial.print(currentTime.month());
  // Serial.print("/");
  // Serial.print(currentTime.day());
  // Serial.print("-");
  // Serial.print(currentTime.hour());
  // Serial.print(":");
  // Serial.print(currentTime.minute());
  // Serial.print(":");
  // Serial.println(currentTime.second());
  if(field == 0 && !isMenu) updateLCD();
  timeUP();
  if(alarmStatus){
    for(int i = 0; i < 5; i++){
    digitalWrite(ALARM_PIN, HIGH); 
    delay(300);
    digitalWrite(ALARM_PIN, LOW);
    delay(300);
    if(!alarmStatus){
    // Khi client phát sự kiện "ALARM_OFF" thì server sẽ tắt BUZZER
      delay(1000);
      break;
    }
  }
    digitalWrite(ALARM_PIN, LOW);
    Serial.println("ALARM_OFF");
    ws.textAll("ALARMStatus_OFF");
    alarmStatus = false;
  }
  checkButtons();
}

// Hàm hiển thị LCD
void updateLCD() {
  String daysOfWeek[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  lcd.setCursor(0, 0);
  lcd.print("TIME:   ");
  printWithLeadingZero(currentTime.hour());
  lcd.print(":");
  printWithLeadingZero(currentTime.minute());
  lcd.print(":");
  printWithLeadingZero(currentTime.second());

  lcd.setCursor(0, 1);
  lcd.print(daysOfWeek[currentTime.dayOfTheWeek()]);
  lcd.print("   ");
  printWithLeadingZero(currentTime.day());
  lcd.print("/");
  printWithLeadingZero(currentTime.month());
  lcd.print("/");
  lcd.print(currentTime.year());
}
// Hàm định dạng thêm số 0 cho các số ở hàng đơn vị (x -> 0x)
void printWithLeadingZero(int value) {
  if (value < 10) {
    lcd.print("0");
  }
  lcd.print(value);
}
// Hàm điều khiển LED, SERVO bằng BUTTON
void checkButtons() {
  //Nếu nhấn LED BUTTON thì toggle LED
  // if (digitalRead(BTN_LED) == LOW) {
  //   ledStatus = !ledStatus;
  //   digitalWrite(LED_PIN, ledStatus ? HIGH : LOW);
  //   Serial.print("Button LED pressed - LED status: ");
  //   Serial.println(ledStatus ? "ON" : "OFF");
  //   ledStatus? ws.textAll("LEDStatus_ON") : ws.textAll("LEDStatus_OFF");
  //   delay(500);
  // }
  //Nếu nhấn SERVO BUTTON thì toggle SERVO
  if (digitalRead(BTN_SERVO) == LOW) {
    servoStatus ? servoAngle = 180 :  servoAngle = 0;
    myservo.write(servoAngle);
    servoStatus = !servoStatus;
    Serial.print("Button Servo pressed - Servo status: ");
    Serial.println(servoStatus ? "ON" : "OFF");
    servoStatus? ws.textAll("DOORStatus_ON") : ws.textAll("DOORStatus_OFF");
    delay(500);
  }
  //Nếu nhấn ALARM BUTTON thì bật ALARM
  // if (digitalRead(BTN_ALARM) == LOW) {
  //   alarmStatus == true;
  //   Serial.println("ALARM_ON");
  //   ws.textAll("ALARMStatus_ON");
  //   delay(500);
  // }
  //Nếu nhấn MENU BUTTON thì hiển thị các hẹn giờ của thiết bi lên LCD
  if (digitalRead(BTN_MENU) == LOW) {
    isMenu = true;
    switch(field){
      case 0: 
        {
          printStatus("LED");
          field += 1;
        }
        break;
      case 1: 
        {
          printStatus("DOOR");
          field += 1;
        }
        break;
      case 2: 
        {
          printStatus("ALARM");
          field += 1;
        }
        break;
      case 3: 
        {
          lcd.clear();
          updateLCD();
          field = 0;
          isMenu = false;
        }
        break;
    }
    delay(500);
  }
}
// Hàm in giá trị của các thiết bị lên LCD
void printStatus(String device){
    lcd.clear();
    lcd.setCursor(0, 0);
    if (device == "LED")
    {
      lcd.print("LED  :  ");
      if (currentTime < ledAlarm){
        printWithLeadingZero(ledAlarm.hour());
        lcd.print(":");
        printWithLeadingZero(ledAlarm.minute());
        lcd.print(":");
        printWithLeadingZero(ledAlarm.second());
        lcd.setCursor(0, 1);
        ledStatus ? lcd.print("ON ") : lcd.print("OFF");
        lcd.print("   ");
        printWithLeadingZero(ledAlarm.day());
        lcd.print("/");
        printWithLeadingZero(ledAlarm.month());
        lcd.print("/");
        lcd.print(ledAlarm.year());
        } else {
          lcd.print("IS NOT");
          lcd.setCursor(0, 1);
          ledStatus ? lcd.print("ON ") : lcd.print("OFF");
          lcd.print("     SETTING");
        }
    }
    else if (device == "DOOR") {
      lcd.print("DOOR :  ");
      if (currentTime < doorAlarm){
        printWithLeadingZero(doorAlarm.hour());
        lcd.print(":");
        printWithLeadingZero(doorAlarm.minute());
        lcd.print(":");
        printWithLeadingZero(doorAlarm.second());
        lcd.setCursor(0, 1);
        servoStatus ? lcd.print("ON ") : lcd.print("OFF");
        lcd.print("   ");
        printWithLeadingZero(doorAlarm.day());
        lcd.print("/");
        printWithLeadingZero(doorAlarm.month());
        lcd.print("/");
        lcd.print(doorAlarm.year());
        } else {
          lcd.print("IS NOT");
          lcd.setCursor(0, 1);
          servoStatus ? lcd.print("ON ") : lcd.print("OFF");
          lcd.print("     SETTING");
        }
    }
    else if (device == "ALARM") {
      lcd.print("ALARM:  ");
      if (currentTime < alarmUp){
        printWithLeadingZero(alarmUp.hour());
        lcd.print(":");
        printWithLeadingZero(alarmUp.minute());
        lcd.print(":");
        printWithLeadingZero(alarmUp.second());
        lcd.setCursor(0, 1);
        alarmStatus ? lcd.print("ON ") : lcd.print("OFF");
        lcd.print("   ");
        printWithLeadingZero(alarmUp.day());
        lcd.print("/");
        printWithLeadingZero(alarmUp.month());
        lcd.print("/");
        lcd.print(alarmUp.year());
        } else {
          lcd.print("IS NOT");
          lcd.setCursor(0, 1);
          alarmStatus ? lcd.print("ON ") : lcd.print("OFF");
          lcd.print("     SETTING");
        }
    }
}
// Hàm báo hiệu đến giờ hẹn của các thiết bị
void timeUP(){
  if(currentTime == ledAlarm) {
    field = 4;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LED ON!!!");
    digitalWrite(LED_PIN, HIGH);
    ledStatus = true;
    Serial.println("LED_ON");
    ws.textAll("LEDStatus_ON");
    ws.textAll("LED_DONE");
    delay(2000);
    ledAlarm = DateTime(2000, 0, 0, 0, 0, 0);
    field = 0;
    isMenu = false;
  }  
  if (currentTime == doorAlarm) {
    field = 4;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DOOR OPEN!!!");
    printStatus("DOOR");
    myservo.write(180);
    servoStatus = true;
    Serial.println("DOOR_ON");
    ws.textAll("DOOR_DONE");
    delay(2000);
    doorAlarm = DateTime(2000, 0, 0, 0, 0, 0);
    field = 0;
    isMenu = false;
  }  
  if (currentTime == alarmUp) {
    field = 4;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ALARM UP!!!");
    printStatus("ALARM");
    ws.textAll("ALARMStatus_ON");
    ws.textAll("ALARM_DONE");
    alarmStatus = true;
    delay(2000);
    alarmUp = DateTime(2000, 0, 0, 0, 0, 0);
    field = 0;
    isMenu = false;
    }
}

