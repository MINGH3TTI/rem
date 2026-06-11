#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include "DHT.h"

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef GRAPHQL_ENDPOINT
#define GRAPHQL_ENDPOINT ""
#endif

#ifndef TOKEN_AUTENTICACAO
#define TOKEN_AUTENTICACAO ""
#endif

#ifndef ENCLOSURE_ID
#define ENCLOSURE_ID ""
#endif

#ifndef NTP_SERVER
#define NTP_SERVER ""
#endif

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* graphql_endpoint = GRAPHQL_ENDPOINT;
const char* token_autenticacao = TOKEN_AUTENTICACAO;
const char* enclosure_id = ENCLOSURE_ID; 
const char* ntpServer = NTP_SERVER;
const long  gmtOffset_sec = -10800; 
const int   daylightOffset_sec = 0;
#define DHTPIN 4     
#define DHTTYPE DHT22  
DHT dht(DHTPIN, DHTTYPE);
#define PINO_LDR 34  
#define PINO_SOM 35  
#define PINO_VENTILADOR  25 
#define PINO_AQUECEDOR   14 
#define PINO_NEBULIZADOR 27 
#define PINO_EXAUSTOR    26 
float tempAtual = 0.0;
float umidAtual = 0.0;
float luzAtual = 0.0; 
float somAtual = 0.0;
unsigned long somaLuz = 0; 
int contagemLuz = 0;       
float picoSom60s = 0.0;    
int sinalMax = 0;          
int sinalMin = 4095;       
float tempMax = 0.0, tempMin = 0.0;
float umidMax = 0.0, umidMin = 0.0; 
float noiseMax = 0.0;
bool estadoFan = false;
bool estadoHeater = false;
bool estadoNebulizer = false;
bool estadoExaustor = false; 
unsigned long tempoAnteriorSensores = 0;
unsigned long tempoAnteriorSom = 0;
unsigned long tempoAnteriorEnvio = 0;
unsigned long tempoAnteriorLimites = 0;
const long intervaloSensores = 2000;  
const long intervaloSom = 5000;       
const long intervaloEnvio = 60000;    
const long intervaloLimites = 30000;  
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
  int leituraSom = analogRead(PINO_SOM);
  if (leituraSom > sinalMax) { sinalMax = leituraSom; }
  if (leituraSom < sinalMin) { sinalMin = leituraSom; }
  if (tempoAtual - tempoAnteriorSom >= intervaloSom) {
    tempoAnteriorSom = tempoAtual;
    int amplitude = sinalMax - sinalMin;
    if (amplitude < 30) { 
      amplitude = 0; 
    }
    somAtual = map(amplitude, 0, 1500, 0, 100);
    somAtual = constrain(somAtual, 0, 100);
    if (somAtual > picoSom60s) {
      picoSom60s = somAtual;
    }
    Serial.print("[Som 5s] Amplitude: "); Serial.print(amplitude); 
    Serial.print(" -> Ruido: "); Serial.print(somAtual); Serial.println(" dB");
    sinalMax = 0;
    sinalMin = 4095; 
  }
  if (tempoAtual - tempoAnteriorSensores >= intervaloSensores) {
    tempoAnteriorSensores = tempoAtual;
    lerSensores();
    controlarAtuadores(); 
  }
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
  if (tempoAtual - tempoAnteriorLimites >= intervaloLimites) {
    tempoAnteriorLimites = tempoAtual;
    buscarLimitesDoRecinto();
  }
}
void lerSensores() {
  float umidadeLida = dht.readHumidity();
  float temperaturaLida = dht.readTemperature();
  if (!isnan(umidadeLida) && !isnan(temperaturaLida)) {
    tempAtual = temperaturaLida;
    umidAtual = umidadeLida;
  }
  int leituraBrutaLuz = analogRead(PINO_LDR);
  float luxReal = 0.0;
  float tensao = leituraBrutaLuz * (3.3 / 4095.0);
  if (tensao > 0.0) {
    float resistenciaLDR = 10000.0 * (3.3 - tensao) / tensao;
    luxReal = pow((50.0 * 1000.0 * pow(10, 0.7)) / resistenciaLDR, (1.0 / 0.7));
  }
  luzAtual = luxReal; 
  somaLuz += luzAtual; 
  contagemLuz ++;
  Serial.print("[Sensores 2s] Temp: "); Serial.print(tempAtual); 
  Serial.print("C | Umid: "); Serial.print(umidAtual); 
  Serial.print("% | Luz: "); Serial.print(luzAtual); Serial.println(" Lux");
}
void controlarAtuadores() {
  if (tempMax == 0 && tempMin == 0) return; 
  bool precisaFan = (tempAtual > tempMax);
  bool precisaHeater = (tempAtual < tempMin);
  atualizarEstadoAtuador("fan", PINO_VENTILADOR, precisaFan, estadoFan);
  atualizarEstadoAtuador("heater", PINO_AQUECEDOR, precisaHeater, estadoHeater);
  bool precisaNebulizer = (umidAtual < umidMin); 
  bool precisaExaustor = (umidAtual > umidMax);   
  atualizarEstadoAtuador("nebulizer", PINO_NEBULIZADOR, precisaNebulizer, estadoNebulizer);
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
  query += "luminosity: " + String(luzEnvio, 2) + ", "; 
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
