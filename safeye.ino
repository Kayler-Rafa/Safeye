#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ------------------ CONFIG Wi-Fi ------------------
const char* ssid = "NOVA ROMA_ALUNOS";
const char* password = "Alunos@2025";

// Node-RED (VPS)
const char* node_red_url = "http://173.249.32.99:1880/telemetria";

// ------------------ PINOS ------------------
#define SENSOR_PIN 34   // OUT do sensor IR
#define BUZZER_PIN 25   // Buzzer
#define LED_PIN 26      // LED externo
#define WIFI_PIN 14     // LED wifi
#define MQTT_PIN 12     // LED broker

// ------------------ VARIÁVEIS GLOBAIS ------------------
unsigned long tempoFechado = 0;
bool olhoFechado = false;
bool alertaAtivo = false;
unsigned long cicloInicio = 0;
unsigned long ultimoEnvioSupabase = 0;

// Mutex para evitar conflitos entre tasks
SemaphoreHandle_t mutexEstado;

// ------------------ HANDLES DE TASK ------------------
TaskHandle_t TaskSensorHandle;
TaskHandle_t TaskEnvioHandle;
TaskHandle_t TaskWiFiHandle;

// ===================================================
// Função: Conexão Wi-Fi
// ===================================================
void setup_wifi() {
  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(ssid, password);
  int tentativas = 0;

  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    digitalWrite(WIFI_PIN, LOW);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Wi-Fi conectado");
    digitalWrite(WIFI_PIN, HIGH);
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Falha na conexão Wi-Fi");
  }
}

// ===================================================
// Função: Envio para Node-RED
// ===================================================
void enviarParaNodeRed(bool olhoFechado, int tempoFechado, bool alertaAtivo) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(node_red_url);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(512);
    doc["olhofechado"] = olhoFechado;
    doc["tempofechado"] = tempoFechado;
    doc["alertaativo"] = alertaAtivo;

    String jsonString;
    serializeJson(doc, jsonString);

    int httpResponseCode = http.POST(jsonString);                                                    
    if (httpResponseCode > 0) {
      Serial.println("✅ Enviado para Node-RED: " + jsonString);
      Serial.println("Response code: " + String(httpResponseCode));
    } else {
      Serial.println("❌ Erro ao enviar para Node-RED: " + String(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("❌ WiFi desconectado!");
  }
}

// ===================================================
// Task: Leitura do Sensor e Controle do Alerta
// ===================================================
void TaskSensor(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    int valor = digitalRead(SENSOR_PIN); // LOW = olho fechado
    unsigned long agora = millis();

    xSemaphoreTake(mutexEstado, portMAX_DELAY);

    if (!alertaAtivo) {
      // 🚨 MODO SENTINELA
      if (valor == LOW) {
        if (!olhoFechado) {
          olhoFechado = true;
          tempoFechado = agora;
        } else if (agora - tempoFechado >= 2000) {
          alertaAtivo = true;
          cicloInicio = agora;
          digitalWrite(BUZZER_PIN, HIGH);
          digitalWrite(LED_PIN, HIGH);
          Serial.println("🚨 ALERTA INICIADO");
        }
      } else {
        olhoFechado = false;
      }

    } else {
      // 🚨 MODO ALERTA
      if (agora - cicloInicio >= 5000) {
        if (valor == HIGH) {
          alertaAtivo = false;
          olhoFechado = false;
          digitalWrite(BUZZER_PIN, LOW);
          digitalWrite(LED_PIN, LOW);
          Serial.println("✅ Alerta encerrado, voltando para sentinela");
        } else {
          cicloInicio = agora;
          Serial.println("♻️ Ciclo reiniciado, olho ainda fechado");
        }
      }
    }

    xSemaphoreGive(mutexEstado);

    vTaskDelay(50 / portTICK_PERIOD_MS); // estabilidade
  }
}

// ===================================================
// Task: Envio periódico para Node-RED
// ===================================================
void TaskEnvioNodeRed(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    unsigned long agora = millis();

    if (agora - ultimoEnvioSupabase >= 5000) {
      xSemaphoreTake(mutexEstado, portMAX_DELAY);
      ultimoEnvioSupabase = agora;

      int tempoAtual = 0;
      if (olhoFechado) tempoAtual = (agora - tempoFechado);

      enviarParaNodeRed(olhoFechado, tempoAtual, alertaAtivo);

      // LED MQTT indica envio
      digitalWrite(MQTT_PIN, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      digitalWrite(MQTT_PIN, LOW);
      xSemaphoreGive(mutexEstado);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// ===================================================
// Task: Monitor Wi-Fi (mantém LED atualizado)
// ===================================================
void TaskWiFi(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      digitalWrite(WIFI_PIN, LOW);
      setup_wifi(); // tenta reconectar
    } else {
      digitalWrite(WIFI_PIN, HIGH);
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

// ===================================================
// SETUP
// ===================================================
void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(WIFI_PIN, OUTPUT);
  pinMode(MQTT_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(WIFI_PIN, LOW);
  digitalWrite(MQTT_PIN, LOW);

  mutexEstado = xSemaphoreCreateMutex();

  setup_wifi();

  // Criar tasks
  xTaskCreatePinnedToCore(TaskSensor, "TaskSensor", 4096, NULL, 2, &TaskSensorHandle, 0);
  xTaskCreatePinnedToCore(TaskEnvioNodeRed, "TaskEnvio", 4096, NULL, 1, &TaskEnvioHandle, 1);
  xTaskCreatePinnedToCore(TaskWiFi, "TaskWiFi", 4096, NULL, 1, &TaskWiFiHandle, 0);

  Serial.println("✅ SafeEye iniciado com FreeRTOS!");
}

void loop() {
}
