#include <WiFi.h>          // Biblioteca para conexão Wi-Fi
#include <PubSubClient.h>  // Biblioteca para comunicação MQTT
#include <DHT.h>           // Biblioteca para o sensor DHT

// === DEFINIÇÕES DE PINOS ===
#define PINO_DHT 5         // Pino conectado ao sensor DHT22
#define TIPO_DHT DHT22     // Tipo do sensor
#define PINO_LDR 34        // Pino analógico para LDR (luminosidade)
#define PINO_LED 2         // Pino de saída para controle do LED

// === CREDENCIAIS DE CONEXÃO ===
const char* REDE_WIFI = "Wokwi-GUEST";  // Nome da rede Wi-Fi
const char* SENHA_WIFI = "";            // Senha da rede Wi-Fi
const char* BROKER_MQTT = "34.9.104.48"; // IP do Broker MQTT
const int PORTA_MQTT = 1883;            // Porta de comunicação MQTT
const char* TOPICO_SUB = "/TEF/lamp001/cmd";     // Tópico de inscrição 
const char* TOPICO_PUB_STATUS = "/TEF/lamp001/attrs"; // Tópico para publicar status 
const char* TOPICO_PUB_LUZ = "/TEF/lamp001/attrs/l";  // Tópico para publicar luminosidade
const char* TOPICO_PUB_TEMP = "/TEF/lamp001/attrs/t"; // Tópico para publicar temperatura
const char* TOPICO_PUB_UMID = "/TEF/lamp001/attrs/h"; // Tópico para publicar umidade
const char* ID_DISPOSITIVO = "fiware_001";           // ID do dispositivo no Broker
const char* PREFIXO_TOPIC = "lamp001";               // Prefixo para o comando MQTT


#define TEMP_MINIMA 18.0   // Temperatura mínima aceitável
#define TEMP_MAXIMA 28.0   // Temperatura máxima aceitável
#define UMID_MINIMA 30.0   // Umidade mínima aceitável
#define UMID_MAXIMA 70.0   // Umidade máxima aceitável
#define LUZ_MINIMA 20      // Luminosidade mínima aceitável
#define LUZ_MAXIMA 80      // Luminosidade máxima aceitável


WiFiClient clienteWiFi;           // Cliente Wi-Fi
PubSubClient clienteMQTT(clienteWiFi); // Cliente MQTT
DHT sensorDHT(PINO_DHT, TIPO_DHT);     // Sensor DHT

// === VARIÁVEIS ===
char estadoLED = '0';      // Estado atual do LED (0 = desligado 1 = ligado)
bool alertaAtivado = false; // Se um alerta de sensor está ativo
bool controleManual = false; // Se o LED foi controlado manualmente por comando


// Inicializa comunicação serial
void inicializarSerial() {
  Serial.begin(115200);
}

// Configura conexão Wi-Fi
void configurarWiFi() {
  WiFi.begin(REDE_WIFI, SENHA_WIFI);
  Serial.println("Conectando ao Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// Configura servidor MQTT e função de callback
void configurarMQTT() {
  clienteMQTT.setServer(BROKER_MQTT, PORTA_MQTT);
  clienteMQTT.setCallback(receberMensagem);
}

// Reconecta ao Broker MQTT caso desconectado
void reconectarMQTT() {
  while (!clienteMQTT.connected()) {
    Serial.println("Tentando reconectar ao Broker MQTT...");
    if (clienteMQTT.connect(ID_DISPOSITIVO)) {
      Serial.println("Conectado ao Broker!");
      clienteMQTT.subscribe(TOPICO_SUB);
    } else {
      Serial.print("Erro: ");
      Serial.print(clienteMQTT.state());
      Serial.println(". Tentando em 2 segundos...");
      delay(2000);
    }
  }
}

// Função chamada sempre que uma mensagem chega via MQTT
void receberMensagem(char* topico, byte* mensagem, unsigned int comprimento) {
  String comando = "";
  for (unsigned int i = 0; i < comprimento; i++) {
    comando += (char)mensagem[i];
  }
  Serial.print("Comando recebido: ");
  Serial.println(comando);

  String comandoLigar = String(PREFIXO_TOPIC) + "@on|";   // Comando para ligar
  String comandoDesligar = String(PREFIXO_TOPIC) + "@off|"; // Comando para desligar

  if (comando.equals(comandoLigar)) {
    digitalWrite(PINO_LED, HIGH);
    estadoLED = '1';
    controleManual = true;
  }
  if (comando.equals(comandoDesligar)) {
    digitalWrite(PINO_LED, LOW);
    estadoLED = '0';
    controleManual = true;
  }
}

// Função para ler sensores e enviar valores
void monitorarSensores() {
  float temperatura = sensorDHT.readTemperature(); // Leitura da temperatura
  float umidade = sensorDHT.readHumidity();         // Leitura da umidade
  int leituraLDR = analogRead(PINO_LDR);            // Leitura do LDR
  int luminosidade = map(leituraLDR, 0, 4095, 0, 100); // Conversão para 0-100%

  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("Falha na leitura do DHT22!");
    return;
  }

  String tempString = String(temperatura, 1);
  String umidString = String(umidade, 1);
  String lumiString = String(luminosidade);

  // Impressão formatada um embaixo do outro
  Serial.println("=======================");
  Serial.print("Temperatura: ");
  Serial.print(tempString);
  Serial.println(" °C");
  
  Serial.print("Umidade: ");
  Serial.print(umidString);
  Serial.println(" %");

  Serial.print("Luminosidade: ");
  Serial.print(lumiString);
  Serial.println(" %");
  Serial.println("=======================");

  // Publicação no Broker MQTT
  clienteMQTT.publish(TOPICO_PUB_TEMP, tempString.c_str());
  clienteMQTT.publish(TOPICO_PUB_UMID, umidString.c_str());
  clienteMQTT.publish(TOPICO_PUB_LUZ, lumiString.c_str());

  // Checar se os parâmetros estão fora dos limites
  checarParametros(temperatura, umidade, luminosidade);
}

// Verifica se as leituras dos sensores estão normais
void checarParametros(float temp, float umid, int luz) {
  if (controleManual) return;

  alertaAtivado = false;

  if (temp < TEMP_MINIMA || temp > TEMP_MAXIMA) {
    alertaAtivado = true;
    Serial.println("Alerta: Temperatura anormal!");
  }
  if (umid < UMID_MINIMA || umid > UMID_MAXIMA) {
    alertaAtivado = true;
    Serial.println("Alerta: Umidade anormal!");
  }
  if (luz < LUZ_MINIMA || luz > LUZ_MAXIMA) {
    alertaAtivado = true;
    Serial.println("Alerta: Luminosidade anormal!");
  }
}

// Pisca o LED para indicar alerta
void piscarLEDAlerta() {
  if (alertaAtivado && !controleManual) {
    digitalWrite(PINO_LED, HIGH);
    delay(300);
    digitalWrite(PINO_LED, LOW);
    delay(300);
  }
}

// Envia o status do LED para o Broker
void enviarStatusLED() {
  if (estadoLED == '1') {
    clienteMQTT.publish(TOPICO_PUB_STATUS, "s|on");
    Serial.println("LED Ligado.");
  } else {
    clienteMQTT.publish(TOPICO_PUB_STATUS, "s|off");
    Serial.println("LED Desligado.");
  }
  delay(1000);
}

// Verifica se Wi-Fi e MQTT estão conectados
void manterConexoes() {
  if (!clienteMQTT.connected()) reconectarMQTT();
  if (WiFi.status() != WL_CONNECTED) configurarWiFi();
}

// Inicializa o pino do LED
void inicializarHardware() {
  pinMode(PINO_LED, OUTPUT);
  digitalWrite(PINO_LED, LOW);
}

// === FUNÇÕES PRINCIPAIS ===

// Setup inicial
void setup() {
  inicializarSerial();
  inicializarHardware();
  configurarWiFi();
  configurarMQTT();
  sensorDHT.begin();
  delay(3000);
  clienteMQTT.publish(TOPICO_PUB_STATUS, "s|on"); // Informa que o dispositivo está online
}

// Loop principal
void loop() {
  manterConexoes();     // Garante que Wi-Fi e MQTT estão ativos
  monitorarSensores();  // Faz leituras dos sensores
  piscarLEDAlerta();    // Pisca LED se houver alerta
  enviarStatusLED();    // Envia estado do LED
  clienteMQTT.loop();   // Mantém a comunicação MQTT funcionando
}
