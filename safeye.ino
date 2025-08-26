#include <WiFi.h>
#include <PubSubClient.h>

// Wi-Fi
const char* ssid = "";
const char* password = "";

// MQTT
const char* mqtt_server = "4";  // ex: 192.168.1.100
WiFiClient espClient;
PubSubClient client(espClient);

// Pinos
#define SENSOR_PIN 34   // OUT do sensor IR
#define BUZZER_PIN 25   // Buzzer
#define LED_PIN 26      // LED externo
#define WIFI_PIN 14     // LED wifi
#define MQTT_PIN 12     // LED broker

// Estado
unsigned long tempoFechado = 0;
bool olhoFechado = false;
bool alertaAtivo = false;
unsigned long cicloInicio = 0;
unsigned long ultimoEnvioMQTT = 0;

void setup_wifi() {
  delay(10);
  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // digitalWrite(WIFI_PIN, LOW);
  }

  Serial.println("");
  Serial.println("Wi-Fi conectado");
  // digitalWrite(WIFI_PIN, HIGH);
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conectar ao broker MQTT...");
    if (client.connect("ESP32_Oculos")) {
      Serial.println("Conectado ao MQTT");
      // digitalWrite(MQTT_PIN, HIGH);
      // Se quiser, inscreva-se em algum tópico aqui
      // client.subscribe("algum/topico/cmd");
    } else {
      Serial.print("Falha, rc=");
      // digitalWrite(MQTT_PIN, LOW);
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(WIFI_PIN, OUTPUT);
  pinMode(MQTT_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(MQTT_PIN, LOW);
  digitalWrite(MQTT_PIN, LOW);


  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  int valor = digitalRead(SENSOR_PIN); // LOW = olho fechado
  unsigned long agora = millis();

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

  // 🟢 Enviar dados a cada 2s via MQTT
  if (agora - ultimoEnvioMQTT >= 2000) {
    ultimoEnvioMQTT = agora;

    // Constrói JSON manualmente
    String payload = "{";
    payload += "\"olhoFechado\":";
    payload += (valor == LOW ? "true" : "false");
    payload += ",\"tempoFechado\":";
    payload += (valor == LOW ? String(agora - tempoFechado) : "0");
    payload += ",\"alertaAtivo\":";
    payload += (alertaAtivo ? "true" : "false");
    payload += "}";

    client.publish("devices/oculos1/telemetry", payload.c_str());
    Serial.println("📡 Publicado no MQTT: " + payload);
  }

  delay(50); // estabilidade
}