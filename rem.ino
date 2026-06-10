#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include "DHT.h"

// --- Credenciais do Wi-Fi ---
const char* ssid = "Magno";
const char* password = "morpheus";

// --- Endpoints e Credenciais da API ---
const char* graphql_endpoint = "https://beatrice-z22c.onrender.com/graphql";
const char* token_autenticacao = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpZCI6ImNvbGxhYm9yYXRvcl8wMSIsImVtYWlsIjoib3BlcmFkb3JAemVj.zoobaby"; // Substitua pelo seu token válido
const char* enclosure_id = "cXFA6qSn1pisMEFOyhp6"; 

// --- Configurações do Servidor de Hora (NTP) ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800; // Fuso -3h
const int   daylightOffset_sec = 0;

// --- Configurações dos Sensores ---
#define DHTPIN 4     
#define DHTTYPE DHT22  
DHT dht(DHTPIN, DHTTYPE);

#define PINO_LDR 34  
#define PINO_SOM 35  

// --- Configurações dos Pinos dos Atuadores (Relés) ---
#define PINO_VENTILADOR  25 // Esfria o ambiente (Temp > tempMax)
#define PINO_AQUECEDOR   14 // Esquenta o ambiente (Temp < tempMin)
#define PINO_NEBULIZADOR 27 // Adiciona umidade (Umid < umidMin)
#define PINO_EXAUSTOR    26 // NOVO: Retira umidade (Umid > umidMax) - Substituiu a Lâmpada

// --- Variáveis de Leitura Instantânea ---
float tempAtual = 0.0;
float umidAtual = 0.0;
float luzAtual = 0.0; // Guardará o valor convertido em Lux
float somAtual = 0.0;

// --- Variáveis de Acumulação para o Envio (60 segundos) ---
unsigned long somaLuz = 0; 
int contagemLuz = 0;       // <--- ADICIONE ESTA LINHA AQUI

// --- Variáveis de Amostragem do Som ---
// --- Variáveis de Amostragem do Som ---
float picoSom60s = 0.0;    
int sinalMax = 0;          // Guarda o ponto mais alto da onda
int sinalMin = 4095;       // Guarda o ponto mais baixo da onda

// --- Variáveis de Limites (Atualizadas dinamicamente pela API) ---
float tempMax = 0.0, tempMin = 0.0;
float umidMax = 0.0, umidMin = 0.0; 
float noiseMax = 0.0;

// --- Variáveis de Estado dos Atuadores ---
bool estadoFan = false;
bool estadoHeater = false;
bool estadoNebulizer = false;
bool estadoExaustor = false; // Substituiu o estado da lâmpada

// --- Controle de Tempo ---
unsigned long tempoAnteriorSensores = 0;
unsigned long tempoAnteriorSom = 0;
unsigned long tempoAnteriorEnvio = 0;
unsigned long tempoAnteriorLimites = 0;

const long intervaloSensores = 2000;  
const long intervaloSom = 5000;       
const long intervaloEnvio = 60000;    
const long intervaloLimites = 30000;  

// Protótipos das Funções
void buscarLimitesDoRecinto();
void enviarDadosParaAPI(float tempEnvio, float umidEnvio, float luzEnvio, float somEnvio);
void lerSensores();
void controlarAtuadores();
void atualizarEstadoAtuador(String tipo, int pino, bool novoEstado, bool &estadoAtual);
void enviarAtuadorAPI(String actuatorType, bool state);
String getTimestampAtual();

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(PINO_VENTILADOR, OUTPUT);
  pinMode(PINO_AQUECEDOR, OUTPUT);
  pinMode(PINO_NEBULIZADOR, OUTPUT);
  pinMode(PINO_EXAUSTOR, OUTPUT);
  
  digitalWrite(PINO_VENTILADOR, LOW);
  digitalWrite(PINO_AQUECEDOR, LOW);
  digitalWrite(PINO_NEBULIZADOR, LOW);
  digitalWrite(PINO_EXAUSTOR, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Conectando au WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sincronizando hora...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(1000); Serial.print(".");
  }
  Serial.println("\nHora sincronizada com sucesso!");

  buscarLimitesDoRecinto();
}

void loop() {
  unsigned long tempoAtual = millis();

  // 1. Amostragem contínua da Onda Sonora (Lê os picos altos e baixos)
  int leituraSom = analogRead(PINO_SOM);
  if (leituraSom > sinalMax) { sinalMax = leituraSom; }
  if (leituraSom < sinalMin) { sinalMin = leituraSom; }

  // 2. Janela de 5s do Som (Calcula a Amplitude)
  if (tempoAtual - tempoAnteriorSom >= intervaloSom) {
    tempoAnteriorSom = tempoAtual;

    // A mágica acontece aqui: O tamanho real do barulho!
    int amplitude = sinalMax - sinalMin;

    // Filtra o ruído estático do próprio circuito elétrico (pequenas variações no silêncio)
    if (amplitude < 30) { 
      amplitude = 0; 
    }

    // Agora mapeamos a AMPLITUDE (variação), e não mais o valor bruto
    // Teto de 1500 (Ajuste esse valor: diminua para ficar mais sensível a barulhos de longe)
    somAtual = map(amplitude, 0, 1500, 0, 100);
    somAtual = constrain(somAtual, 0, 100);

    if (somAtual > picoSom60s) {
      picoSom60s = somAtual;
    }

    Serial.print("[Som 5s] Amplitude: "); Serial.print(amplitude); 
    Serial.print(" -> Ruido: "); Serial.print(somAtual); Serial.println(" dB");

    // Reseta as variáveis para a próxima onda de 5 segundos
    sinalMax = 0;
    sinalMin = 4095; 
  }

  // 3. Sensores Regulares e Atuadores (A cada 2s)
  if (tempoAtual - tempoAnteriorSensores >= intervaloSensores) {
    tempoAnteriorSensores = tempoAtual;
    lerSensores();
    controlarAtuadores(); 
  }

  // 4. Envio de Telemetria (A cada 60s)
  if (tempoAtual - tempoAnteriorEnvio >= intervaloEnvio) {
    tempoAnteriorEnvio = tempoAtual;
    
    if (tempAtual != 0.0) {
      float luzMediaEnvio = 0.0;
      if (contagemLuz > 0) {
        luzMediaEnvio = (float)somaLuz / contagemLuz;
      }

      Serial.print("\n[Calculo] Fechando o minuto -> Luz Media: "); Serial.print(luzMediaEnvio); Serial.print(" Lux");
      Serial.print(" | Maior Ruido (Pico): "); Serial.println(picoSom60s);

      enviarDadosParaAPI(tempAtual, umidAtual, luzMediaEnvio, picoSom60s);

      somaLuz = 0;
      contagemLuz = 0;
      picoSom60s = 0;
    }
  }

  // 5. Atualização de Limites (A cada 30s)
  if (tempoAtual - tempoAnteriorLimites >= intervaloLimites) {
    tempoAnteriorLimites = tempoAtual;
    buscarLimitesDoRecinto();
  }
}

// ==============================================================================
// LEITURA DOS SENSORES (COM CONVERSÃO PARA LUX)
// ==============================================================================
void lerSensores() {
  float umidadeLida = dht.readHumidity();
  float temperaturaLida = dht.readTemperature();

  if (!isnan(umidadeLida) && !isnan(temperaturaLida)) {
    tempAtual = temperaturaLida;
    umidAtual = umidadeLida;
  }

  // Conversão matemática do LDR para Lux Reais
  int leituraBrutaLuz = analogRead(PINO_LDR);
  float luxReal = 0.0;
  float tensao = leituraBrutaLuz * (3.3 / 4095.0);
  
  if (tensao > 0.0) {
    float resistenciaLDR = 10000.0 * (3.3 - tensao) / tensao;
    luxReal = pow((50.0 * 1000.0 * pow(10, 0.7)) / resistenciaLDR, (1.0 / 0.7));
  }
  
  luzAtual = luxReal; // Armazena o valor em Lux
  somaLuz += luzAtual; // Acumula para a média do minuto
  contagemLuz ++;

  Serial.print("[Sensores 2s] Temp: "); Serial.print(tempAtual); 
  Serial.print("C | Umid: "); Serial.print(umidAtual); 
  Serial.print("% | Luz: "); Serial.print(luzAtual); Serial.println(" Lux");
}

// ==============================================================================
// NOVA LÓGICA DOS ATUADORES
// ==============================================================================
void controlarAtuadores() {
  if (tempMax == 0 && tempMin == 0) return; 

  // 1. Controle de Temperatura (Ventilador e Aquecedor)
  bool precisaFan = (tempAtual > tempMax);
  bool precisaHeater = (tempAtual < tempMin);
  atualizarEstadoAtuador("fan", PINO_VENTILADOR, precisaFan, estadoFan);
  atualizarEstadoAtuador("heater", PINO_AQUECEDOR, precisaHeater, estadoHeater);

  // 2. NOVO CONTROLE: Umidade Máxima e Mínima (Nebulizador e Exaustor)
  bool precisaNebulizer = (umidAtual < umidMin); // Falta umidade -> Liga Nebulizador
  bool precisaExaustor = (umidAtual > umidMax);   // Excesso umidade -> Liga Exaustor
  
  atualizarEstadoAtuador("nebulizer", PINO_NEBULIZADOR, precisaNebulizer, estadoNebulizer);
  
  // NOTA: Se o backend der erro 400 por causa da string "exhaust", mude temporariamente para "fan"
  atualizarEstadoAtuador("exhaust", PINO_EXAUSTOR, precisaExaustor, estadoExaustor);
}

void atualizarEstadoAtuador(String tipo, int pino, bool novoEstado, bool &estadoAtual) {
  if (novoEstado != estadoAtual) {
    digitalWrite(pino, novoEstado ? HIGH : LOW);
    estadoAtual = novoEstado; 
    Serial.print("[Atuador] Mudanca! "); Serial.print(tipo); Serial.println(novoEstado ? " -> ATIVO" : " -> INATIVO");
    enviarAtuadorAPI(tipo, novoEstado);
  }
}

// ==============================================================================
// COMUNICAÇÃO GRAPHQL
// ==============================================================================
void enviarAtuadorAPI(String actuatorType, bool state) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(graphql_endpoint);
  http.addHeader("Content-Type", "application/json");
  String authHeader = "Bearer " + String(token_autenticacao);
  http.addHeader("Authorization", authHeader.c_str());

  JsonDocument doc;
  doc["query"] = "mutation($enclosureId: ID!, $actuatorType: String!, $state: Boolean!) { updateActuatorStateFromEsp32(enclosureId: $enclosureId, actuatorType: $actuatorType, state: $state) { success message } }";
  
  JsonObject variables = doc.createNestedObject("variables");
  variables["enclosureId"] = enclosure_id; 
  variables["actuatorType"] = actuatorType;
  variables["state"] = state;

  String payloadJSON;
  serializeJson(doc, payloadJSON);

  int httpResponseCode = http.POST(payloadJSON);
  String responseBody = http.getString(); 

  Serial.print("[API Atuador] " + actuatorType + " -> Resposta HTTP: "); Serial.println(httpResponseCode);
  Serial.print("[API Atuador] Resposta do Servidor: "); Serial.println(responseBody);

  http.end();
}

void enviarDadosParaAPI(float tempEnvio, float umidEnvio, float luzEnvio, float somEnvio) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(graphql_endpoint);
  http.addHeader("Content-Type", "application/json");
  String authHeader = "Bearer " + String(token_autenticacao);
  http.addHeader("Authorization", authHeader.c_str());

  String timestamp = getTimestampAtual();

  String query = "mutation { createVariant(input: { ";
  query += "enclosureId: \"" + String(enclosure_id) + "\", ";
  query += "temp: " + String(tempEnvio, 2) + ", ";
  query += "humidity: " + String(umidEnvio, 2) + ", ";
  query += "noise: " + String(somEnvio, 2) + ", ";
  query += "luminosity: " + String(luzEnvio, 2) + ", "; // Envia o valor calculado em Lux
  query += "timestamp: \"" + timestamp + "\" ";
  query += "}) { id } }";

  JsonDocument doc;
  doc["query"] = query;
  String payloadJSON;
  serializeJson(doc, payloadJSON);

  int httpResponseCode = http.POST(payloadJSON);
  if (httpResponseCode == 200) { 
    Serial.println("[API] Dados de telemetria do minuto enviados com sucesso!");
  } else {
    Serial.print("[Erro] Falha no POST de telemetria. HTTP: "); Serial.println(httpResponseCode);
  }
  http.end();
}

void buscarLimitesDoRecinto() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(graphql_endpoint);
  http.addHeader("Content-Type", "application/json");
  String authHeader = "Bearer " + String(token_autenticacao);
  http.addHeader("Authorization", authHeader.c_str());

  JsonDocument docEnvio;
  char queryBuffer[400];
  
  // Traz a umidade máxima (humidityMax) para usarmos na lógica do exaustor
  snprintf(queryBuffer, sizeof(queryBuffer), 
           "query { enclosureLimits(id: \"%s\") { tempMin tempMax humidityMin humidityMax noiseMax } }", 
           enclosure_id);

  docEnvio["query"] = queryBuffer;
  String requestBody;
  serializeJson(docEnvio, requestBody);

  int httpResponseCode = http.POST(requestBody);
  String response = http.getString();

  if (httpResponseCode == 200) { 
    JsonDocument docResposta; 
    DeserializationError error = deserializeJson(docResposta, response);

    if (!error && !docResposta["errors"]) {
      JsonObject limits = docResposta["data"]["enclosureLimits"];
      
      if (!limits.isNull()) {
        float novoTempMin = limits["tempMin"];
        float novoTempMax = limits["tempMax"];
        float novoUmidMin = limits["humidityMin"];
        float novoUmidMax = limits["humidityMax"];
        float novoNoiseMax = limits["noiseMax"];
        
        bool mudou = false;
        if (tempMax != 0.0) { 
          if (novoTempMin != tempMin || novoTempMax != tempMax || 
              novoUmidMin != umidMin || novoUmidMax != umidMax || 
              novoNoiseMax != noiseMax) {
              mudou = true;
          }
        }

        if (mudou) {
          Serial.println("\n=============================================");
          Serial.println("  [API] NOVOS LIMITES DETECTADOS NO BACKEND  ");
          Serial.println("=============================================");
          if (novoTempMin != tempMin) { Serial.printf("Temp Minima:  %.2f -> %.2f\n", tempMin, novoTempMin); }
          if (novoTempMax != tempMax) { Serial.printf("Temp Maxima:  %.2f -> %.2f\n", tempMax, novoTempMax); }
          if (novoUmidMin != umidMin) { Serial.printf("Umid Minima:  %.2f -> %.2f\n", umidMin, novoUmidMin); }
          if (novoUmidMax != umidMax) { Serial.printf("Umid Maxima:  %.2f -> %.2f\n", umidMax, novoUmidMax); }
          if (novoNoiseMax != noiseMax) { Serial.printf("Ruido Maximo: %.2f -> %.2f\n", noiseMax, novoNoiseMax); }
          Serial.println("=============================================\n");
        } else if (tempMax == 0.0) {
          Serial.println("\n[API] Primeira carga de limites realizada com sucesso.");
        }

        tempMin = novoTempMin;
        tempMax = novoTempMax;
        umidMin = novoUmidMin;
        umidMax = novoUmidMax;
        noiseMax = novoNoiseMax;
      }
    }
  }
  http.end();
}

String getTimestampAtual() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01T00:00:00-03:00"; 
  }
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S-03:00", &timeinfo);
  return String(buffer);
}
