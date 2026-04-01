#include <WiFi.h>
#include <PubSubClient.h>

#define LED_PIN 2
#define BUTTON_PIN 4

const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASS";
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

QueueHandle_t sensorQueue;
SemaphoreHandle_t buttonSemaphore;

// ISR
void IRAM_ATTR buttonISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(buttonSemaphore, &xHigherPriorityTaskWoken);
}

// MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  if (msg == "ON") digitalWrite(LED_PIN, HIGH);
  if (msg == "OFF") digitalWrite(LED_PIN, LOW);
}

// Reconnect MQTT
void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32Client")) {
      client.subscribe("esp32/led");
    } else delay(1000);
  }
}

// Sensor Task
void SensorTask(void *pvParameters) {
  int value;
  while (1) {
    value = random(20, 40);
    xQueueSend(sensorQueue, &value, portMAX_DELAY);

    char msg[10];
    sprintf(msg, "%d", value);
    client.publish("esp32/sensor", msg);

    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
}

// LED Task
void LEDTask(void *pvParameters) {
  int val;
  while (1) {
    if (xQueueReceive(sensorQueue, &val, portMAX_DELAY)) {
      if (val > 30) digitalWrite(LED_PIN, HIGH);
    }

    if (xSemaphoreTake(buttonSemaphore, 0)) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  }
}

// MQTT Task
void MQTTTask(void *pvParameters) {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  while (1) {
    if (!client.connected()) reconnect();
    client.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  sensorQueue = xQueueCreate(5, sizeof(int));
  buttonSemaphore = xSemaphoreCreateBinary();

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  xTaskCreate(SensorTask, "Sensor", 2048, NULL, 1, NULL);
  xTaskCreate(LEDTask, "LED", 2048, NULL, 2, NULL);
  xTaskCreate(MQTTTask, "MQTT", 4096, NULL, 1, NULL);
}

void loop() {}
