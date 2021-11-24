#include "ArduinoJson.h"
// https://arduinojson.org/?utm_source=meta&utm_medium=library.properties

// dinamicas
int g_vlrSensorVolume = 0; // 0 - 1023
int g_vlrSensorUmidade = 0; // 0 - 1023
int g_vlrSensorIluminacao = 0; // 0 - 1023

StaticJsonDocument<256> g_ultComandoRecebido;
StaticJsonDocument<256> g_reposta;

enum class Sensor {
  Nenhum = -1,
  Umidade,
  Temperatura,
  Iluminacao  
};

enum class Operador { // tem q ser de acordo com o index do combo na aplicação
  Menor = 0,
  Igual = 1,
  Maior = 2  
};

struct RegraItem {
  bool ativa = false;
  char operador;
  int valor = 0;

  // {"ativa": false, "operador": -1, "valor": 30}
};

struct Regra {
  bool ativa = false;
  RegraItem sensores[3]; // index é igual ao enum Sensor
  int tempoParaRegar = 0;
  
  // {"ativa":true, "sensores": [{"ativo": false, "operador": -1, "valor": 30}], "tempoParaRegar": 10}
};

String g_wifiSsid = "WateringControl";
String g_wifiPass = "Watering1234";
String g_wifiIp = "192.168.0.5";

// configurações
int g_vlrSensorVolumeMin = 0; // 0 - 10000
int g_vlrSensorVolumeMax = 10000; // 0 - 10000
struct Configuracoes {
  long long tempoMinEntreAsRegas = 0;
  Regra g_regras[5];
};
Configuracoes g_configuracoes;

// Caio
void lerConfiguracao() {
  // ler o json
}

// Caio
void gravarConfiguracao() {
  // gravar como json
}

// Felipe
void inicializarWifi() {
  // verificar como inicializar o wifi
}

// Soter
void setupPortas() {
  // inicializar as portas dos sensores
}

// Israel
void setup() {
  lerConfiguracao();
  setupPortas();
  inicializarWifi();
}

// Felipe
bool verificarConexaoWifi() {
  // retornar true se a conexão estiver ok
  // senão false
  g_wifiClient
}

// Israel
void retornaComandoProcessado() {
  // escrever no socket

}

// Gean
void calibrarReservatorioVazio() {
  // preencher a variavel e gravar a config
  g_vlrSensorVolumeMin = g_vlrSensorVolume;
  g_reposta.clear();
  if (!gravarConfiguracao()) {
    g_reposta["sucess"] = false;
    g_reposta["msg"] = "Erro ao salvar";
  } else {
    g_reposta["sucess"] = true;
  }
  retornaComandoProcessado();
}

// Gean
void calibrarReservatorioCheio() {
  // preencher a variavel e gravar a config
  g_vlrSensorVolumeMax = g_vlrSensorVolume;
  gravarConfiguracao();
  retornaComandoProcessado();
}

// Gean
void sensores() {
    g_reposta["sucess"] = true;
    g_reposta["umidade"] = g_vlrSensorUmidade;
    g_reposta["sucess"] = true;
    g_reposta["sucess"] = true;
  retornaComandoProcessado();
}

// Gean
void regaManual() {
  // acionar a rega
  if (regar()) {
    g_reposta["sucess"] = true;    
  } else {

  }
  retornaComandoProcessado();
}

// Gean
void configuracoes() {
  // preencher a variavel e gravar a config
  retornaComandoProcessado();
}

// Gean JsonArray =>
// [
//   {"ativa":true, [{"sensor": 1, "operador": -1, "valor": 30}], "tempoParaRegar": 10},
//   {"ativa":true, [{"sensor": 1, "operador": -1, "valor": 70}, {"sensor": 2, "operador": -1, "valor": 50}], "tempoParaRegar": 5}
// ]
void setConfiguracoes() {
  // preencher a variavel e gravar a config
  retornaComandoProcessado();
}

// Caio
void processarComandoRecebido() {
  // para cada possível comando, chamar o método
  if (stricmp(g_ultComandoRecebido["nome"], "calibrarReservatorioVazio") == 0) {
    calibrarReservatorioVazio();
  }
  if (stricmp(g_ultComandoRecebido["nome"], "setConfiguracoes") == 0) {
    setConfiguracoes();
  }
}

// Israel
void receberComando() {
  if (!g_conecao.avail()) return;
  deserializeJson(g_ultComandoRecebido, g_wifiClient);
  processarComandoRecebido();
}

// Soter
void lerSensores() {
  // preencher os valores dos sensores

}

// Soter
void acenderLedReservatorio() {
  
}

// Soter
bool regar() {
  // verificar volume e regar se tiver água
  if (g_vlrSensorVolume > g_vlrSensorVolumeMin) {
    return false;
  }

  acionarBomba();
  return true;
}

// Soter e Israel
void verificarSeDesligaABomba() {
  // se tiver passado o tempo pra desligar

}

// Soter e Israel
void verificarSePrecisaRegar() {
  // evaluation das regras
  
  verificarSeDesligaABomba();
}

// Soter
void exibirSensorNoLCD() {

}

// Israel
void loop() {
  lerSensores();
  acenderLedReservatorio();

  if (verificarConexaoWifi())
    receberComando();

  verificarSePrecisaRegar();

  delay(10);  
}
