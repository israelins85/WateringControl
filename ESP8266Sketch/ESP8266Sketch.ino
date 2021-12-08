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

struct RegraItem {
  bool ativa = false;
  String operador;
  int valor = 0;

  // {"ativa": false, "operador": -1, "valor": 30}
};

struct RegraHorario {
  bool restringir = false;
  int horaInicio = 0;
  int minutoInicio = 0;
  int horaFim = 0;
  int minutoFim = 0;
  String operador;
} 

struct Regra {
  bool ativa = false;
  RegraHorario horario;
  RegraItem sensores[3]; // index é umidade = 0, temperatura = 1, iluminacao = 2
  int tempoParaRegar = 0;
  // {"ativa":true, "sensores": [{"ativo": false, "operador": -1, "valor": 30}], "tempoParaRegar": 10}
};

struct Sensores {
  int volume = 0; // medida em mm
  int volumePerc = 0;
  int umidade = 0; // 0 - 1023
  int iluminacao = 0; // 0 - 1023
  int iluminacaoPerc = 0;
  int temperatura = 0; // 0 - 1023

  bool operator == (const Sensores& a_other) const {
    // * ATENÇÃO: alguns estão comentados abaixo, pois não importa para a verificação de enviar
    
//    *if (volume != a_other.volume) return false; 
    if (volumePerc != a_other.volumePerc) return false;
    if (umidade != a_other.umidade) return false;
//    *if (iluminacao != a_other.iluminacao) return false;
    if (iluminacaoPerc != a_other.iluminacaoPerc) return false;
    if (temperatura != a_other.temperatura) return false;
    return true;
  }
  
  bool operator != (const Sensores& a_other) const {
    return !(*this == a_other);
  }
};

Sensores g_sensores;

unsigned long g_millisBombaLigada = 0;
unsigned long g_millisParaManterBombaLigada = 0;

// configurações
int g_currentConfiguracoesStructVersion = 1;
struct Configuracoes {
  time_t unixEpoch = 0;
  int vlrSensorVolumeVazio = 0; // em cm
  int vlrSensorVolumeCheio = 100; // em cm
  int vlrSensorIluminacaoMax = 0; // em cm
  long long tempoMinEntreAsRegas = 0;
  Regra regras[5];
};
Configuracoes g_configuracoes;
bool g_configuracoesDirty = false;

time_t g_lastSaveUnixEpoch = 0;
long long g_lastTimeClockUpdate = -1;
long long g_lastTimeClockWebUpdate = -1;
long long g_lastTryTimeClockWebUpdate = -1; 

WiFiServer g_server(TCP_PORT);
WiFiClient g_client;
WiFiClient g_clientClock;

time_t currentUnixEpoch() {
  time_t l_newClock = g_configuracoes.unixEpoch;

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
      g_configuracoes.unixEpoch = l_newClock;
    }
  }

  return l_newClock;
}

template<typename T>
unsigned long calculateCheckSum(T a_data, unsigned long a_lastChecksum = ~0L) {
  unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };
  unsigned long crc = a_lastChecksum;
  char* l_e = (char*)(&a_data);
  unsigned int n = 0;

  while (n < sizeof(T)) {
    char byte = l_e[n];    
    crc = crc_table[(crc ^ byte) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (byte >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
    n++;
  }

  return crc;
}

Configuracoes configuracoesIniciais() {
  Configuracoes l_ret;
  Regra& l_regra = l_ret.regras[0];
  RegraItem& l_item = l_regra.sensores[0];

  g_configuracoesDirty = true;
  l_regra.ativa = true;
  l_item.ativa = true;
  l_item.operador = "<";
  l_item.valor = 70;

  return l_ret;
}

void lerConfiguracoes() {
  Serial.print("EEPROM length: ");
  Serial.println(EEPROM.length());
  Serial.print("Configuracoes length: ");
  Serial.println(sizeof(Configuracoes));
  
  unsigned long l_version = 0;
  EEPROMClass l_eeprom;
  l_eeprom.begin(sizeof(g_currentConfiguracoesStructVersion) + sizeof(unsigned long) + sizeof(Configuracoes));
  l_eeprom.get(0, l_version);

  if (l_version == g_currentConfiguracoesStructVersion) {
    Configuracoes l_lido;    
    unsigned long l_checksum = 0;
    unsigned long l_expectedCrc = 0;
    
    l_eeprom.get(0, l_checksum);
    l_eeprom.get(sizeof(l_version) + sizeof(g_currentConfiguracoesStructVersion), l_lido);
    l_expectedCrc = calculateCheckSum(l_lido);
    
    if (l_checksum == l_expectedCrc) {
        g_configuracoes = l_lido;    
        l_eeprom.end();  
        return;      
    }
  }
  
  l_eeprom.end();  

  g_configuracoes = configuracoesIniciais();     
}

void gravarConfiguracoes() {
  if (!g_configuracoesDirty) 
    return;

  g_configuracoes.unixEpoch = currentUnixEpoch();

  Serial.print("Salvando configurações: ");
  Serial.println(g_configuracoes.unixEpoch, DEC);

  unsigned long l_checksum = calculateCheckSum(g_configuracoes);
  EEPROMClass l_eeprom;
  l_eeprom.begin(sizeof(l_checksum) + sizeof(g_currentConfiguracoesStructVersion) + sizeof(g_configuracoes));
  l_eeprom.put(0, g_currentConfiguracoesStructVersion);
  l_eeprom.put(sizeof(g_currentConfiguracoesStructVersion), l_checksum);
  l_eeprom.put(sizeof(g_currentConfiguracoesStructVersion) + sizeof(l_checksum), g_configuracoes);
  l_eeprom.end();  
  
  g_lastSaveUnixEpoch = g_configuracoes.unixEpoch;  
  g_configuracoesDirty = false;
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

  lerConfiguracoes();
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
      g_configuracoes.unixEpoch = l_newTime;

      Serial.println(c);
      Serial.print("Nova data da web: ");
      Serial.println(g_configuracoes.unixEpoch);
      
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

void currentUnixEpoch(const DynamicJsonDocument& /*a_cmd*/, DynamicJsonDocument& a_resp) {
    a_resp["unixEpoch"] = currentUnixEpoch();
    a_resp["sucess"] = true;
}

void calibrarReservatorioVazio(const DynamicJsonDocument& /*a_cmd*/, DynamicJsonDocument& a_resp) {
  // preencher a variavel e gravar a config
  g_configuracoes.vlrSensorVolumeVazio = g_sensores.volume;
  g_configuracoesDirty = true;
  a_resp["sucess"] = true;
}

void calibrarReservatorioCheio(const DynamicJsonDocument& /*a_cmd*/, DynamicJsonDocument& a_resp) {
  // preencher a variavel e gravar a config
  g_configuracoes.vlrSensorVolumeCheio = g_sensores.volume;
  g_configuracoesDirty = true;
  a_resp["sucess"] = true;
}

void sensores(const DynamicJsonDocument& /*a_cmd*/, DynamicJsonDocument& a_resp) {
  a_resp["sucess"] = true;
  a_resp["umidade"] = g_sensores.umidade;
  a_resp["iluminacao"] = g_sensores.iluminacao;
  a_resp["temperatura"] = g_sensores.temperatura;
  a_resp["volume"] = map(g_sensores.volume, g_configuracoes.vlrSensorVolumeVazio, g_configuracoes.vlrSensorVolumeCheio, 0, 100);
}

void regaManual(const DynamicJsonDocument& /*a_cmd*/, DynamicJsonDocument& a_resp) {
  // acionar a rega
  if (g_sensores.volume <= g_configuracoes.vlrSensorVolumeVazio) {
    a_resp["sucess"] = false;
    a_resp["error"] = "Volume abaixo do limite mínimo";
    return;
  }
}

void configuracoes(const DynamicJsonDocument& /*a_cmd*/, DynamicJsonDocument& a_resp) {
  // preencher a variavel e gravar a config
}

// JsonArray =>
// [
//   {"ativa":true, [{"sensor": 1, "operador": -1, "valor": 30}], "tempoParaRegar": 10},
//   {"ativa":true, [{"sensor": 1, "operador": -1, "valor": 70}, {"sensor": 2, "operador": -1, "valor": 50}], "tempoParaRegar": 5}
// ]
void setConfiguracoes(const DynamicJsonDocument& a_cmd, DynamicJsonDocument& a_resp) {
  // preencher a variavel e gravar a config

  g_configuracoesDirty = true;
  a_resp["sucess"] = true;
}

struct FunctionByNomeWithResponse {
  String nome;
  void (*fun_ptr)(const DynamicJsonDocument&, DynamicJsonDocument&);

  FunctionByNomeWithResponse();
  FunctionByNomeWithResponse(String a_nome, void (*a_fun_ptr)(const DynamicJsonDocument&, DynamicJsonDocument&)) :
    nome(a_nome), fun_ptr(a_fun_ptr) {}
};

FunctionByNomeWithResponse g_functionsFromClient[] = {
  FunctionByNomeWithResponse("currentUnixEpoch", currentUnixEpoch),
  FunctionByNomeWithResponse("calibrarReservatorioVazio", calibrarReservatorioVazio),
  FunctionByNomeWithResponse("calibrarReservatorioCheio", calibrarReservatorioCheio),
  FunctionByNomeWithResponse("regaManual", regaManual),
  FunctionByNomeWithResponse("configuracoes", configuracoes),
  FunctionByNomeWithResponse("setConfiguracoes", setConfiguracoes)
};

void processarComandoRecebidoCliente(String a_comando) {
  DynamicJsonDocument l_doc(1024);
  DynamicJsonDocument l_response(1024);
  bool l_found = false;

  deserializeJson(l_doc, a_comando);

  l_response["nome"] = l_doc["nome"];
  for (unsigned int f = 0; f < sizeof(g_functionsFromClient) / sizeof(FunctionByNomeWithResponse); ++f) {
    FunctionByNomeWithResponse& l_f = g_functionsFromClient[f];
    if (l_doc["nome"] == l_f.nome) {
      l_found = true;
      l_response["sucess"] = true;
      l_f.fun_ptr(l_doc, l_response);
      break;
    }
  }

  if (!l_found) {
    l_response["sucess"] = false;
    l_response["error"] = "Método não encontrado";
  }

  serializeJson(l_response, g_client);
  g_client.println("");
}

/*

  int volume = 0; // medida em mm
  int volumePerc = 0;
  int umidade = 0; // 0 - 1023
  int iluminacao = 0; // 0 - 1023
  int iluminacaoPerc = 0;
  int temperatura = 0; // 0 - 1023

*/
void setSensores(const DynamicJsonDocument& a_data) {
  Sensores l_new;

  l_new.volume = a_data["volume"];
  l_new.umidade = a_data["umidade"];
  l_new.iluminacao = a_data["iluminacao"];
  l_new.temperatura = a_data["temperatura"];

  if (l_new.iluminacao > g_configuracoes.vlrSensorIluminacaoMax) {
    g_configuracoes.vlrSensorIluminacaoMax = l_new.iluminacao;
    g_configuracoesDirty = true;
  }

  l_new.volumePerc = map(l_new.volume, g_configuracoes.vlrSensorVolumeVazio, g_configuracoes.vlrSensorVolumeCheio, 0, 100);
  l_new.iluminacaoPerc = map(l_new.iluminacao, 0, g_configuracoes.vlrSensorIluminacaoMax, 0, 100);

  if (g_sensores == l_new)
    return;

  g_sensores = l_new;

  if (g_client.connected()) {
    DynamicJsonDocument l_resp(512);
    
    l_resp["sucess"] = true;
    l_resp["umidade"] = g_sensores.umidade;
    l_resp["iluminacao"] = g_sensores.iluminacaoPerc;
    l_resp["temperatura"] = g_sensores.temperatura;
    l_resp["volume"] = g_sensores.volumePerc;
    
    serializeJson(l_resp, g_client);
    g_client.println("");
  }
}

struct FunctionByNomeNoResponse {
  String nome;
  void (*fun_ptr)(const DynamicJsonDocument&);

  FunctionByNomeNoResponse();
  FunctionByNomeNoResponse(String a_nome, void (*a_fun_ptr)(const DynamicJsonDocument&)) :
    nome(a_nome), fun_ptr(a_fun_ptr) {}
};

FunctionByNomeNoResponse g_functionsFromSerial[] = {
  FunctionByNomeNoResponse("setSensores", setSensores)
};

void processarComandoRecebidoSerial(String a_comando) {
  DynamicJsonDocument l_doc(512);
  bool l_found = false;

  deserializeJson(l_doc, a_comando);

  for (unsigned int f = 0; f < sizeof(g_functionsFromSerial) / sizeof(FunctionByNomeNoResponse); ++f) {
    FunctionByNomeNoResponse& l_f = g_functionsFromSerial[f];
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

void verificarSeTemNovoClient() {
  if (g_server.hasClient()) {
    g_client = g_server.available();
    Serial.println("New client arived");
  }
}

void verificarSeTemDadosNaSerial() {
  while (Serial.available()) {
    String l_data = Serial.readStringUntil('\n');
    if (l_data.length() == 0) break;
    processarComandoRecebidoSerial(l_data);
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
    processarComandoRecebidoCliente(l_data);
  }
}


// struct RegraItem {
//   bool ativa = false;
//   String operador;
//   int valor = 0;

//   // {"ativa": false, "operador": -1, "valor": 30}
// };

// struct Regra {
//   bool ativa = false;
//   int horarioInicio = 0;
//   int horarioFinal = 0;
//   RegraItem sensores[3]; // index é umidade = 0, temperatura = 1, iluminacao = 2
//   int tempoParaRegar = 0;
//   // {"ativa":true, "sensores": [{"ativo": false, "operador": -1, "valor": 30}], "tempoParaRegar": 10}
// };

bool validarRegraParaLigarBomba(const Regra& a_r) {
  if (!a_r.ativa) return false;
  return false;
}

void iniciarRegaSeNecessario() {
  if (g_millisBombaLigada != 0) return; // ligada

  for (unsigned int i = 0; i < (sizeof(g_configuracoes.regras) / sizeof(Regra)); ++i) {
    const Regra& l_r = g_configuracoes.regras[i];
    if (validarRegraParaLigarBomba(l_r)) {
      g_millisParaManterBombaLigada = max(g_millisParaManterBombaLigada, ((unsigned long)(l_r.tempoParaRegar) * 1000));
    }
  }
  
  if (g_millisParaManterBombaLigada > 0) {
      g_millisBombaLigada = millis();
      Serial.println("{\"nome\": \"ligarBomba\"}");
  }
}

void encerrarRegaSeNecessario() {
  if (g_millisBombaLigada == 0) return; // desligada
  if ((millis() - g_millisBombaLigada) < g_millisParaManterBombaLigada)
    return;

  g_millisBombaLigada = 0;
  g_millisParaManterBombaLigada = 0;
  Serial.println("{\"nome\": \"desligarBomba\"}");
}

void loop() {
  verificarBuscarNaWebADataHora();
  gravarConfiguracoes();
  verificarSeTemNovoClient();
  verificarSeTemDadosNaSerial();
  verificarSeTemDadosNoCliente();
  iniciarRegaSeNecessario();
  encerrarRegaSeNecessario();

  delay(10);
}
