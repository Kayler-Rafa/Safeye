#define SENSOR_PIN 34   // OUT do sensor IR
#define BUZZER_PIN 27   // Buzzer
#define LED_PIN 26      // LED externo

unsigned long tempoFechado = 0;
bool olhoFechado = false;
bool alertaAtivo = false;
unsigned long cicloInicio = 0;

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  int valor = digitalRead(SENSOR_PIN); // LOW = olho fechado, HIGH = aberto
  unsigned long agora = millis();

  if (!alertaAtivo) {
    // 🚨 MODO SENTINELA: esperando 2 segundos de olho fechado
    if (valor == LOW) {
      if (!olhoFechado) {
        olhoFechado = true;
        tempoFechado = agora;
      } else if (agora - tempoFechado >= 2000) {
        // olho ficou fechado por 2s → ativa primeiro ciclo de alerta
        alertaAtivo = true;
        cicloInicio = agora;
        digitalWrite(BUZZER_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
        Serial.println("🚨 ALERTA INICIADO");
      }
    } else {
      // Olho abriu antes dos 2 segundos → reseta
      olhoFechado = false;
    }

  } else {
    // 🚨 MODO ALERTA: ciclos de 5 segundos
    if (agora - cicloInicio >= 5000) {
      // completou 1 ciclo de 5s
      if (valor == HIGH) {
        // 👁️ olho abriu → encerra alerta após o ciclo
        alertaAtivo = false;
        olhoFechado = false;
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        Serial.println("✅ Alerta encerrado, voltando para sentinela");
      } else {
        // 👁️ ainda fechado → reinicia ciclo de 5s
        cicloInicio = agora;
        Serial.println("♻️ Ciclo reiniciado, olho ainda fechado");
        // buzzer/LED continuam ligados sem interrupção
      }
    }
  }

  delay(50); // estabilidade
}
