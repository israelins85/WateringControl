# WateringControl
An Arduino program to control plants Watering


Aplicativo WateringControl
- Dashboard:
 - mostrar volume do vasilhame;
 - umidade atual do solo;
 - iluminação atual;
 - ultima vez que aguou;
 - acionamento manual da bomba;
- Parametros:
 - regras para ligar a bomba;
- Histórico:
 - Últimas regas;

Arduino:
 - dependencias:
    - Wifi:
        - arduino com wifi;
        - roteador;
    - HTTP;
    - Json: https://arduinojson.org;
    - EEPROM;
    - TinyWebDb API;
    - Time library;
    - HCSR04 (sensor de distância);
 - ao ligar ler do EEPROM os parametros: regras, volume do reservatório e data/hora da ultima rega;
 - ler sensores:
    - verificar alterações no volume do reservatório;
    - verificar se tem que efetuar uma rega:
        - lembrar quando foi a ultima rega automática para não aguar novamente enquanto a água se espalha;
        - guardar no EEPROM um histórico de regas ainda n enviadas para web;
 - ao enviar pra web, remover a rega do histórico:
   - post em http://tinywebdb.appinventor.mit.edu/storeavalue com Content-Type: application/x-www-form-urlencoded e body tag=teste&value=value
 - receber as requisições via TCP do aplicativo:
  - calibrarReservatorioVazio: faz o arduino gravar qual o valor do sensor de distância para reservatório vazio;
  - calibrarReservatorioCheio: faz o arduino gravar qual o valor do sensor de distância para reservatório cheio;
  - sensores: devolve a situação atual de todos os sensores;
  - regaManual: aciona bomba de forma manual;
  - regras: lê as regras atuais;
  - setRegras: altera as regras para rega automática;
