#include <ESP8266WiFi.h>
#include <PolledTimeout.h>
#include <LinkedList.h>
#include <ArduinoJson.h>
#include <algorithm> // std::min

#define STASSID "WateringControl"
#define STAPSK  "senha1234"
#define MAX_BUFFER 512
#define TCP_PORT 1324
#define BREATH_MS 200

WiFiServer g_server(TCP_PORT);
WiFiClient g_client;
char g_buffer[MAX_BUFFER];
esp8266::polledTimeout::oneShotFastMs g_enoughMs(BREATH_MS);

void setup() {
  delay(1000);
  Serial.begin(115200);
  while (!Serial); // Wait until Serial is ready
  Serial.print(ESP.getFullVersion());

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  WiFi.setAutoReconnect(true);
  Serial.print("Connecting to ");
  Serial.print(STASSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println("");

  Serial.print("Connected, address=");
  Serial.println(WiFi.localIP());
  Serial.flush();

  g_server.begin();
}

void processarComandoRecebido(String);

void loop() {
  g_enoughMs.reset(BREATH_MS);

  if (g_server.hasClient()) {
    g_client = g_server.available();
    Serial.println("New client arived");
  }

  while (Serial.available() && !g_enoughMs) {
    String l_data = Serial.readStringUntil('\n');
    if (!g_client.connected()) {
      continue;
    }
    if (l_data.startsWith("ToClient:")) {
      // remove a string "ToClient:" e manda pro cliente o dado
      g_client.println(l_data.substring(9));
      g_client.flush();
    }
    if (l_data.startsWith("ToESP:")) {
      processarComandoRecebido(l_data.substring(6));
    }
  }

  if (!g_client.connected()) {
    if (g_client) {
      Serial.println("Client gone");
      Serial.flush();
      g_client = WiFiClient();
    }
    return;
  }

  if (g_client.available() && !g_enoughMs) {
    int readed = g_client.read(g_buffer, MAX_BUFFER);
    Serial.print("ToArduino:");
    Serial.write(g_buffer, readed);
    Serial.println("\n");
    Serial.flush();
    // echo to client
    // g_client.write(g_buffer, readed);
    // g_client.flush();
  }
}

void processarComandoRecebido(String a_comando) {
  DynamicJsonDocument l_doc(1024);
  deserializeJson(l_doc, a_comando);

  // TODO: ver quais comandos o arduino vai precisar mandar pra mim...
}