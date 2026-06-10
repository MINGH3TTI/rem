# Projeto REM - Sistema de Monitoramento e Controle Ambiental

Este repositório contém o código-fonte do firmware para o **Projeto REM**, um sistema inteligente de monitoramento e controle automatizado de ambientes. Este projeto foi desenvolvido como nosso **Trabalho de Graduação (TG)**.

O sistema utiliza um ESP32 para coletar dados sensoriais (temperatura, umidade, luminosidade e ruído) e controlar atuadores (ventilador, aquecedor, nebulizador e exaustor) em tempo real, mantendo o ambiente dentro de limites pré-configurados através de uma API GraphQL.

## 🚀 Funcionalidades

- **Monitoramento em Tempo Real:** Leitura de temperatura e umidade (DHT22), luminosidade (LDR) e nível de ruído.
- **Controle Automatizado:** Ativação de relés para controle de temperatura e umidade com base em limites dinâmicos.
- **Integração com Nuvem:** Comunicação bidirecional com backend via GraphQL (Mutations para envio de telemetria e estado de atuadores, Queries para busca de limites).
- **Sincronização de Tempo:** Uso de protocolo NTP para timestamping preciso das medições.
- **Lógica de Som Avançada:** Cálculo de amplitude sonora para detecção de picos de ruído em decibéis aproximados.

## 🛠️ Hardware Utilizado

- **Microcontrolador:** ESP32
- **Sensores:**
  - DHT22 (Temperatura e Umidade)
  - LDR (Luminosidade)
  - Sensor de Som Analógico
- **Atuadores (Módulo Relé):**
  - Ventilador (Resfriamento)
  - Aquecedor (Aquecimento)
  - Nebulizador (Umidificação)
  - Exaustor (Desumidificação)

## 💻 Tecnologias e Bibliotecas

- [Arduino IDE](https://www.arduino.cc/en/software) / [PlatformIO](https://platformio.org/)
- [WiFi.h](https://github.com/espressif/arduino-esp32) & [HTTPClient.h](https://github.com/espressif/arduino-esp32)
- [ArduinoJson](https://arduinojson.org/) (Processamento de payloads JSON/GraphQL)
- [DHT sensor library](https://github.com/adafruit/DHT-sensor-library)

## 📋 Configuração

Para compilar o projeto, certifique-se de configurar as seguintes variáveis no arquivo `rem.ino`:

```cpp
const char* ssid = "NOME_DA_REDE";
const char* password = "SENHA_DA_REDE";
const char* graphql_endpoint = "URL_DA_API";
const char* token_autenticacao = "SEU_TOKEN_JWT";
const char* enclosure_id = "ID_DO_RECINTO";
```

## 🎓 Sobre o Projeto

Este projeto é o resultado do nosso esforço e dedicação como **Trabalho de Graduação (TG)**, visando aplicar conhecimentos de Internet das Coisas (IoT), sistemas embarcados e desenvolvimento de software fullstack para resolver problemas reais de automação ambiental.

---

![Rem](https://media.tenor.com/a1V-3ctKdQ0AAAAM/rem-rezero.gif)
