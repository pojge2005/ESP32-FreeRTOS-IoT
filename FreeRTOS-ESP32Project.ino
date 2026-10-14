#include <WiFi.h>
#include <PubSubClient.h>

#define LED_PIN 2
#define BUTTON_PIN 4

constexpr UBaseType_t QUEUE_DEPTH = 5;
constexpr TickType_t SENSOR_PERIOD = pdMS_TO_TICKS(3000);
constexpr uint32_t WIFI_RETRY_MS = 5000;
constexpr uint32_t MQTT_RETRY_MIN_MS = 1000;
constexpr uint32_t MQTT_RETRY_MAX_MS = 30000;

const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASS";
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

QueueHandle_t ledQueue = nullptr;
QueueHandle_t mqttQueue = nullptr;
SemaphoreHandle_t buttonSemaphore = nullptr;

volatile uint32_t ledQueueDrops = 0;
volatile uint32_t mqttQueueDrops = 0;
volatile uint32_t publishFailures = 0;

void IRAM_ATTR buttonISR() {
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  if (buttonSemaphore != nullptr) {
    xSemaphoreGiveFromISR(buttonSemaphore, &higherPriorityTaskWoken);
  }
  if (higherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // MQTT callback runs inside client.loop(), owned by MQTTTask.
  if (strcmp(topic, "esp32/led") != 0 || length > 3) {
    return;
  }

  char message[4] = {0};
  memcpy(message, payload, length);

  if (strcmp(message, "ON") == 0) {
    digitalWrite(LED_PIN, HIGH);
  } else if (strcmp(message, "OFF") == 0) {
    digitalWrite(LED_PIN, LOW);
  }
}

void SensorTask(void* pvParameters) {
  (void)pvParameters;

  for (;;) {
    const int value = random(20, 40);

    // Keep sampling responsive: never wait for a consumer.
    if (xQueueSend(ledQueue, &value, 0) != pdPASS) {
      ++ledQueueDrops;
    }

    // Drop the newest MQTT reading if the bounded queue is full.
    // Sampling continues even while Wi-Fi/MQTT is unavailable.
    if (xQueueSend(mqttQueue, &value, 0) != pdPASS) {
      ++mqttQueueDrops;
    }

    vTaskDelay(SENSOR_PERIOD);
  }
}

void LEDTask(void* pvParameters) {
  (void)pvParameters;
  int value = 0;

  for (;;) {
    // Timeout lets the task service the button even without sensor data.
    if (xQueueReceive(ledQueue, &value, pdMS_TO_TICKS(50)) == pdPASS) {
      if (value > 30) {
        digitalWrite(LED_PIN, HIGH);
      }
    }

    if (buttonSemaphore != nullptr &&
        xSemaphoreTake(buttonSemaphore, 0) == pdTRUE) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  }
}

void MQTTTask(void* pvParameters) {
  (void)pvParameters;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.setSocketTimeout(3);

  uint32_t nextWifiLog = 0;
  uint32_t nextMqttAttempt = 0;
  uint32_t retryDelay = MQTT_RETRY_MIN_MS;

  for (;;) {
    const uint32_t now = millis();

    if (WiFi.status() != WL_CONNECTED) {
      if (static_cast<int32_t>(now - nextWifiLog) >= 0) {
        Serial.println("Wi-Fi not connected; waiting for auto-reconnect.");
        nextWifiLog = now + WIFI_RETRY_MS;
      }

      // Only this task touches the MQTT client. Wi-Fi recovery never
      // blocks SensorTask or LEDTask.
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    if (!client.connected()) {
      if (static_cast<int32_t>(now - nextMqttAttempt) >= 0) {
        Serial.println("Attempting MQTT connection...");

        if (client.connect("ESP32Client")) {
          client.subscribe("esp32/led");
          Serial.println("MQTT connected.");
          retryDelay = MQTT_RETRY_MIN_MS;
        } else {
          Serial.printf("MQTT connect failed, state=%d\n", client.state());
          nextMqttAttempt = now + retryDelay;
          retryDelay = (retryDelay >= MQTT_RETRY_MAX_MS / 2)
                           ? MQTT_RETRY_MAX_MS
                           : retryDelay * 2;
        }
      }

      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    // All PubSubClient calls are confined to this task.
    client.loop();

    int value = 0;
    while (xQueueReceive(mqttQueue, &value, 0) == pdPASS) {
      char message[12];
      snprintf(message, sizeof(message), "%d", value);

      if (!client.publish("esp32/sensor", message)) {
        ++publishFailures;
        // This reading has been removed from the queue; record the failure
        // rather than retrying forever and blocking newer readings.
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  ledQueue = xQueueCreate(QUEUE_DEPTH, sizeof(int));
  mqttQueue = xQueueCreate(QUEUE_DEPTH, sizeof(int));
  buttonSemaphore = xSemaphoreCreateBinary();

  if (ledQueue == nullptr || mqttQueue == nullptr ||
      buttonSemaphore == nullptr) {
    Serial.println("FATAL: failed to allocate FreeRTOS queue/semaphore.");
    while (true) {
      delay(1000);
    }
  }

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  const BaseType_t sensorCreated =
      xTaskCreate(SensorTask, "Sensor", 2048, nullptr, 1, nullptr);
  const BaseType_t ledCreated =
      xTaskCreate(LEDTask, "LED", 2048, nullptr, 2, nullptr);
  const BaseType_t mqttCreated =
      xTaskCreate(MQTTTask, "MQTT", 4096, nullptr, 1, nullptr);

  if (sensorCreated != pdPASS || ledCreated != pdPASS ||
      mqttCreated != pdPASS) {
    Serial.println("FATAL: failed to create one or more FreeRTOS tasks.");
    while (true) {
      delay(1000);
    }
  }
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
