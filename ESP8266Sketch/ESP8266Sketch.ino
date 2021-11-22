#include <ESP8266WiFi.h>
#include <ESP_EEPROM.h>
#include <PolledTimeout.h>
#include <LinkedList.h>
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#include <algorithm> // std::min

#define STASSID "iPhone de Israel"
#define STAPSK  "di120604"
#define TCP_PORT 1324
#define BREATH_MS 200

time_t g_clock = 0;
time_t g_clockSaved = 0;
long long g_lastTimeClockUpdate = -1;
long long g_lastTimeClockWebUpdate = -1;
long long g_lastTryTimeClockWebUpdate = -1; 

WiFiServer g_server(TCP_PORT);
WiFiClient g_client;
WiFiClient g_clientClock;

template<typename T>
char calculateCheckSum(T v, char l_lastChecksum = 0) {
  char l_checksum = l_lastChecksum;

  while (v != 0) {
    l_checksum += (v & 0xFF);
    v = v >> 4;
  }

  return l_checksum;
}

void lerDataSistema() {
  char l_checksum = 0;
  EEPROMClass l_eeprom;
  l_eeprom.begin(sizeof(g_clock) + sizeof(l_checksum));
  l_eeprom.get(0, g_clock);
  l_eeprom.get(sizeof(g_clock), l_checksum);
  l_eeprom.end();

  if (calculateCheckSum(g_clock) != l_checksum) {
    Serial.println("checksum invalido");
    g_clock = 0;
    g_clockSaved = 0;
  } else {
    g_clockSaved = g_clock; 
  }

  Serial.print("Data salva: ");
  Serial.println(g_clock, DEC);
}

time_t currentUnixEpoch() {
  time_t l_newClock = g_clock;

  if (g_lastTimeClockUpdate < 0)
    g_lastTimeClockUpdate = g_lastTimeClockWebUpdate;

  if (g_lastTimeClockUpdate >= 0) {
    long long l_millis = millis();
    long long l_elapsedFromLastUpdate = 0;
    l_elapsedFromLastUpdate = std::ceil((l_millis - g_lastTimeClockUpdate) / 1000);

    if (l_elapsedFromLastUpdate > 0) // verifico se millis não reiniciou
      l_newClock += l_elapsedFromLastUpdate;

    if (l_elapsedFromLastUpdate > 60) {// caso o millis tenha passado um minuto
      g_lastTimeClockUpdate = l_millis;
      g_clock = l_newClock;
    }
  }

  return l_newClock;
}

void gravarDataSistema() {
  time_t l_newClock = currentUnixEpoch();
  long long l_delta = std::abs(g_clockSaved - l_newClock);
  
  if (l_delta < 5 * 60) 
    return;

  Serial.print("Salvando data: ");
  Serial.println(l_newClock, DEC);

  char l_checksum = calculateCheckSum(l_newClock);
  EEPROMClass l_eeprom;
  l_eeprom.begin(sizeof(time_t) + sizeof(l_checksum));
  l_eeprom.put(0, l_newClock);
  l_eeprom.put(sizeof(l_newClock), l_checksum);
  l_eeprom.end();  
  
  g_clockSaved = l_newClock;  
}

void inicializarWifi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  WiFi.setAutoReconnect(true);
  Serial.print("Connecting to ");
  Serial.print(STASSID);

  int l_ledState = LOW;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    l_ledState = ((l_ledState = LOW) ? HIGH : LOW);
    digitalWrite(LED_BUILTIN, l_ledState);
    delay(500);
  }
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("");

  Serial.print("Connected, address=");
  Serial.println(WiFi.localIP());
  Serial.flush();

  g_server.begin();
}

void setup() {
  delay(2000);
  Serial.begin(115200);
  while (!Serial); // Wait until Serial is ready
  Serial.println("");
  Serial.println(ESP.getFullVersion());

  lerDataSistema();
  inicializarWifi();
}

LinkedList<String>* splitString(String a_string, char a_sep) {
  LinkedList<String>* l_ret = new LinkedList<String>();
  unsigned int l_lastPartEnd = 0;

  for (unsigned int i = 0; i < a_string.length(); ++i) {
    if (a_string.charAt(i) == a_sep) {
      String l_part;

      if (i > l_lastPartEnd) {
        l_part = a_string.substring(l_lastPartEnd, i);
      }

      l_ret->add(l_part);
      l_lastPartEnd = i + 1;
    }
  }

  if (l_lastPartEnd < a_string.length()) {
      String l_part = a_string.substring(l_lastPartEnd);
      l_ret->add(l_part);
  }

  return l_ret;
}

int monthNumber(String a_string) {
  if ((a_string == "Jan") || (a_string == "January"))   return 0;
  if ((a_string == "Feb") || (a_string == "February"))  return 1;
  if ((a_string == "Mar") || (a_string == "March"))     return 2;
  if ((a_string == "Apr") || (a_string == "April"))     return 3;
  if ((a_string == "May"))                              return 4;
  if ((a_string == "Jun") || (a_string == "June"))      return 5;
  if ((a_string == "Jul") || (a_string == "July"))      return 6;
  if ((a_string == "Aug") || (a_string == "August"))    return 7;
  if ((a_string == "Sep") || (a_string == "September")) return 8;
  if ((a_string == "Oct") || (a_string == "October"))   return 9;
  if ((a_string == "Nov") || (a_string == "November"))  return 10;
  if ((a_string == "Dec") || (a_string == "December"))  return 11;

  Serial.print("invalid month name");
  Serial.println(a_string);
  return 0;
}

time_t parseHttpHeaderDate(String a_date) {
  // Date: Sat, 20 Nov 2021 21:22:09 GMT
  time_t l_newTime = 0;
  struct tm* l_ti = localtime ( &l_newTime );
  LinkedList<String>* l_parts = splitString(a_date, ' ');
  String l_time;
  
  l_ti->tm_mday = l_parts->get(2).toInt();
  l_ti->tm_mon = monthNumber(l_parts->get(3));
  l_ti->tm_year = l_parts->get(4).toInt() - 1900;
  l_time = l_parts->get(5);

  delete l_parts;

  l_parts = splitString(l_time, ':');
  l_ti->tm_hour = l_parts->get(0).toInt();
  l_ti->tm_min = l_parts->get(1).toInt();
  l_ti->tm_sec = l_parts->get(2).toInt();

  delete l_parts;

  l_newTime = mktime(l_ti);

  return l_newTime;
}

void verificarBuscarNaWebADataHora() {
  long long l_millis = millis();
  
  while (g_clientClock.available()) {
    // read an incoming byte from the server and print them to serial monitor:
    String c = g_clientClock.readStringUntil('\n');
    if (c.startsWith("Date: ")) {
      time_t l_newTime = parseHttpHeaderDate(c);

      g_lastTimeClockWebUpdate = l_millis;
      g_clock = l_newTime;

      Serial.println(c);
      Serial.print("Nova data da web: ");
      Serial.println(g_clock);
      
      g_clientClock.stop();
    }
  }

  if ((g_lastTimeClockWebUpdate != -1) && (std::abs(g_lastTimeClockWebUpdate - l_millis) < 24 * 60 * 60 * 1000)) {
    return;
  }

  if ((g_lastTryTimeClockWebUpdate != -1) && (std::abs(g_lastTryTimeClockWebUpdate - l_millis) < 30 * 1000)) {
    return;
  }

  if (g_clientClock.status() == 0) {
    String l_host = "google.com.br";

    g_lastTryTimeClockWebUpdate = l_millis;
    if (!g_clientClock.connect(l_host, 80)) {
      Serial.println("connection to " + l_host + " failed");
      return;
    }

    Serial.println("connected to " + l_host);
    g_clientClock.println(l_host + " / HTTP/1.1");
    g_clientClock.println("Host: " + l_host);
    g_clientClock.println("Connection: close");
    g_clientClock.println(); // end HTTP request header
  }
}

void verificarSeTemNovoClient() {
  if (g_server.hasClient()) {
    g_client = g_server.available();
    Serial.println("New client arived");
  }
}

void verificarSeTemDadosNaSerial() {
  while (Serial.available()) {
    String l_data = Serial.readStringUntil('\n');
    if (l_data.startsWith("ToESP:")) {
      processarComandoRecebido(l_data.substring(6));
    }
    if (l_data.startsWith("ToClient:")) {
      if (!g_client.connected()) {
        continue;
      }
      // remove a string "ToClient:" e manda pro cliente o dado
      g_client.println(l_data.substring(9));
      g_client.flush();
    }
  }
}

void verificarSeTemDadosNoCliente() {
  if (!g_client.connected()) {
    if (g_client) {
      Serial.println("Client gone");
      Serial.flush();
      g_client = WiFiClient();
    }
    g_client.stop();
    return;
  }

  while (g_client.available()) {
    String l_data = g_client.readStringUntil('\n');
    if (l_data.length() == 0) break;
    if (l_data.startsWith("ToESP:")) {
      processarComandoRecebido(l_data.substring(6));
    } else {
      if (!l_data.startsWith("ToArduino:")) {
        Serial.print("ToArduino:");
      }
      Serial.println(l_data);
      Serial.flush();
    }
    // echo to client
    // g_client.write(g_buffer, readed);
    // g_client.flush();
  }
}

void processarComandoRecebido(String a_comando) {
  DynamicJsonDocument l_doc(1024);
  deserializeJson(l_doc, a_comando);

  if (l_doc["name"] == String("currentUnixEpoch")) {
    DynamicJsonDocument l_response(1024);
    l_response["name"] = l_doc["name"];
    l_response["unixEpoch"] = currentUnixEpoch();
    Serial.print("ToArduino:");
    serializeJson(l_response, Serial);
    Serial.println("");
  }

  // TODO: ver quais comandos o arduino vai precisar mandar pra mim...
}

void loop() {
  verificarBuscarNaWebADataHora();
  gravarDataSistema();

  verificarSeTemNovoClient();
  verificarSeTemDadosNaSerial();
  verificarSeTemDadosNoCliente();

  delay(10);
}
