/*
 * EN2853: Embedded Systems and Applications
 * Programming Assignment 2 - Medibox Enhancements
 * Name: Abeysinghe G. A. I. N. M.
 * Index No: 220009H
 * Date: April 6, 2025
 * Description: Controls a Medibox with LDR, DHT11, servo motor, and Node-RED dashboard
 * Designed for Wokwi simulation with ESP32
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <cmath>
#include <ESP32Servo.h>

// OLED Display Parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Hardware Pins
#define BUZZER 5
#define LED_1 15
#define LED_2 4
#define PB_CANCEL 25
#define PB_OK 26
#define PB_UP 27
#define PB_DOWN 13
#define DHTPIN 12
#define LDR_PIN 33
#define SERVO_PIN 14

// Time Settings
#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET 19800 // 5 hours 30 minutes in seconds (5 * 3600 + 30 * 60)
#define UTC_OFFSET_DST 0 // No daylight saving time in Sri Lanka

// Objects
DHTesp dhtSensor;                   // DHT11 sensor
WiFiClient espClient;               // WiFi client
PubSubClient mqttClient(espClient); // MQTT client
Servo servoMotor;                   // Servo motor

// Timekeeping Variables
int hours, minutes, seconds, day, month, year;
int alarm_hours[] = {-1, -1};
int alarm_minutes[] = {-1, -1};
bool snooze_active = false;
unsigned long snooze_time = 0;
int LED2_blink;
const int MAX_ADC_VALUE = 4095; // ESP32 ADC max in Wokwi
const int MIN_ADC_VALUE = 0;    // ESP32 ADC min in Wokwi

// Light Intensity Variables
unsigned long lastSamplingTime = 0;  // LDR sampling time
unsigned long lastSendTime = 0;      // LDR sending time
unsigned long last_servo_update = 0; // Servo update time
unsigned long lastPublishTime = 0;   // DHT publish time
float lightReading_sum = 0;          // Sum for averaging LDR readings
float normalized_LDR_Value = 0;      // Normalized LDR value (0-1)
int sample_count = 0;                // Track number of LDR samples

// Dashboard Variables
bool LED1_on = false;                                // LED state
int LDR_sampling_interval = 1 * 1000; // Sample every 1 second
int LDR_sending_interval = 5 * 1000;  // Send every 5 seconds
unsigned long lastSamplingTime = 0;   // Last LDR sampling time
unsigned long lastSendTime = 0;       // Last LDR sending time
float lightReading_sum = 0;           // Sum for averaging LDR readings
int sample_count = 0;                 // Number of LDR samples           
bool scheduled = false;                              // Scheduled state
int DHT_sending_interval = 5000;                     // DHT sending interval (ms)
int servo_update_interval = 2000;                    // Servo update interval (ms)
int sc_hours, sc_minutes, sc_day, sc_month, sc_year; // Scheduled time
float min_angle = 30;                                // Servo minimum angle
float ctrl_factor = 0.75;                            // Control factor
float ideal_temp = 30;                               // Ideal temperature
bool sys_active = true;                              // System active state

// MQTT Topics

const char *main_switch_topic = "220009/on_off";         // System on/off
const char *LDR_data_send = "220009/ldr";                // LDR average intensity
const char *temp_data_send = "220009/temp";              // Temperature data
const char *sampling_inter_receive = "220009/ts";        // Sampling interval
const char *sending_inter_receive = "220009/tu";         // Sending interval
const char *min_angle_receive = "220009/theta_offset";   // Servo minimum angle
const char *ctrl_factor_receive = "220009/gamma";        // Control factor
const char *ideal_temp_receive = "220009/tmed";          // Ideal temperature

// Unique MQTT Client ID
String clientId = "esp1254785458" + String(random(0, 1000));
const char *esp_client = clientId.c_str();

char tempAr[6]; // Temperature string buffer
char humAr[6];  // Humidity string buffer

// Print text on OLED
void print_line(String text, int x, int y, int size) {
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.println(text);
}

// Display time, temperature, and humidity
void print_time_now() {
  display.clearDisplay();
  char text[9];
  sprintf(text, "%02d:%02d:%02d", hours, minutes, seconds);
  print_line(text, 20, 0, 2);
  char date[11];
  sprintf(date, "%04d - %02d - %02d", year, month, day);
  print_line(date, 0, 20, 1);

  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  print_line("T = " + String(data.temperature) + "C H = " + String(data.humidity) + "%", 0, 30, 1);

  boolean temp_warning = data.temperature < 24 || data.temperature > 32;
  boolean humidity_warning = data.humidity < 65 || data.humidity > 80;

  if (temp_warning)
    print_line("Temperature Warning!", 0, 40, 1);
  if (humidity_warning)
    print_line("Humidity Warning!", 0, 50, 1);

  display.display();

  if (temp_warning || humidity_warning) {
    digitalWrite(LED_1, !digitalRead(LED_1));
    if (LED2_blink + 500 < millis()) {
      digitalWrite(LED_2, !digitalRead(LED_2));
      digitalWrite(LED_1, !digitalRead(LED_1));
      LED2_blink = millis();
      tone(BUZZER, 330);
      delay(300);
      noTone(BUZZER);
      delay(50);
    }
  } else {
    digitalWrite(LED_2, LOW);
    digitalWrite(LED_1, LOW);
  }

  if (LED1_on) {
    digitalWrite(LED_1, HIGH);
  } else {
    digitalWrite(LED_1, LOW);
  }
}

// Play melody on buzzer
void play_melody() {
  int melody[] = {262, 294, 330, 262, 330, 392, 440};
  int noteDurations[] = {300, 300, 300, 300, 400, 400, 500};

  for (int i = 0; i < 7; i++) {
    tone(BUZZER, melody[i]);
    delay(noteDurations[i]);
    noTone(BUZZER);
    delay(50);
  }
}

// Ring alarm
void ring_alarm() {
  display.clearDisplay();
  print_line("MEDICINE", 25, 5, 2);
  print_line("TIME!", 40, 20, 2);
  print_line("OK: Snooze (5 min)", 10, 40, 1);
  print_line("Cancel: Stop", 10, 50, 1);
  display.display();

  digitalWrite(LED_1, HIGH);

  while (true) {
    play_melody();
    digitalWrite(LED_1, !digitalRead(LED_1));

    if (digitalRead(PB_OK) == LOW) {
      snooze_active = true;
      snooze_time = millis() + 300000;
      display.clearDisplay();
      print_line("Snoozed for 5 min", 10, 20, 1);
      display.display();
      delay(1000);
      break;
    }

    if (digitalRead(PB_CANCEL) == LOW) {
      display.clearDisplay();
      print_line("Alarm", 10, 20, 2);
      print_line("Stopped !", 10, 40, 2);
      display.display();
      delay(1000);
      break;
    }
  }

  noTone(BUZZER);
  digitalWrite(LED_1, LOW);
  display.clearDisplay();
  display.display();
}

// Update time and check alarms
void update_time_with_check_alarm() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
    return;
  hours = timeinfo.tm_hour;
  minutes = timeinfo.tm_min;
  seconds = timeinfo.tm_sec;
  day = timeinfo.tm_mday;
  month = timeinfo.tm_mon + 1;
  year = timeinfo.tm_year + 1900;
  print_time_now();

  for (int i = 0; i < 2; i++) {
    if (hours == alarm_hours[i] && minutes == alarm_minutes[i] && seconds == 0) {
      ring_alarm();
    }
  }

  if (scheduled) {
    if (hours == sc_hours && minutes == sc_minutes && seconds == 0 &&
        day == sc_day && month == sc_month && year == sc_year) {
      ring_alarm();
    }
  }

  if (snooze_active && millis() >= snooze_time) {
    snooze_active = false;
    ring_alarm();
  }
}

// Set time zone
void set_time() {
  int new_hours = UTC_OFFSET / 3600;
  int new_minutes = UTC_OFFSET % 3600 / 60;
  bool setting_minutes = false;

  while (true) {
    display.clearDisplay();
    print_line("Time Zone:", 0, 0, 2);
    print_line(String(new_hours) + ":" + (new_minutes < 10 ? "0" : "") + String(new_minutes), 20, 20, 2);
    print_line(setting_minutes ? "Adjusting Minutes" : "Adjusting Hours", 0, 40, 1);
    print_line("Press Cancel to Exit", 0, 50, 1);
    display.display();

    if (digitalRead(PB_UP) == LOW) {
      if (!setting_minutes) {
        new_hours = (new_hours == 14) ? -12 : new_hours + 1;
      } else {
        new_minutes = (new_minutes + 1) % 60;
      }
      delay(200);
    }

    if (digitalRead(PB_DOWN) == LOW) {
      if (!setting_minutes) {
        new_hours = (new_hours == -12) ? 14 : new_hours - 1;
      } else {
        new_minutes = (new_minutes == 0) ? 59 : new_minutes - 1;
      }
      delay(200);
    }

    if (digitalRead(PB_OK) == LOW) {
      delay(200);
      if (!setting_minutes) {
        setting_minutes = true;
      } else {
        int new_utc_offset = new_hours * 3600 + new_minutes * 60;
        configTime(new_utc_offset, UTC_OFFSET_DST, NTP_SERVER);
        display.clearDisplay();
        print_line("Time Zone", 0, 0, 2);
        print_line("Updated..!", 0, 20, 2);
        display.display();
        delay(1000);
        break;
      }
    }
    if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      break;
    }
  }
}

// Set alarm
void set_alarm() {
  int alarm_index = 0;
  int new_hours = hours;
  int new_minutes = minutes;
  bool setting_minutes = false;
  bool selecting_alarm = true;

  while (true) {
    display.clearDisplay();
    if (selecting_alarm) {
      print_line("___Select Alarm___", 5, 0, 1);
      print_line((alarm_index == 0 ? "--> " : "   ") + String("Alarm 1"), 0, 20, 1);
      print_line((alarm_index == 1 ? "--> " : "   ") + String("Alarm 2"), 0, 30, 1);
    } else {
      print_line("_Alarm " + String(alarm_index + 1) + "_", 10, 0, 2);
      print_line(String(new_hours) + ":" + (new_minutes < 10 ? "0" : "") + String(new_minutes), 30, 20, 2);
      print_line(setting_minutes ? "Adjusting Minutes" : "Adjusting Hours", 0, 40, 1);
    }
    display.display();

    if (digitalRead(PB_UP) == LOW) {
      if (selecting_alarm) {
        alarm_index = (alarm_index == 0) ? 1 : 0;
      } else {
        if (!setting_minutes)
          new_hours = (new_hours + 1) % 24;
        else
          new_minutes = (new_minutes + 1) % 60;
      }
      delay(200);
    }

    if (digitalRead(PB_DOWN) == LOW) {
      if (selecting_alarm) {
        alarm_index = (alarm_index == 0) ? 1 : 0;
      } else {
        if (!setting_minutes)
          new_hours = (new_hours == 0) ? 23 : new_hours - 1;
        else
          new_minutes = (new_minutes == 0) ? 59 : new_minutes - 1;
      }
      delay(200);
    }

    if (digitalRead(PB_OK) == LOW) {
      delay(200);
      if (selecting_alarm) {
        selecting_alarm = false;
      } else if (!setting_minutes) {
        setting_minutes = true;
      } else {
        alarm_hours[alarm_index] = new_hours;
        alarm_minutes[alarm_index] = new_minutes;
        break;
      }
    }

    if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      break;
    }
  }
}

// View alarms
void view_alarms() {
  while (true) {
    display.clearDisplay();
    print_line("___Current Alarms___", 0, 0, 1);

    for (int i = 0; i < 2; i++) {
      if (alarm_hours[i] != -1) {
        String alarmText = "Alarm " + String(i + 1) + ": " +
                           String(alarm_hours[i]) + ":" +
                           (alarm_minutes[i] < 10 ? "0" : "") + String(alarm_minutes[i]);
        print_line(alarmText, 0, 15 + (i * 10), 1);
      } else {
        print_line("Alarm " + String(i + 1) + ": None", 0, 15 + (i * 10), 1);
      }
    }

    print_line("Press Cancel key to exit", 0, 45, 1);
    display.display();

    if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      break;
    }
  }
}

// View scheduled time
void view_schedule() {
  while (true) {
    display.clearDisplay();
    print_line("___Scheduled___", 0, 0, 1);
    if (scheduled) {
      String scheduleText = "Scheduled: " + String(sc_hours) + ":" +
                           (sc_minutes < 10 ? "0" : "") + String(sc_minutes) + " on " +
                           String(sc_day) + "/" + String(sc_month) + "/" + String(sc_year);
      print_line(scheduleText, 0, 20, 1);
    } else {
      print_line("No Schedule Set", 0, 20, 1);
    }
    print_line("Press Cancel key to exit", 0, 50, 1);
    display.display();

    if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      break;
    }
  }
}

// Delete alarm
void delete_alarm() {
  int alarm_index = 0;

  while (true) {
    display.clearDisplay();
    print_line("___Delete Alarm___", 0, 0, 1);

    for (int i = 0; i < 2; i++) {
      if (i == alarm_index) {
        print_line("--> Alarm " + String(i + 1) + ": " +
                   String(alarm_hours[i]) + ":" +
                   (alarm_minutes[i] < 10 ? "0" : "") + String(alarm_minutes[i]),
                   0, 15 + (i * 10), 1);
      } else {
        print_line("Alarm " + String(i + 1) + ": " +
                   String(alarm_hours[i]) + ":" +
                   (alarm_minutes[i] < 10 ? "0" : "") + String(alarm_minutes[i]),
                   0, 15 + (i * 10), 1);
      }
    }

    print_line("PB_OK to Delete", 0, 40, 1);
    print_line("PB_CANCEL to Exit", 0, 50, 1);
    display.display();

    if (digitalRead(PB_UP) == LOW) {
      alarm_index = (alarm_index == 0) ? 1 : 0;
      delay(200);
    }
    if (digitalRead(PB_DOWN) == LOW) {
      alarm_index = (alarm_index == 1) ? 0 : 1;
      delay(200);
    }

    if (digitalRead(PB_OK) == LOW) {
      alarm_hours[alarm_index] = -1;
      alarm_minutes[alarm_index] = -1;
      display.clearDisplay();
      print_line("Alarm", 0, 20, 2);
      print_line("Deleted!", 0, 40, 2);
      display.display();
      delay(1000);
      break;
    }

    if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      break;
    }
  }
}

// Navigate main menu
void go_to_menu() {
  int menu_option = 1;

  while (true) {
    display.clearDisplay();

    if (menu_option == 1) {
      print_line("--> Set Time Zone", 0, 0, 1);
    } else {
      print_line("1. Set Time Zone", 0, 0, 1);
    }

    if (menu_option == 2) {
      print_line("--> Set Alarms", 0, 10, 1);
    } else {
      print_line("2. Set Alarms", 0, 10, 1);
    }

    if (menu_option == 3) {
      print_line("--> View Alarms", 0, 20, 1);
    } else {
      print_line("3. View Alarms", 0, 20, 1);
    }

    if (menu_option == 4) {
      print_line("--> View Scheduled", 0, 30, 1);
    } else {
      print_line("4. View Scheduled", 0, 30, 1);
    }

    if (menu_option == 5) {
      print_line("--> Delete Alarm", 0, 40, 1);
    } else {
      print_line("5. Delete Alarm", 0, 40, 1);
    }

    if (menu_option == 6) {
      print_line("--> Exit", 0, 50, 1);
    } else {
      print_line("6. Exit", 0, 50, 1);
    }

    display.display();

    if (digitalRead(PB_UP) == LOW) {
      menu_option = (menu_option == 1) ? 6 : menu_option - 1;
      delay(200);
    }
    if (digitalRead(PB_DOWN) == LOW) {
      menu_option = (menu_option == 6) ? 1 : menu_option + 1;
      delay(200);
    }

    if (digitalRead(PB_OK) == LOW) {
      delay(200);
      if (menu_option == 1)
        set_time();
      if (menu_option == 2)
        set_alarm();
      if (menu_option == 3)
        view_alarms();
      if (menu_option == 4)
        view_schedule();
      if (menu_option == 5)
        delete_alarm();
      if (menu_option == 6)
        break;
    }

    if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      break;
    }
  }
}

// Setup WiFi
void setupWifi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin("Wokwi-GUEST", "");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi");
  Serial.println("IP address: " + WiFi.localIP().toString());
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
}

// Connect to MQTT broker
void connectToBroker() {
  const char *mqtt_server = "test.mosquitto.org";
  mqttClient.setServer(mqtt_server, 1883);

  int attempts = 0;
  while (!mqttClient.connected() && attempts < 5) {
    Serial.println("Connecting to MQTT broker...");
    if (mqttClient.connect(esp_client)) {
      Serial.println("Connected to MQTT broker");
      mqttClient.subscribe(main_switch_topic);
      mqttClient.subscribe(sampling_inter_receive);
      mqttClient.subscribe(sending_inter_receive);
      mqttClient.subscribe(min_angle_receive);
      mqttClient.subscribe(ctrl_factor_receive);
      mqttClient.subscribe(ideal_temp_receive);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" - retrying in 1 second");
      delay(1000);
      attempts++;
    }
  }
  if (!mqttClient.connected()) {
    Serial.println("Failed to connect to MQTT broker after 5 attempts");
  }
}

// Power on/off system
void power_on_off(bool power_on) {
  if (power_on) {
    display.clearDisplay();
    print_line("Medibox \nTurning \nON...", 0, 0, 2);
    display.display();
    delay(2000);
    configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
  } else {
    display.clearDisplay();
    print_line("Medibox \nTurning \nOFF...", 0, 0, 2);
    display.display();
    delay(3000);
    display.clearDisplay();
    display.display();
  }
}

// Handle MQTT messages
void receiveCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
    Serial.print((char)payload[i]);
  }
  message[length] = '\0';
  Serial.println();

  if (strcmp(topic, main_switch_topic) == 0) {
    if (strcmp(message, "ON") == 0) {
      Serial.println("Turning device ON");
      power_on_off(true);
      sys_active = true;
    } else if (strcmp(message, "OFF") == 0) {
      Serial.println("Turning device OFF");
      power_on_off(false);
      sys_active = false;
    }
  } else if (strcmp(topic, sampling_inter_receive) == 0) {
    LDR_sampling_interval = atoi(message) * 1000; // Seconds to milliseconds
    Serial.println("Updated sampling interval: " + String(LDR_sampling_interval / 1000) + "s");
  } else if (strcmp(topic, sending_inter_receive) == 0) {
    LDR_sending_interval = atoi(message) * 1000; // Seconds to milliseconds
    Serial.println("Updated sending interval: " + String(LDR_sending_interval / 1000) + "s");
  } else if (strcmp(topic, min_angle_receive) == 0) {
    min_angle = strtod(message, nullptr);
    Serial.println("Updated min angle: " + String(min_angle));
  } else if (strcmp(topic, ctrl_factor_receive) == 0) {
    ctrl_factor = strtod(message, nullptr);
    Serial.println("Updated control factor: " + String(ctrl_factor));
  } else if (strcmp(topic, ideal_temp_receive) == 0) {
    ideal_temp = strtod(message, nullptr);
    Serial.println("Updated ideal temperature: " + String(ideal_temp));
  }

  
}

// Publish DHT data
void publish_DHT_data() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  dtostrf(data.temperature, 1, 2, tempAr);
  dtostrf(data.humidity, 1, 2, humAr);
  mqttClient.publish("220009/MEDI_TEMP", tempAr, true);
  mqttClient.publish("220009/MEDI_HUM", humAr, true);
}

// Setup pin modes
void setupPinModes() {
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(PB_CANCEL, INPUT_PULLUP);
  pinMode(PB_OK, INPUT_PULLUP);
  pinMode(PB_UP, INPUT_PULLUP);
  pinMode(PB_DOWN, INPUT_PULLUP);
}

// Sample LDR
void sampleLightIntensity() {
    int rawValue = analogRead(LDR_PIN);
    normalized_LDR_Value = (float)(MAX_ADC_VALUE - rawValue) / (MAX_ADC_VALUE - MIN_ADC_VALUE);
    lightReading_sum += normalized_LDR_Value;
    sample_count++;
    Serial.print("Raw LDR: ");
    Serial.print(rawValue);
    Serial.print(" Normalized: ");
    Serial.println(normalized_LDR_Value);
}

// Send average LDR intensity
void sendAverageIntensity() {
    float average = (sample_count > 0) ? lightReading_sum / sample_count : 0;
    char valueStr[10];
    dtostrf(average, 1, 4, valueStr);
    mqttClient.publish(LDR_data_send, valueStr);
    Serial.print("Published LDR average: ");
    Serial.println(valueStr);
    lightReading_sum = 0;
    sample_count = 0;
}

// Calculate servo angle
double servo_angle() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  double T = data.temperature;
  double ts_seconds = LDR_sampling_interval / 1000.0; // Convert ms to seconds
  double tu_seconds = LDR_sending_interval / 1000.0; // Convert ms to seconds
  double ratio = ts_seconds / tu_seconds; // Note: t_s / t_u as per equation
  double log_term = (ratio > 0) ? std::log(ratio) : 0; // Prevent negative/zero log
  // Equation: theta = offset + (180 - offset) * I * gamma * ln(t_s/t_u) * (T/T_med)
  double theta = min_angle +
                 (180.0 - min_angle) *
                 normalized_LDR_Value *
                 ctrl_factor *
                 log_term *
                 (T / ideal_temp);
  return theta;
}

void setup() {
  Serial.begin(115200);
  setupPinModes();
  dhtSensor.setup(DHTPIN, DHTesp::DHT22); // Use DHT22 as per assignment

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while (true);
  }

  display.display();
  delay(2000);
  setupWifi();

  mqttClient.setCallback(receiveCallback);
  connectToBroker();

  servoMotor.setPeriodHertz(50);
  servoMotor.attach(SERVO_PIN, 500, 2400);
}

void loop() {
  if (!mqttClient.connected())
    connectToBroker();
  mqttClient.loop();

  if (!sys_active) {
    delay(3000);
    return;
  }

  update_time_with_check_alarm();

  unsigned long currentTime = millis();

  if (digitalRead(PB_OK) == LOW) {
    delay(200);
    go_to_menu();
  }

  if (currentTime - lastPublishTime >= DHT_sending_interval) {
    publish_DHT_data();
    lastPublishTime = currentTime;
  }

  if (currentTime - lastSamplingTime >= LDR_sampling_interval) {
    lastSamplingTime = currentTime;
    sampleLightIntensity();
}

  if (currentTime - lastSendTime >= LDR_sending_interval) {
    lastSendTime = currentTime;
    sendAverageIntensity();
}

  if (currentTime - last_servo_update >= servo_update_interval) {
    last_servo_update = currentTime;
    double angle = servo_angle();
    servoMotor.write(constrain(angle, 0, 180));

    // Publish servo angle to MQTT
    char angleStr[8];
    dtostrf(angle, 1, 2, angleStr); // Convert to string
    mqttClient.publish("220009/servo_angle", angleStr);
  }
}