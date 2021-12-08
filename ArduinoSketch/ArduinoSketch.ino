#define ARDUINOJSON_USE_LONG_LONG 1
#include "ArduinoJson.h"
#include "Ultrasonic.h"
#include <stdlib.h>

#define PS_TEMP A0
#define PS_ILUM A1
#define PS_UMIDADE A2

#define PS_ULTSOM_TRIG 4
#define PS_ULTSOM_ECHO 5

#define P_BOMBA 3

#define P_LCD1 A4
#define P_LCD2 A5

// dinamicas
// Inicializa o sensor nos pinos definidos acima
Ultrasonic g_ultrasonic(PS_ULTSOM_TRIG, PS_ULTSOM_ECHO);

void setup() {
  delay(2000);
  Serial.begin(115200);
  while (!Serial); // Wait until Serial is ready
  Serial.println("");

  g_ultrasonic.setInterval(1000000);

  pinMode(PS_TEMP, INPUT);
  pinMode(PS_ILUM, INPUT);
  pinMode(PS_UMIDADE, INPUT);
  pinMode(P_BOMBA, OUTPUT);
}

void ligarBomba(const DynamicJsonDocument& /*a_comando*/) {
  digitalWrite(P_BOMBA, HIGH);
  Serial.println("ligarBomba");
}

void desligarBomba(const DynamicJsonDocument& /*a_comando*/) {
  digitalWrite(P_BOMBA, LOW);
  Serial.println("desligarBomba");
}

void trocarMensagemLcd(const DynamicJsonDocument& a_comando) {
  String l_linha1 = a_comando["linha1"];
  String l_linha2 = a_comando["linha2"];

  Serial.println("trocarMensagemLcd: ");
  Serial.println(l_linha1);
  Serial.println(l_linha2);
}

struct FunctionByNome {
  String nome;
  void (*fun_ptr)(const DynamicJsonDocument&);

  FunctionByNome();
  FunctionByNome(String a_nome, void (*a_fun_ptr)(const DynamicJsonDocument&))
    : nome(a_nome), fun_ptr(a_fun_ptr) {}
};

FunctionByNome g_functions[] = {
  FunctionByNome("ligarBomba", ligarBomba),
  FunctionByNome("desligarBomba", desligarBomba),
  FunctionByNome("trocarMensagemLcd", trocarMensagemLcd)
};

void processarComandoRecebido(String a_comando) {
  DynamicJsonDocument l_doc(512);
  bool l_found = false;

  deserializeJson(l_doc, a_comando);
  for (int f = 0; f < sizeof(g_functions) / sizeof(FunctionByNome); ++f) {
    FunctionByNome& l_f = g_functions[f];
    if (l_doc["nome"] == l_f.nome) {
      l_found = true;
      l_f.fun_ptr(l_doc);
      break;
    }
  }

  if (!l_found) {
    Serial.print("{\"name\":\"error\", \"error\":\"Método não encontrado\"}");
    Serial.println("");
  }
}

void receberComando() {
  while (Serial.available()) {
    String l_data = Serial.readStringUntil('\n');
    if (l_data.startsWith("{") && l_data.endsWith("}")) {
      processarComandoRecebido(l_data);
    }
  }
}

int sensorToPercent(int a_valor, int min = 0, int max = 1023) {
  return map(a_valor, min, max, 0, 100);
}

int tempSensorToCelsios(int a_valor) {
  float R1 = 10000;
  float logR2, R2, T, Tc, Tf;
  float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

  R2 = R1 * ((1023.0 / float(a_valor)) - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2 * logR2 + c3 * logR2 * logR2 * logR2));
  Tc = T - 215.63;

  return Tc;
}

unsigned long l_lastEnvioSensores = 0;
void atualizarSensores() {
  g_ultrasonic.update();
  unsigned long l_millis = millis();
  if ((l_lastEnvioSensores != 0) && (l_millis - l_lastEnvioSensores < 1000)) {
    return;
  }
  l_lastEnvioSensores = l_millis;

  DynamicJsonDocument l_doc(512);
  // preencher os valores dos sensores
  l_doc["nome"] = "setSensores";
  l_doc["temperatura"] = tempSensorToCelsios(analogRead(PS_TEMP));
  l_doc["umidade"] = sensorToPercent(1023 - analogRead(PS_UMIDADE));
  if (g_ultrasonic.measureIsAvaible())
    l_doc["volume"] = g_ultrasonic.measureMM();

  l_doc["iluminacao"] = analogRead(PS_ILUM);
  serializeJson(l_doc, Serial);
  Serial.println("");
}

void loop() {
  atualizarSensores();
  receberComando();
}
