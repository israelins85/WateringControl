#include "ArduinoJson.h"
#include <EEPROM.h>
#include "Ultrasonic.h"
#include <stdlib.h>

#define PS_TEMP A0
#define PS_ILUM A1
#define PS_UMIDADE A2

#define PS_ULTSOM_TRIG 4
#define PS_ULTSOM_ECHO 5

#define P_BOMBA 1

#define P_LCD1 A4
#define P_LCD2 A5

// dinamicas
// Inicializa o sensor nos pinos definidos acima
Ultrasonic g_ultrasonic(PS_ULTSOM_TRIG, PS_ULTSOM_ECHO);
int g_vlrSensorVolume = 0; // medida em mm
int g_vlrSensorUmidade = 0; // 0 - 1023
int g_vlrSensorIluminacao = 0; // 0 - 1023
int g_vlrSensorTemperatura = 0; // 0 - 1023

int g_millisBombaLigada = 0;
int g_millisParaManterBombaLigada = 0;

struct RegraItem {
  bool ativa = false;
  String operador;
  int valor = 0;

  // {"ativa": false, "operador": -1, "valor": 30}
};

struct Regra {
  bool ativa = false;
  int horarioInicio = 0;
  int horarioFinal = 0;
  RegraItem sensores[3]; // index é umidade = 0, temperatura = 1, iluminacao = 2
  int tempoParaRegar = 0;
  
  // {"ativa":true, "sensores": [{"ativo": false, "operador": -1, "valor": 30}], "tempoParaRegar": 10}
};

// configurações
int g_currentConfiguracoesStructVersion = 0;
struct Configuracoes {
  int vlrSensorVolumeMin = 0; // em cm
  int vlrSensorVolumeMax = 100; // em cm
  int vlrSensorIluminacaoMax = 0; // em cm
  long long tempoMinEntreAsRegas = 0;
  Regra regras[5];
};
Configuracoes g_configuracoes;
bool g_configuracoesDirty = false;

unsigned long crc_table[16] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
  0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
  0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

template<typename T>
char calculateCheckSum(T a_data) {
  unsigned long crc = ~0L;
  char* l_e = (char*)(&a_data);
  int n = 0;

  while (n < sizeof(T)) {
    char byte = l_e[n];    
    crc = crc_table[(crc ^ byte) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (byte >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
    n++;
  }

  return crc;
}

template<typename T>
T lerEEPROM(int& initialOffset) {
  T l_ret;
  char* l_e = (char*)(&l_ret);
  for (int addr = 0; addr < sizeof(T); ++addr) {
    l_e[addr] = EEPROM[initialOffset + addr];
  }
  initialOffset += sizeof(T);

  return l_ret;
}

template<typename T>
void gravarEEPROM(int initialOffset, const T& a_data) {
  T l_ret;
  const char* l_e = (const char*)(&a_data);
  for (int addr = 0; addr < sizeof(T); ++addr) {
    EEPROM.update(initialOffset + addr, l_e[addr]);
  }
}

Configuracoes configuracoesIniciais() {
  Configuracoes l_ret;

  Regra& l_regra = l_ret.regras[0];
  RegraItem& l_item = l_regra.sensores[0];

  l_regra.ativa = true;
  l_item.ativa = true;
  l_item.operador = "<";
  l_item.valor = 70;

  return l_ret;
}

void lerConfiguracao() {
  Serial.print("EEPROM length: ");
  Serial.println(EEPROM.length());
  Serial.print("Configuracoes length: ");
  Serial.println(sizeof(Configuracoes));

  int offset = 0;
  int structVersion = lerEEPROM<int>(offset);
  unsigned long expectedCrc = lerEEPROM<unsigned long>(offset);

  // prepared for future migrations of struct
  if (structVersion == g_currentConfiguracoesStructVersion) {
    Configuracoes l_lido = lerEEPROM<Configuracoes>(offset);    
    unsigned long crc = calculateCheckSum(l_lido);
    if (crc == expectedCrc) {
        g_configuracoes = l_lido;    
        return;      
    }
  }
  
  g_configuracoes = configuracoesIniciais();     
}

bool gravarConfiguracao() {
  if (!g_configuracoesDirty) return false; 
  unsigned long crc = calculateCheckSum(g_configuracoes);

  gravarEEPROM(0, g_currentConfiguracoesStructVersion);
  gravarEEPROM(4, crc);
  gravarEEPROM(8, g_configuracoes);
  g_configuracoesDirty = false;

  return true;
}

void setupPortas() {
  // inicializar as portas dos sensores
  Serial.begin(115200);
  
  pinMode(PS_TEMP, INPUT);
  pinMode(PS_ILUM, INPUT);
  pinMode(PS_UMIDADE, INPUT);
  pinMode(P_BOMBA, OUTPUT);
}

void setup() {
  lerConfiguracao();
  setupPortas();
}

void retornaComandoProcessado() {
  // escrever no socket

}

void calibrarReservatorioVazio() {
  // preencher a variavel e gravar a config
  g_configuracoes.vlrSensorVolumeMin = g_vlrSensorVolume;
  g_configuracoesDirty = true;
  if (!gravarConfiguracao()) {
    g_2send["sucess"] = false;
    g_2send["msg"] = "Erro ao salvar";
  } else {
    g_2send["sucess"] = true;
  }
  retornaComandoProcessado();
}

void calibrarReservatorioCheio() {
  // preencher a variavel e gravar a config
  g_configuracoes.vlrSensorVolumeMax = g_vlrSensorVolume;
  g_configuracoesDirty = true;
  if (!gravarConfiguracao()) {
    g_2send["sucess"] = false;
    g_2send["msg"] = "Erro ao salvar";
  } else {
    g_2send["sucess"] = true;
  }
  retornaComandoProcessado();
}

void sensores() {
  g_2send["sucess"] = true;
  g_2send["umidade"] = g_vlrSensorUmidade;
  g_2send["iluminacao"] = g_vlrSensorIluminacao;
  g_2send["temperatura"] = g_vlrSensorTemperatura;
  g_2send["volume"] = map(g_vlrSensorVolume, g_configuracoes.vlrSensorVolumeMin, g_configuracoes.vlrSensorVolumeMax, 0, 100);
  retornaComandoProcessado();
}

bool regar();

void regaManual() {
  // acionar a rega
  if (regar()) {
    g_2send["sucess"] = true;    
  } else {
    
  }
  retornaComandoProcessado();
}

void configuracoes(const DynamicJsonDocument& /*a_cmd*/, DynamicJsonDocument& a_resp) {
  // preencher a variavel e gravar a config
  retornaComandoProcessado();
}

// JsonArray =>
// [
//   {"ativa":true, [{"sensor": 1, "operador": -1, "valor": 30}], "tempoParaRegar": 10},
//   {"ativa":true, [{"sensor": 1, "operador": -1, "valor": 70}, {"sensor": 2, "operador": -1, "valor": 50}], "tempoParaRegar": 5}
// ]
void setConfiguracoes(const DynamicJsonDocument& a_cmd, DynamicJsonDocument& a_resp) {
  // preencher a variavel e gravar a config

  g_configuracoesDirty = true;
  if (!gravarConfiguracao()) {
    g_2send["sucess"] = false;
    g_2send["msg"] = "Erro ao salvar";
  } else {
    g_2send["sucess"] = true;
  }
  retornaComandoProcessado();
}

struct FunctionByNome {
  String nome;
  void (*fun_ptr)(const DynamicJsonDocument&, DynamicJsonDocument&);

  FunctionByNome();
  FunctionByNome(String a_nome, void (*a_fun_ptr)(const DynamicJsonDocument&, DynamicJsonDocument&)) :
    nome(a_nome), fun_ptr(a_fun_ptr) {}
};

FunctionByNome g_functions[] = {
  FunctionByNome("calibrarReservatorioVazio", calibrarReservatorioVazio)
};

void processarComandoRecebido(String a_comando, Stream& a_quemPediu) {
  DynamicJsonDocument l_doc(1024);
  DynamicJsonDocument l_response(1024);
  deserializeJson(l_doc, a_comando);

  l_response["nome"] = l_doc["nome"];
  for (int f = 0; f < sizeof(g_functions) / sizeof(FunctionByNome); ++f) {
    l_doc["nome"] == String("currentUnixEpoch")
  }

  if (!l_found) {
    l_response["sucess"] = false;
    l_response["error"] = "Método não encontrado";
  }

  if (a_quemPediu == Serial) {
      a_quemPediu.print("ToArduino:");
  }
  serializeJson(l_response, a_quemPediu);
  a_quemPediu.println("");
}

void processarComandoRecebido(String a_comando) {
  DynamicJsonDocument l_doc(1024);
  deserializeJson(l_doc, a_comando);

  // para cada possível comando, chamar o método
  if (l_doc["nome"] == String("calibrarReservatorioVazio")) {
    calibrarReservatorioVazio();
  }
  if (l_doc["nome"] == String("setConfiguracoes")) {
    setConfiguracoes(l_doc);
  }
}

void receberComando() {
  while (Serial.available()) {
    String l_data = Serial.readStringUntil('\n');
    if (l_data.startsWith("ToArduino:")) {
      processarComandoRecebido(l_data.substring(6));
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

void lerSensores() {
  g_ultrasonic.update();
  
  // preencher os valores dos sensores
  g_vlrSensorTemperatura = tempSensorToCelsios(analogRead(PS_TEMP));
  g_vlrSensorUmidade = sensorToPercent(1023 - analogRead(PS_UMIDADE));
  if (g_ultrasonic.measureIsAvaible())
    g_vlrSensorVolume = g_ultrasonic.measureMM();

  g_vlrSensorIluminacao = analogRead(PS_ILUM);
  if (g_configuracoes.vlrSensorIluminacaoMax < g_vlrSensorIluminacao) {
    g_configuracoes.vlrSensorIluminacaoMax  = g_vlrSensorIluminacao;
    g_configuracoesDirty = true;
  }
  g_vlrSensorIluminacao = sensorToPercent(g_vlrSensorIluminacao, 0, g_configuracoes.vlrSensorIluminacaoMax);



  Serial.print("g_vlrSensorTemperatura");
  Serial.println(g_vlrSensorTemperatura);

  Serial.print("g_vlrSensorUmidade");
  Serial.println(g_vlrSensorUmidade);

  Serial.print("g_vlrSensorVolume");
  Serial.println(g_vlrSensorVolume);

  Serial.print("g_vlrSensorIluminacao");
  Serial.println(g_vlrSensorIluminacao);

  delay(1000);
}

void alertarReservatorioVazio() {
  
}

bool regar() {
  // verificar volume e regar se tiver água
  if (g_vlrSensorVolume > g_configuracoes.vlrSensorVolumeMin) {
    return false;
  }

  // acionar Bomba
  return true;
}

void verificarSeDesligaABomba() {
  // já esta desligada
  if (g_millisBombaLigada == 0) return;
  // se tiver passado o tempo pra desligar

}

void verificarSePrecisaRegar() {
  // já esta regando
  if (g_millisBombaLigada != 0) return;
  // evaluation das regras
  
}

enum class LCDState {
  None = -1,
  MostrandoIP,
  MostrandoVolume,
  MostrandoSensores,
  MostrandoUltRega,
  Last = MostrandoUltRega
};

unsigned long g_lastLcdStateChange = 0;
LCDState g_lcdState = LCDState::None;

String g_ip = "172.16.11.11";
int g_vlrSensorVolume = 5; // medida em mm
int g_vlrSensorVolumeCheio = 5; // medida em mm
int g_vlrSensorVolumeVazio = 32; // medida em mm
int g_vlrSensorUmidade = 27; // 0 - 1023
int g_vlrSensorIluminacao = 49; // 0 - 1023
int g_vlrSensorTemperatura = 32; // 0 - 1023

void atualizaMensagemLCD() {
  int l_millis = millis();

  if ((g_lastLcdStateChange != 0) && (abs(l_millis - g_lastLcdStateChange) < 2000))
    return;

  g_lastLcdStateChange = l_millis;
  
  if (g_lcdState == LCDState::Last) 
    g_lcdState = LCDState::None;
  g_lcdState = LCDState(int(g_lcdState) + 1);

  switch(g_lcdState) {
    case MostrandoIP: {
      // escrever IP: g_ip
    } break;
    case MostrandoVolume: {
      g_vlrSensorVolume++; // estou alterando para simular ele secando
      if (g_vlrSensorVolume > g_vlrSensorVolumeVazio)
        g_vlrSensorVolume = g_vlrSensorVolumeCheio;
      // escrever VOLUME: e barra
    } break;
    case MostrandoSensores: {
      // escrever TEMP: 32oC ILUM: 49 UMI: 27
    } break;
  }
}

void loop() {
  lerSensores();
  alertarReservatorioVazio();
  receberComando();
  verificarSePrecisaRegar();
  verificarSeDesligaABomba();
  gravarConfiguracao();

  delay(10);  
}
