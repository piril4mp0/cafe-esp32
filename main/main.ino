#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <cJSON.h>

// Preencha com os dados da rede e o IP do computador que roda o servidor.
constexpr char WIFI_SSID[] = "Murilo's Samsung Galaxy A57 5G";
constexpr char WIFI_PASSWORD[] = "euamoaDio";
// localhost no ESP32 aponta para a própria placa, não para o computador.
constexpr char AUTH_URL[] = "http://10.116.16.150:8080/cafes/autorizar";

// RC522 conectado à LILYGO T3 V1.6.1 (alimentação em 3,3 V).
constexpr uint8_t RFID_RST = 23;
constexpr uint8_t RFID_MISO = 35;
constexpr uint8_t RFID_MOSI = 15;
constexpr uint8_t RFID_SCK = 14;
constexpr uint8_t RFID_SS = 13;
constexpr uint8_t BUZZER_PIN = 0;
constexpr unsigned int BUZZER_FREQUENCY_HZ = 6000;
constexpr unsigned long BUZZER_DURATION_MS = 1000;

MFRC522 rfid(RFID_SS, RFID_RST);

bool autorizarCartao(const String& idCartao) {
  // valida se tem conexao wifi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem Wi-Fi. Cartao nao enviado.");
    return false;
  }

  //inicializa client wifi e http
  WiFiClient client;
  HTTPClient http;

  // tenta se conectar ao servidor, com timeout de 3s e timeout de resposta de 5s
  http.setConnectTimeout(3000);
  http.setTimeout(5000);
  if (!http.begin(client, AUTH_URL)) {
    Serial.println("Nao foi possivel iniciar a requisicao HTTP.");
    return false;
  }

  // monta payload e envia requisicao POST
  http.addHeader("Content-Type", "application/json");
  const String payload = String("{\"id_cartao\":\"") + idCartao + "\"}";
  const int status = http.POST(payload);
  if (status > 0) {
    Serial.printf("POST /cafes/autorizar: HTTP %d\n", status);
  } else {
    Serial.printf("Falha no POST: %s\n", HTTPClient::errorToString(status).c_str());
  }
  if (status < 200 || status >= 300) {
    http.end();
    return false;
  }

  const String resposta = http.getString();
  http.end();
  cJSON* json = cJSON_ParseWithOpts(resposta.c_str(), nullptr, true);
  const cJSON* campo = cJSON_GetObjectItemCaseSensitive(json, "autorizado");
  const bool autorizado = cJSON_IsTrue(campo);
  cJSON_Delete(json);
  return autorizado;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // SPI configura SCK/MOSI como saídas e MISO como entrada.
  // GPIO 35 é somente entrada e não possui pull-up interno.
  pinMode(RFID_SS, OUTPUT);
  digitalWrite(RFID_SS, HIGH);
  SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_SS);
  rfid.PCD_Init();  // Configura também o pino de reset.

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("RC522 inicializado. Conectando ao Wi-Fi...");
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    delay(20);
    return;
  }

  String idCartao;
  idCartao.reserve(rfid.uid.size * 3);
  for (byte i = 0; i < rfid.uid.size; ++i) {
    if (i > 0) {
      idCartao += ':';
    }
    char hexByte[3];
    snprintf(hexByte, sizeof(hexByte), "%02X", rfid.uid.uidByte[i]);
    idCartao += hexByte;
  }

  // Encerra a comunicação sem gravar dados no cartão.
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  if (autorizarCartao(idCartao)) {
    tone(BUZZER_PIN, BUZZER_FREQUENCY_HZ, BUZZER_DURATION_MS);
  }
}
