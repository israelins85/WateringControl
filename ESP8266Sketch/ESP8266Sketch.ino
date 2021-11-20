#include <ESP8266WiFi.h>
#include <ESP_EEPROM.h>
#include <PolledTimeout.h>
#include <LinkedList.h>
#include <ArduinoJson.h>
#include <algorithm> // std::min

#define STASSID "iPhone de Israel"
#define STAPSK  "di120604"
#define MAX_BUFFER 512
#define TCP_PORT 1324
#define BREATH_MS 200

time_t g_clock;
long long g_lastTimeClockMillis = -1;
long long g_lastTimeClockUpdate = -1;
long long g_lastTryTimeClockUpdate = -1; 

WiFiServer g_server(TCP_PORT);
WiFiClient g_client;
WiFiClient g_clientClock;
char g_buffer[MAX_BUFFER];
esp8266::polledTimeout::oneShotFastMs g_enoughMs(BREATH_MS);

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
    g_clock = 1637442342;
    gravarDataSistema();
  } else {
    g_lastTimeClockMillis = millis();
  }
}

void gravarDataSistema() {
  long long l_millis = millis();

  if ((g_lastTimeClockMillis >= 0) && (g_lastTimeClockMillis < l_millis)) {
    int l_delta = (l_millis - g_lastTimeClockMillis);
    if (l_delta < 5 * 60 * 1000) 
      return;
    g_clock = g_clock + l_delta;
  }

  g_lastTimeClockMillis = l_millis;

  Serial.print("NEW UNIX EPOCH: ");
  Serial.println(g_clock, DEC);

  char l_checksum = calculateCheckSum(g_clock);
  EEPROMClass l_eeprom;
  l_eeprom.begin(sizeof(time_t) + sizeof(l_checksum));
  l_eeprom.put(0, g_clock);
  l_eeprom.put(sizeof(g_clock), l_checksum);
  l_eeprom.end();    
}

void setup() {
  delay(2000);
  Serial.begin(115200);
  while (!Serial); // Wait until Serial is ready
  Serial.println(ESP.getFullVersion());

  lerDataSistema();
  Serial.print("UNIX EPOCH: ");
  Serial.println(g_clock, DEC);

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

LinkedList<String> splitString(String a_string, char a_sep) {
  LinkedList<String> l_ret;
  int l_lastPartEnd = 0;

  Serial.println("split " + a_string);
  Serial.print("sep ");
  Serial.println(a_sep);

  for (int i = 0; i < a_string.length(); ++i) {
    if (a_string.charAt(i) == a_sep) {
      String l_part;

      if (i > l_lastPartEnd) {
        l_part = a_string.substring(l_lastPartEnd, i);
      }

      Serial.print("sep at ");
      Serial.println(i, DEC);

      Serial.print("part ");
      Serial.println(l_part);

      l_ret.add(l_part);
      l_lastPartEnd = i + 1;
    }
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

void parseLineDate(String a_date) {
  // Date: Sat, 20 Nov 2021 21:22:09 GMT
  LinkedList<String> l_parts = splitString(a_date, ' ');
  time_t l_newTime = 0;
  struct tm* l_ti = localtime ( &l_newTime );
  
  l_ti->tm_mday = l_parts.get(2).toInt() - 1900;
  l_ti->tm_mon = monthNumber(l_parts.get(3));
  l_ti->tm_year = l_parts.get(4).toInt();

  LinkedList<String> l_parts2 = splitString(l_parts[5], ':');
  l_ti->tm_hour = l_parts2.get(0).toInt();
  l_ti->tm_min = l_parts2.get(1).toInt();
  l_ti->tm_sec = l_parts2.get(2).toInt();

  Serial.print("l_ti->tm_mday");
  Serial.println(l_ti->tm_mday, DEC);
  Serial.print("l_ti->tm_mon");
  Serial.println(l_ti->tm_mon, DEC);
  Serial.print("l_ti->tm_year");
  Serial.println(l_ti->tm_year, DEC);

  Serial.print("l_ti->tm_hour");
  Serial.println(l_ti->tm_hour, DEC);
  Serial.print("l_ti->tm_min");
  Serial.println(l_ti->tm_min, DEC);
  Serial.print("l_ti->tm_sec");
  Serial.println(l_ti->tm_sec, DEC);

  l_newTime = mktime(l_ti);
  Serial.print("l_newTime");
  Serial.println(l_newTime, DEC);

  // if (std::abs(l_newTime - g_clock) > 5 * 60 * 1000) {
  //   g_clock = l_newTime;
  //   g_lastTimeClockMillis = -1;// força a gravação da data no EEPROM
  // }
}

void verificarBuscarNaWebADataHora() {
  long long l_now = millis();

  while (g_clientClock.available()) {
    // read an incoming byte from the server and print them to serial monitor:
    String c = g_clientClock.readStringUntil('\n');
    if (c.startsWith("Date: ")) {
      parseLineDate(c);
      g_clientClock.stop();

      Serial.print("g_clientClock.status()");
      Serial.println(g_clientClock.status(), DEC);
      g_lastTryTimeClockUpdate = l_now;
    }
    Serial.println(c);
  }

  if ((g_lastTimeClockUpdate != -1) && (std::abs(g_lastTimeClockUpdate - l_now) < 60 * 60 * 1000)) {
    return;
  }

  if ((g_lastTryTimeClockUpdate != -1) && (std::abs(g_lastTryTimeClockUpdate - l_now) < 30 * 1000)) {
    return;
  }

  if (g_clientClock.status() == 0) {
    String l_host = "google.com.br";

    Serial.print("verificarBuscarNaWebADataHora:");
    if (!g_clientClock.connect(l_host, 80)) {
      Serial.println("connection to " + l_host + " failed");
      return;
    }

    Serial.println("connected to " + l_host);
    g_clientClock.println(l_host + " / HTTP/1.1");
    g_clientClock.println("Host: " + l_host);
    g_clientClock.println(); // end HTTP request header
    g_lastTimeClockUpdate = l_now;
  }
}

void verificarSeTemNovoClient() {
  if (g_server.hasClient()) {
    g_client = g_server.available();
    Serial.println("New client arived");
  }
}

void verificarSeTemDadosNaSerial() {
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

void loop() {
  g_enoughMs.reset(BREATH_MS);
  
  verificarBuscarNaWebADataHora();
  gravarDataSistema();

  verificarSeTemNovoClient();
  verificarSeTemDadosNaSerial();
  verificarSeTemDadosNoCliente();
}
