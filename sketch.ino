
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ==========================================
// 1. KHAI BAO CHAN KET NOI
// ==========================================

#define TRIG_PIN 14
#define ECHO_PIN 12
#define DS_PIN 21

#define PUMP_PIN 17
#define VALVE_PIN 16

#define TANK_HEIGHT 2.5

// ==========================================
// 2. WIFI VA MQTT
// ==========================================

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

// Cac topic MQTT - website phai dung dung cac topic nay
const char* TOPIC_SENSOR =
  "aquaculture/lab4/esp32_a8f31c/sensor";

const char* TOPIC_CONTROL =
  "aquaculture/lab4/esp32_a8f31c/control";

const char* TOPIC_STATUS =
  "aquaculture/lab4/esp32_a8f31c/status";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ==========================================
// 3. KHOI TAO CAM BIEN
// ==========================================

OneWire oneWire(DS_PIN);
DallasTemperature sensors(&oneWire);

// ==========================================
// 4. CAU TRUC DU LIEU
// ==========================================

struct SensorData {
  float waterLevel;
  float temperature;
};

enum Action {
  IDLE,
  PUMP_50,
  PUMP_100,
  DRAIN
};

struct ControlCommand {
  bool manualMode;
  bool pump;
  bool valve;
};

struct DeviceStatus {
  bool manualMode;
  bool pump;
  bool valve;
};

Action currentAction = IDLE;

// ==========================================
// 5. QUEUE VA MUTEX
// ==========================================

QueueHandle_t dataQueue;
QueueHandle_t dataAlert;
QueueHandle_t controlQueue;
QueueHandle_t statusQueue;

SemaphoreHandle_t serialMutex;

// ==========================================
// 6. BIEN DIEU KHIEN
// ==========================================

unsigned long actionStart = 0;
unsigned long actionDuration = 0;

int previousZone = -1;

bool manualMode = false;
bool pumpState = false;
bool valveState = false;

// ==========================================
// 7. HAM DOC HC-SR04
// ==========================================

float readDistance() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration =
    pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1;
  }

  return duration * 0.0343 / 2.0;
}

// ==========================================
// 8. GUI TRANG THAI LEN QUEUE
// ==========================================

void sendDeviceStatus() {

  DeviceStatus status;

  status.manualMode = manualMode;
  status.pump = pumpState;
  status.valve = valveState;

  xQueueOverwrite(statusQueue, &status);
}

// ==========================================
// 9. HAM DIEU KHIEN TU DONG
// ==========================================

void startAction(Action action,
                 unsigned long duration) {

  analogWrite(PUMP_PIN, 0);
  digitalWrite(VALVE_PIN, LOW);

  pumpState = false;
  valveState = false;

  currentAction = action;
  actionStart = millis();
  actionDuration = duration;

  if (action == PUMP_50) {

    analogWrite(PUMP_PIN, 128);
    pumpState = true;

    Serial.println("BAT BOM 50% - 10 GIAY");
  }

  else if (action == PUMP_100) {

    analogWrite(PUMP_PIN, 255);
    pumpState = true;

    Serial.println("BAT BOM 100% - 20 GIAY");
  }

  else if (action == DRAIN) {

    digitalWrite(VALVE_PIN, HIGH);
    valveState = true;

    Serial.println("MO VAN XA - 10 GIAY");
  }

  sendDeviceStatus();
}

// ==========================================
// 10. HAM DUNG THIET BI
// ==========================================

void stopAction() {

  analogWrite(PUMP_PIN, 0);
  digitalWrite(VALVE_PIN, LOW);

  pumpState = false;
  valveState = false;

  currentAction = IDLE;
  actionDuration = 0;

  Serial.println("DA DUNG THIET BI");

  sendDeviceStatus();
}

// ==========================================
// 11. NHAN LENH MQTT
// ==========================================

void mqttCallback(char* topic,
                  byte* payload,
                  unsigned int length) {

  if (String(topic) != TOPIC_CONTROL) {
    return;
  }

  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("MQTT CONTROL: ");
  Serial.println(message);

  ControlCommand command;

  if (message.indexOf("\"mode\":\"MANUAL\"") >= 0) {
    command.manualMode = true;
  }
  else if (message.indexOf("\"mode\":\"AUTO\"") >= 0) {
    command.manualMode = false;
  }
  else {
    return;
  }

  command.pump =
    message.indexOf("\"pump\":1") >= 0;

  command.valve =
    message.indexOf("\"valve\":1") >= 0;

  xQueueSend(controlQueue, &command, 0);
}

// ==========================================
// 12. KET NOI WIFI
// ==========================================

void connectWiFi() {

  Serial.println("DANG KET NOI WIFI...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);

  while (WiFi.status() != WL_CONNECTED) {

    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WIFI DA KET NOI!");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ==========================================
// 13. KET NOI MQTT
// ==========================================

void reconnectMQTT() {

  while (!mqttClient.connected()) {

    Serial.println("DANG KET NOI MQTT...");

    if (mqttClient.connect("ESP32_AQUA_A8F31C")) {

      Serial.println("MQTT DA KET NOI!");

      mqttClient.subscribe(TOPIC_CONTROL);

      Serial.println("DA DANG KY TOPIC CONTROL");
    }

    else {

      Serial.print("MQTT LOI: ");
      Serial.println(mqttClient.state());

      vTaskDelay(pdMS_TO_TICKS(3000));
    }
  }
}

// ==========================================
// 14. TASK DOC DATA - CORE 1
// ==========================================

void TaskReadData(void *parameter) {

  SensorData data;

  for (;;) {

    float distance = readDistance();

    sensors.requestTemperatures();

    float temperature =
      sensors.getTempCByIndex(0);

    if (distance > 0 &&
        temperature != DEVICE_DISCONNECTED_C) {

      data.waterLevel =
        TANK_HEIGHT - distance / 100.0;

      if (data.waterLevel < 0) {
        data.waterLevel = 0;
      }

      if (data.waterLevel > TANK_HEIGHT) {
        data.waterLevel = TANK_HEIGHT;
      }

      data.temperature = temperature;

      xSemaphoreTake(serialMutex, portMAX_DELAY);

      Serial.println("\n===== DU LIEU CAM BIEN =====");

      Serial.print("Khoang cach: ");
      Serial.print(distance);
      Serial.println(" cm");

      Serial.print("Muc nuoc: ");
      Serial.print(data.waterLevel);
      Serial.println(" m");

      Serial.print("Nhiet do: ");
      Serial.print(data.temperature);
      Serial.println(" C");

      Serial.println("============================");

      xSemaphoreGive(serialMutex);

      xQueueSend(dataQueue, &data, 0);

      xQueueOverwrite(dataAlert, &data.waterLevel);
    }

    else {

      xSemaphoreTake(serialMutex, portMAX_DELAY);

      Serial.println("LOI DOC CAM BIEN!");

      if (distance <= 0) {
        Serial.println("Kiem tra HC-SR04");
      }

      if (temperature == DEVICE_DISCONNECTED_C) {
        Serial.println("Kiem tra DS18B20");
      }

      xSemaphoreGive(serialMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ==========================================
// 15. TASK TRUYEN DATA - CORE 0
// ==========================================

void TaskSendData(void *parameter) {

  SensorData receivedData;
  DeviceStatus status;

  connectWiFi();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(256);

  for (;;) {

    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
    }

    if (!mqttClient.connected()) {
      reconnectMQTT();
    }

    mqttClient.loop();

    // Nhan du lieu cam bien va gui len MQTT
    if (xQueueReceive(
          dataQueue,
          &receivedData,
          pdMS_TO_TICKS(100)) == pdTRUE) {

      char payload[128];

      snprintf(
        payload,
        sizeof(payload),
        "{\"waterLevel\":%.2f,\"temperature\":%.2f}",
        receivedData.waterLevel,
        receivedData.temperature
      );

      if (mqttClient.connected()) {

        mqttClient.publish(TOPIC_SENSOR, payload);

        Serial.print("DA GUI MQTT: ");
        Serial.println(payload);
      }
    }

    // Gui trang thai bom va van len website
    if (xQueueReceive(statusQueue, &status, 0) == pdTRUE) {

      char statusPayload[128];

      snprintf(
        statusPayload,
        sizeof(statusPayload),
        "{\"mode\":\"%s\",\"pump\":%d,\"valve\":%d}",
        status.manualMode ? "MANUAL" : "AUTO",
        status.pump ? 1 : 0,
        status.valve ? 1 : 0
      );

      if (mqttClient.connected()) {
        mqttClient.publish(TOPIC_STATUS, statusPayload);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ==========================================
// 16. TASK CANH BAO - CORE 1
// ==========================================

void TaskAlert(void *parameter) {

  float level;
  int zone = 0;

  ControlCommand command;

  for (;;) {

    // Xu ly lenh dieu khien tu website
    while (xQueueReceive(controlQueue, &command, 0) == pdTRUE) {

      stopAction();

      manualMode = command.manualMode;

      if (manualMode) {

        // Khong cho bom va van xa bat cung luc
        if (command.pump && command.valve) {

          command.pump = false;
          command.valve = false;
        }

        analogWrite(
          PUMP_PIN,
          command.pump ? 255 : 0
        );

        digitalWrite(
          VALVE_PIN,
          command.valve ? HIGH : LOW
        );

        pumpState = command.pump;
        valveState = command.valve;

        Serial.println("CHE DO THU CONG");

      } else {

        analogWrite(PUMP_PIN, 0);
        digitalWrite(VALVE_PIN, LOW);

        pumpState = false;
        valveState = false;

        previousZone = -1;

        Serial.println("CHE DO TU DONG");
      }

      sendDeviceStatus();
    }

    // Nhan muc nuoc tu TaskReadData
    if (xQueueReceive(
          dataAlert,
          &level,
          pdMS_TO_TICKS(20)) == pdTRUE) {

      // Chi dieu khien tu dong khi o AUTO
      if (!manualMode) {

        if (level >= 1.0 && level < 1.5) {
          zone = 1;
        }

        else if (level >= 1.5 && level <= 2.0) {
          zone = 2;
        }

        else if (level < 0.5) {
          zone = 3;
        }

        else {
          zone = 0;
        }

        if (zone != previousZone) {

          xSemaphoreTake(serialMutex, portMAX_DELAY);

          Serial.println("\n===== TASK CANH BAO =====");

          Serial.print("Muc nuoc: ");
          Serial.print(level);
          Serial.println(" m");

          if (currentAction != IDLE) {
            stopAction();
          }

          previousZone = zone;

          if (zone == 1) {

            Serial.println("VUNG 1: BOM 50%");
            startAction(PUMP_50, 10000);
          }

          else if (zone == 2) {

            Serial.println("VUNG 2: BOM 100%");
            startAction(PUMP_100, 20000);
          }

          else if (zone == 3) {

            Serial.println("VUNG 3: MO VAN XA");
            startAction(DRAIN, 10000);
          }

          else {

            Serial.println("MUC NUOC BINH THUONG");
            stopAction();
          }

          Serial.println("========================");

          xSemaphoreGive(serialMutex);
        }
      }
    }

    // Kiem tra het thoi gian dieu khien
    if (!manualMode &&
        currentAction != IDLE &&
        actionDuration > 0 &&
        millis() - actionStart >= actionDuration) {

      xSemaphoreTake(serialMutex, portMAX_DELAY);

      Serial.println("HET THOI GIAN DIEU KHIEN");

      stopAction();

      xSemaphoreGive(serialMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ==========================================
// 17. SETUP
// ==========================================

void setup() {

  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(PUMP_PIN, OUTPUT);
  pinMode(VALVE_PIN, OUTPUT);

  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(VALVE_PIN, LOW);

  sensors.begin();

  dataQueue = xQueueCreate(10, sizeof(SensorData));
  dataAlert = xQueueCreate(1, sizeof(float));

  controlQueue = xQueueCreate(5, sizeof(ControlCommand));
  statusQueue = xQueueCreate(1, sizeof(DeviceStatus));

  serialMutex = xSemaphoreCreateMutex();

  if (dataQueue == NULL ||
      dataAlert == NULL ||
      controlQueue == NULL ||
      statusQueue == NULL ||
      serialMutex == NULL) {

    Serial.println("LOI KHOI TAO FREERTOS!");

    while (true) {
      delay(1000);
    }
  }

  // Task truyen data - Core 0
  xTaskCreatePinnedToCore(
    TaskSendData,
    "TaskSendData",
    8192,
    NULL,
    1,
    NULL,
    0
  );

  // Task doc data - Core 1
  xTaskCreatePinnedToCore(
    TaskReadData,
    "TaskReadData",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // Task canh bao - Core 1
  xTaskCreatePinnedToCore(
    TaskAlert,
    "TaskAlert",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  Serial.println("\nHE THONG FREERTOS DA KHOI DONG");
}

// ==========================================
// 18. LOOP
// ==========================================

void loop() {

  vTaskDelay(pdMS_TO_TICKS(1000));
}
