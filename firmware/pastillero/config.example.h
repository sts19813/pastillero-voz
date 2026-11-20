#pragma once
// Copia este archivo a config.h; nunca publiques config.h.
constexpr char WIFI_SSID[] = "TU_RED";
constexpr char WIFI_PASSWORD[] = "TU_CLAVE";
// Servicio propio publicado con HTTPS; sin barra al final.
constexpr char SERVER_URL[] = "https://pastillero.ejemplo.com";
constexpr char DEVICE_TOKEN[] = "TOKEN_LARGO_ALEATORIO";
// Certificado PEM de la autoridad raíz del certificado HTTPS del servidor.
constexpr char SERVER_ROOT_CA[] = R"EOF_CA(
-----BEGIN CERTIFICATE-----
PEGA_AQUI_EL_CERTIFICADO_RAIZ_COMPLETO
-----END CERTIFICATE-----
)EOF_CA";
// Zona POSIX: America/Merida usa UTC-6 sin horario de verano desde 2022.
constexpr char TIMEZONE[] = "CST6";
constexpr int ALARM_HOUR = 8;
constexpr int ALARM_MINUTE = 0;
constexpr unsigned long RESPONSE_MS = 10UL * 60 * 1000;
constexpr unsigned long REPEAT_MS = 30UL * 1000;
