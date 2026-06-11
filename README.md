# Projeto REM - Sistema de Monitoramento e Controle Ambiental

Este repositorio contem o firmware do Projeto REM, um sistema de monitoramento e controle automatizado de ambientes desenvolvido para ESP32.

O sistema coleta temperatura, umidade, luminosidade e ruido, controla atuadores em tempo real e sincroniza os dados com uma API GraphQL.

## Funcionalidades

- Monitoramento de temperatura e umidade com DHT22.
- Leitura de luminosidade com LDR.
- Leitura de ruido por sensor de som analogico.
- Controle automatico de ventilador, aquecedor, nebulizador e exaustor.
- Envio de telemetria para backend GraphQL.
- Busca dinamica de limites ambientais pela API.
- Sincronizacao de horario via NTP.

## Hardware

- ESP32
- DHT22
- LDR
- Sensor de som analogico
- Modulo rele
- Ventilador
- Aquecedor
- Nebulizador
- Exaustor

## Bibliotecas

- WiFi.h
- HTTPClient.h
- ArduinoJson
- DHT sensor library

## Configuracao

As credenciais e configuracoes sensiveis nao ficam no `rem.ino`.

Crie um arquivo `.env` na raiz do projeto com:

```env
WIFI_SSID=nome_da_rede
WIFI_PASSWORD=senha_da_rede
GRAPHQL_ENDPOINT=https://sua-api/graphql
TOKEN_AUTENTICACAO=seu_token_jwt
ENCLOSURE_ID=id_do_recinto
NTP_SERVER=pool.ntp.org
```

O arquivo `.env` esta no `.gitignore` e nao deve ser versionado.

## Variaveis de build

O firmware espera receber as configuracoes como macros de compilacao:

```cpp
WIFI_SSID
WIFI_PASSWORD
GRAPHQL_ENDPOINT
TOKEN_AUTENTICACAO
ENCLOSURE_ID
NTP_SERVER
```

Ao usar PlatformIO, essas variaveis podem ser passadas por `build_flags`, carregadas a partir do `.env` por script ou definidas no ambiente de build.

Exemplo do formato esperado pelo compilador:

```ini
build_flags =
  -DWIFI_SSID=\"nome_da_rede\"
  -DWIFI_PASSWORD=\"senha_da_rede\"
  -DGRAPHQL_ENDPOINT=\"https://sua-api/graphql\"
  -DTOKEN_AUTENTICACAO=\"seu_token_jwt\"
  -DENCLOSURE_ID=\"id_do_recinto\"
  -DNTP_SERVER=\"pool.ntp.org\"
```

Na Arduino IDE, defina essas macros nas opcoes de compilacao ou adapte o fluxo de build para carregar o `.env` antes de compilar.

## Estrutura

```text
.
|-- rem.ino
|-- README.md
|-- .env
`-- .gitignore
```

## Seguranca

Nao coloque SSID, senha, token ou endpoint privado diretamente no codigo-fonte.

Caso alguma credencial real ja tenha sido versionada, gere novas credenciais e invalide as antigas.
