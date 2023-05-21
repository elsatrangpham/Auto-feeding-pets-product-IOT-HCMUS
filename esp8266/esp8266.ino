#include <ESP8266WiFi.h>
#include "PubSubClient.h"
#include <Servo.h>
#include "ThingSpeak.h"

#define button_Pin D5

#define buzzer_Pin D6
#define sensor_water_Pin A0
#define sensor_food_Pin D1
#define sensor_movement_Pin D4

#define servo_water_Pin D7
#define servo_food_Pin D2

int frequency = 10;
int state_button = HIGH;
int countStatusOf2 = -1;

Servo servo_water, servo_food;
uint8_t button, sensor_water, sensor_food, sensor_movement;
uint8_t count_water = 0, count_food = 0;

unsigned long time_start_food, time_start_buzzer, set_time = 10000;
boolean already_buzzer = false, already_fill = false, set_time_food = false, set_time_buzzer = false;
boolean already_fill_bowls = false, isOverWater = false;

const char* ssid = "Elsatrangpham’s iphone";
const char* password = "19032002";
const char* mqttServer = "test.mosquitto.org";
int port = 1883;

//ThingSpeakSetting
unsigned long myChannelNumber = 1822680;  // Replace the 0 with your channel number, channel của bạn
const char * myWriteAPIKey = "020GVFEH7H7I9A4K";    // Paste your ThingSpeak Write API Key between the quotes, write API key

WiFiClient espClient;
PubSubClient client(espClient);

void mqttReconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("20127228")) {
      Serial.println(" connected");
      //-------- Truyền từ Node-Red -> Thiết bị-------------
      client.subscribe("20127228/set_time");
    } else {
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.println(topic);
  String strMessage;
  for (int i = 0; i < length; i++) {
    strMessage += (char)message[i];
  }
  Serial.println(strMessage);

  if (strMessage == "medium") set_time = 10000;
  else if (strMessage == "large") set_time = 15000;
  else if (strMessage == "small") set_time = 5000;
}

void wifiConnect() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  // Led ngoài board mạch
  pinMode(LED_BUILTIN, OUTPUT);
  //Loa
  pinMode(buzzer_Pin, OUTPUT);
  //Nút bấm
  pinMode(button_Pin, INPUT_PULLUP);
  //Sensor water
  pinMode(sensor_water_Pin, INPUT);
  //Sensor food
  pinMode(sensor_food_Pin, INPUT);
  //Sensor movement
  pinMode(sensor_movement_Pin, INPUT);
  //Servo water
  servo_water.attach(13); // Chân D7
  servo_water.write(0);  // nhập góc xoay của servo từ 0 - 180 độ
  //Servo food
  servo_food.attach(4); // Chân D2
  servo_food.write(0);  // nhập góc xoay của servo từ 0 - 180 độ

  Serial.begin(115200);

  //Kết nối WiFi
  Serial.println("Connecting to WiFi");
  wifiConnect();
  //Mqtt(mosquitto) - kết nối trung gian
  client.setServer(mqttServer, port);
  client.setCallback(callback);

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  ThingSpeak.begin(espClient);
}

// the loop function runs over and over again forever
void loop() {
  delay(100); 
  
  if (!client.connected()) {
    mqttReconnect();
  }
  client.loop();

  //-------- Biến truyền thiết bị -> node-red
  char buffer_water[50];
  char buffer_food[50];
  char buffer_already_fill[50];
  char buffer_already_fill_bowls[50];
  
  //------- Đọc cảm biến nước -----------------
  sensor_water = analogRead(sensor_water_Pin);
  //------- Đọc cảm biến đồ ăn ----------------
  sensor_food = digitalRead(sensor_food_Pin);
  //------- Đọc cảm biến chuyển động ----------
  sensor_movement = digitalRead(sensor_movement_Pin);

  sprintf(buffer_water, "%d", sensor_water);
  sprintf(buffer_food, "%d", sensor_food);
  sprintf(buffer_already_fill, "%d", already_fill);
  sprintf(buffer_already_fill_bowls, "%d", already_fill_bowls);
  
  //-------- Truyền từ thiết bị -> Node-Red-------------
  client.publish("20127228/sensor_water", buffer_water);
  client.publish("20127228/sensor_food", buffer_food);
  client.publish("20127228/already_fill", buffer_already_fill);
  client.publish("20127228/already_fill_bowls", buffer_already_fill_bowls);

  delay(100);
  
  Serial.println(sensor_water);
  
  //TH: Khi đồ ăn || nước ở trong bát HẾT và chưa đổ lần nào
  if ((sensor_water <= 80 || sensor_food == HIGH) && already_fill == false) {
    already_fill_bowls = true;
    time_start_buzzer = millis();
    Serial.println("Het thuc an trong bat");

    //Th1: Đồ ăn và nước đều hết
    if (sensor_water <= 80 && sensor_food == HIGH) {
      Serial.println("bat servo ca 2");
      
      //Servo nâng lên
      servo_water.write(180);
      servo_food.write(180);
      countStatusOf2 = 2;
      // Bắt đầu set thời gian mở servo food
      if (set_time_food == false){
        time_start_food = millis(); // Thời gian bắt đầu đổ đồ ăn
        set_time_food = true; //Đã set tgian của food
      }
    }
    //Th2: Khi chỉ có nước hết
    else if (sensor_water <= 80) {
      Serial.println("bat servo nuoc");
      servo_water.write(180);
      isOverWater = true;
      countStatusOf2 = 0;

    }
    // Khi chỉ có đồ ăn hết
    else if (sensor_food == HIGH) {
      Serial.println("bat servo food");
      servo_food.write(180);
      countStatusOf2 = 1;

      // Bắt đầu set thời gian mở servo food
      if (set_time_food == false){
        time_start_food = millis(); // Thời gian bắt đầu đổ đồ ăn
        set_time_food = true; //Đã set tgian của food
      }
    }
  }

  //TH: Khi đồ ăn || nước ở trong bát HẾT và đã đổ -> đồ ăn trong bình hết
  else if ((sensor_water <= 80 || sensor_food == HIGH) && already_fill == true ){
    servo_food.write(0);
    
    already_fill_bowls = false;
    Serial.println("Hết đồ ăn trong bình");
    if (sensor_water <= 80 && sensor_food == HIGH) {
      Serial.println("count status = 2");
      countStatusOf2 = 2;
    }
    else if (sensor_water > 80 && sensor_food == HIGH) {
      Serial.println("count status = 1");
      countStatusOf2 = 1;
    }
    else if (sensor_water <= 80 && sensor_food == LOW) {
      Serial.println("count status = 0");
      countStatusOf2 = 0;
    }
    
    if(state_button == HIGH && already_buzzer == false){
      //Loa kêu
      tone(buzzer_Pin, frequency);
      set_time_buzzer = true;
    }
    
    // Nếu mà loa đã kêu trước đó -> Không có chủ ở nhà
    if(already_buzzer == true){
      noTone(buzzer_Pin);
      Serial.println("Đồ ăn || nước hết (Không có chủ)");
    }
  }

  //TH: BÌNH THƯỜNG có đồ ăn và nước
  else if ((sensor_water > 80 && sensor_food == LOW)){
    already_fill_bowls = false;
  
    Serial.println("Refill");
    state_button = HIGH;
    noTone(buzzer_Pin); // Loa tắt
    already_fill = false; // Set lại chu kì mới
    already_buzzer = false; // Set lại trạng thái loa chưa kêu
    if (countStatusOf2 == 0) {
        Serial.println("Nuoc");
        count_water = count_water + 1;
        int x =  ThingSpeak.setField(1, count_food);
        int y =  ThingSpeak.setField(2, count_water); //setField(field, value)
        int z = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        Serial.println(z);
        isOverWater = false;
    }
    else if (countStatusOf2 == 1) {
        Serial.println("Food");
          count_food = count_food + 1;
        int x =  ThingSpeak.setField(1, count_food);
        int y =  ThingSpeak.setField(2, count_water);//setField(field, value)
        int z = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        Serial.println(z);
    }
    else if (countStatusOf2 == 2) {
        Serial.println("Thieu ca 2");
          count_water = count_water + 1;
          count_food = count_food + 1;
        int x =  ThingSpeak.setField(1, count_food); //setField(field, value)
        int y =  ThingSpeak.setField(2, count_water); //setField(field, value)
        int z = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        Serial.println(z);
      }
    countStatusOf2 = -1;
  }

  
  // Đóng servo food
  if (set_time_food == true) {
    already_fill_bowls = false;
    if ((unsigned long)(millis() - time_start_food) > set_time) {
      
      servo_food.write(0);
      set_time_food = false; //Bỏ set tgian
        
      if (sensor_food == LOW){
            Serial.println("Dong servo Food");

          count_food = count_food + 1;
          int x =  ThingSpeak.setField(1, count_food); //setField(field, value)
          int y =  ThingSpeak.setField(2, count_food);
          int z = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
          Serial.println(z);
          already_fill = false;
        }  //Đã fill đồ ăn hoặc uống
      else {
        already_fill = true; //Đã fill đồ ăn hoặc uống
      }
    }
  }

  // Đóng servo water
  if (sensor_water > 80) {
    already_fill_bowls = false;
    servo_water.write(0);
    if (sensor_water > 80 || sensor_food == LOW) {
          if (isOverWater) {
              Serial.println("Dong servo Nuoc");
              count_water = count_water + 1;
              int x = ThingSpeak.setField(1, count_food);
              int y = ThingSpeak.setField(2, count_water); //setField(field, value)
              int z = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
              Serial.println(y);
              isOverWater = false;
            }
          already_fill = false;
      }  //Đã fill đồ ăn hoặc uống
    else already_fill = true; //Đã fill đồ ăn hoặc uống
  }
          
//  int x =  ThingSpeak.setField(1, count_food); //setField(field, value)
//  int z = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  // Tắt Loa
  if (set_time_buzzer == true) {
    already_fill_bowls = false;
        
    //Kiểm tra nút bấm
    button = digitalRead(button_Pin);
    // Nếu đã bấm nút
    if (button == LOW){
      servo_food.write(0);
      noTone(buzzer_Pin); // Loa tắt
      state_button = LOW; // Set trạng thái nút là đã bấm
      set_time_buzzer = false; //Bỏ set tgian
    }
    else if ((unsigned long)(millis() - time_start_buzzer) > 5000) {
      noTone(buzzer_Pin);
//      servo_food.write(0);
      set_time_buzzer = false; //Bỏ set tgian
      already_buzzer = true; //Đã fill đồ ăn hoặc uống
      time_start_buzzer = millis();
    }
  }

  // Ghi dữ liệu lên nhiều field
//  int x =  ThingSpeak.setField(1, sensor_food); //setField(field, value)
//  int y =  ThingSpeak.setField(2, sensor_water); //setField(field, value)
//  int z = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  
  
  //--------- Cảm biến chuyển động --------------------
//  if (sensor_movement == LOW)
//    Serial.println("Meo meo");
//  else
//    Serial.println("No meo meo");

}
