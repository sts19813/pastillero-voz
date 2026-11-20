#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DFRobotDFPlayerMini.h>
#include <time.h>
#include "config.h"

// Pines elegidos para el ESP32-S3 de la imagen; comprobar etiquetas de la placa.
constexpr int SDA_PIN = 8, SCL_PIN = 9, BUTTON_PIN = 4;
constexpr int PLAYER_RX = 17, PLAYER_TX = 18; // RX del ESP, TX del ESP
constexpr uint8_t OLED_ADDRESS = 0x3C;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
HardwareSerial playerSerial(1);
DFRobotDFPlayerMini player;
bool oledOk = false, playerOk = false, active = false, notified = false;
bool rawButton = HIGH, stableButton = HIGH;
unsigned long changedAt = 0, alarmStarted = 0, lastPlay = 0, lastWifi = 0;
unsigned long lastDisplay = 0, lastSend = 0;
int lastAlarmDay = -1;
String eventId, statusText = "Esperando hora";

void screen() {
  if (!oledOk) return;
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.println("PASTILLERO");
  struct tm t;
  if (getLocalTime(&t, 10)) { char s[20]; strftime(s, sizeof(s), "%d/%m %H:%M:%S", &t); display.println(s); }
  else display.println("Sin hora NTP");
  display.println(WiFi.status() == WL_CONNECTED ? "WiFi conectado" : "Sin WiFi");
  display.println(statusText);
  if (active) display.println("Pulse para apagar");
  display.display();
}
void playReminder() {
  if (playerOk) { player.playMp3Folder(1); lastPlay = millis(); }
}
void startAlarm(const tm &t) {
  // Se conserva el evento durante toda la alarma, incluso en los reintentos.
  char id[32]; strftime(id, sizeof(id), "%Y%m%d-%H%M", &t); eventId = String(id);
  active = true; notified = false; alarmStarted = millis(); lastSend = 0;
  statusText = "Medicamento verde";
  Serial.printf("Alarma %s iniciada\n", id); playReminder(); screen();
}
void sendAlert() {
  lastSend = millis();
  if (WiFi.status() != WL_CONNECTED) { statusText = "SMS: sin WiFi"; return; }
  WiFiClientSecure client; client.setCACert(SERVER_ROOT_CA); client.setTimeout(5);
  HTTPClient http; http.setConnectTimeout(5000); http.setTimeout(5000);
  if (!http.begin(client, String(SERVER_URL) + "/alert")) { statusText = "Error HTTPS"; return; }
  http.addHeader("Authorization", String("Bearer ") + DEVICE_TOKEN);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(String("{\"event_id\":\"") + eventId + "\"}");
  String response = http.getString(); http.end();
  Serial.printf("Servidor HTTP %d: %s\n", code, response.c_str());
  if (code == 200) { notified = true; statusText = "SMS aceptado"; }
  // 202 significa resultado incierto; no repetir para evitar mensajes duplicados.
  else if (code == 202) { notified = true; statusText = "SMS incierto"; }
  else { statusText = "Error SMS; reintento"; }
}
void setup() {
  Serial.begin(115200); pinMode(BUTTON_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  playerSerial.begin(9600, SERIAL_8N1, PLAYER_RX, PLAYER_TX);
  playerOk = player.begin(playerSerial, false, true); // sin esperar respuestas UART
  if (playerOk) { player.volume(25); delay(200); }
  else Serial.println("No se pudo iniciar DFPlayer");
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  configTzTime(TIMEZONE, "pool.ntp.org", "time.nist.gov"); screen();
}
void loop() {
  unsigned long now = millis();
  if (WiFi.status() != WL_CONNECTED && now - lastWifi >= 30000) {
    lastWifi = now; WiFi.disconnect(); WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != rawButton) { rawButton = reading; changedAt = now; }
  if (now - changedAt >= 50 && stableButton != rawButton) {
    stableButton = rawButton;
    if (stableButton == LOW && active) {
      active = false; if (playerOk) player.stop();
      statusText = notified ? "Confirmado; revisar SMS" : "Alarma confirmada";
      Serial.println("Boton pulsado: alarma apagada"); screen();
    }
  }
  struct tm t;
  if (getLocalTime(&t, 10)) {
    int dayKey = (t.tm_year + 1900) * 1000 + t.tm_yday;
    if (!active && t.tm_hour == ALARM_HOUR && t.tm_min == ALARM_MINUTE && dayKey != lastAlarmDay) {
      lastAlarmDay = dayKey; startAlarm(t);
    }
  }
  if (active) {
    if (now - lastPlay >= REPEAT_MS) playReminder();
    if (!notified && now - alarmStarted >= RESPONSE_MS && (lastSend == 0 || now - lastSend >= 60000)) sendAlert();
  }
  if (now - lastDisplay >= 1000) { lastDisplay = now; screen(); }
  delay(10);
}
