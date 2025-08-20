#define SENSOR_PIN 34   // OUT do sensor IR
#define BUZZER_PIN 27   // Buzzer
#define LED_PIN 26      // LED externo

unsigned long tempoFechado = 0;
bool olhoFechado = false;

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  int valor = digitalRead(SENSOR_PIN);

  if (valor == LOW) {  
    // Olho fechado detectado
    if (!olhoFechado) {
      olhoFechado = true;
      tempoFechado = millis();  // marca o tempo em que fechou
    } else {
      // verifica se passou de 2 segundos
      if (millis() - tempoFechado >= 2000) {
        Serial.println("ALERTA! Olho fechado por 2s!");
        digitalWrite(BUZZER_PIN, HIGH);  // ativa buzzer
        digitalWrite(LED_PIN, HIGH);     // acende LED

        delay(5000);  // mantém LED aceso por 5 segundos

        digitalWrite(LED_PIN, LOW);      // apaga LED
        digitalWrite(BUZZER_PIN, LOW);   // desliga buzzer
        olhoFechado = false;             // reseta estado
      }
    }
  } else {
    // olho aberto → reseta flag
    olhoFechado = false;
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(50); // pequena espera para estabilidade
}
