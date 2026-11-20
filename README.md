# Pastillero con aviso de voz y SMS

Prototipo con ESP32-S3, pantalla OLED I²C y reproductor DFPlayer Mini. A una hora configurada reproduce «Doña Belén le toca su medicamento caja verde» cada 30 segundos. Un botón grande detiene la alarma. Si transcurren 10 minutos sin pulsarlo, solicita a un servidor propio que envíe por SMS: «señor su madre no responde a la alarma».

**Es un prototipo, no un dispositivo médico ni un sistema de emergencia confiable.** Pulsar el botón solo indica que alguien lo pulsó; no demuestra que se haya tomado la medicina. Los cortes de luz, Wi‑Fi, internet, el servidor, la tarjeta, el altavoz y el operador pueden impedir los avisos. Mantenga otro recordatorio y pruebe el conjunto antes de utilizarlo con una persona. No coloque medicamentos en contacto con electrónica ni modifique el pastillero comercial sin comprobar cómo está construido.

## Qué hay y qué falta

En las imágenes se aprecia una ESP32-S3 N16R8 con OLED I²C, un módulo rotulado MP3-TF-16P/DFPlayer compatible y un pastillero comercial. Se usa **la ESP32-S3 como única placa programable**: no hace falta conectar el Arduino adicional. El pastillero de la foto es un organizador independiente; este proyecto no controla sus compartimentos ni detecta si se abren.

Además se necesita:

- Un pulsador momentáneo normalmente abierto, de preferencia grande y accesible.
- Un altavoz pasivo compatible con la salida del módulo (por ejemplo 4–8 Ω y hasta 3 W; confirme el módulo adquirido), microSD FAT32 de hasta 32 GB, cables y protoboard.
- Una resistencia de 1 kΩ para TX de la ESP32 al RX del reproductor. Alimentación USB estable de 5 V capaz de alimentar ambos módulos; una fuente insuficiente puede provocar reinicios.
- Red Wi‑Fi de 2,4 GHz con internet, un servidor propio siempre activo con HTTPS público y almacenamiento persistente, y cuenta de Twilio con remitente SMS habilitado para el destino y saldo. Los SMS y el alojamiento pueden tener costo; en cuentas de prueba puede ser necesario verificar el número de destino.

## Conexiones (desconectar la alimentación primero)

Las cifras son **GPIO**, no la posición física del pin. Verifique las etiquetas de su placa y del módulo: las variantes pueden diferir. El ESP32-S3 trabaja con lógica de 3,3 V; nunca aplique 5 V a sus GPIO.

| Dispositivo | Pin | Conexión ESP32-S3 |
|---|---|---|
| OLED SSD1306 I²C | GND | GND |
| OLED | VCC | 3V3 (verifique que su módulo admita 3,3 V) |
| OLED | SDA | GPIO 8 |
| OLED | SCL | GPIO 9 |
| DFPlayer | VCC | 5V/VIN de la placa, con alimentación USB estable |
| DFPlayer | GND | GND común |
| DFPlayer | RX | GPIO 18 **a través de 1 kΩ** |
| DFPlayer | TX | Sin conectar |
| Altavoz pasivo | terminales | SPK1 y SPK2 del DFPlayer, uno a cada terminal |
| Pulsador | un terminal | GPIO 4 |
| Pulsador | otro terminal | GND |

El programa usa `INPUT_PULLUP`: sin pulsar lee HIGH; al pulsar, LOW. En el reproductor solo se utiliza la comunicación desde la ESP32; no se conecta TX del reproductor a la ESP32, evitando una posible señal de tensión incompatible. **No conecte SPK1 ni SPK2 a GND ni a la ESP32**, son salidas amplificadas. No alimente simultáneamente la placa mediante distintas fuentes sin verificar su circuito. Use los nombres impresos en cada módulo, no el orden de los pines de una foto.

```text
ESP32-S3                     OLED
  3V3 ---------------------- VCC
  GND ---------------------- GND
  GPIO8 -------------------- SDA
  GPIO9 -------------------- SCL

ESP32-S3                     DFPlayer              ALTAVOZ
  5V/VIN ------------------- VCC
  GND ---------------------- GND
  GPIO18 ----[1 kΩ]--------- RX
                             SPK1 ------------------ (+)
                             SPK2 ------------------ (-)
  GPIO4 ----[pulsador]------ GND
```

La pantalla se supone SSD1306 de 128×64, dirección `0x3C`; algunas pantallas usan `0x3D` u otro controlador. Compruebe el modelo y modifique `OLED_ADDRESS` si hace falta. La placa de la imagen indica N16R8: se evitan GPIO 33–37 por la PSRAM octal, los pines de arranque 0/3/45/46 y 19/20 de USB. El módulo de la foto parece compatible DFPlayer, pero los clones pueden variar. [Documentación del DFPlayer](https://wiki.dfrobot.com/dfr0299); [restricciones de GPIO de Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html).

## Preparar el audio

1. Formatee la microSD como FAT32, preferiblemente de 32 GB o menos.
2. Copie la carpeta `audio/mp3` a la **raíz** de la tarjeta: debe quedar `/mp3/0001.mp3`.
3. Inserte la tarjeta con la alimentación apagada. El archivo incluido usa voz sintética y dice el aviso solicitado. Puede reemplazarlo por una grabación MP3 propia con el mismo nombre.
4. Ajuste `player.volume(25)` en el código: el rango habitual es 0–30. Escuche el volumen real con la caja terminada.

## Preparar el servidor de SMS

La ESP32 no contiene las credenciales de Twilio. El servidor Python utiliza la [API de mensajes de Twilio](https://www.twilio.com/docs/messaging/api/message-resource). Requiere un equipo permanentemente encendido, dominio HTTPS y **proxy inverso TLS**; `server.py` solo escucha `127.0.0.1:8080` y no debe exponerse directamente a internet. No sirve ejecutar `localhost` en la computadora si la ESP32 debe acceder a ella por internet. Esta guía supone Linux con systemd, Python 3 y Caddy instalados, un dominio que apunta al servidor y puertos 80 y 443 accesibles.

1. Cree una cuenta Twilio; obtenga Account SID, Auth Token y un número remitente compatible con SMS hacia el país del destino. Revise restricciones regionales y de cuenta. Use formato internacional E.164, por ejemplo `+52...` para México. Compruebe los números reales en la consola de Twilio.
2. Copie `servidor/server.py` a `/opt/pastillero/server.py` y cree un usuario de servicio y una carpeta persistente:

```sh
sudo useradd --system --home /var/lib/pastillero --create-home --shell /usr/sbin/nologin pastillero
sudo install -d -o pastillero -g pastillero /var/lib/pastillero /opt/pastillero
sudo install -o pastillero -g pastillero -m 644 server.py /opt/pastillero/server.py
openssl rand -hex 32
```

Ejecute el último comando para generar el token. Desde el directorio local `servidor`, adapte la ruta del archivo al copiarlo al servidor. Cree `/etc/pastillero.env` como root basándose en `servidor/env.example`, con el token generado y las credenciales reales. No incluya comillas ni espacios alrededor del `=`. Proteja el archivo: `sudo chmod 600 /etc/pastillero.env`. Nunca suba este archivo ni `config.h` a Git. La ruta `DB_PATH` debe estar en almacenamiento persistente y ser escribible por el usuario `pastillero`.

3. Cree `/etc/systemd/system/pastillero.service`:

```ini
[Unit]
Description=Servidor de SMS del pastillero
After=network-online.target
Wants=network-online.target
[Service]
User=pastillero
Group=pastillero
EnvironmentFile=/etc/pastillero.env
WorkingDirectory=/var/lib/pastillero
ExecStart=/usr/bin/python3 /opt/pastillero/server.py
Restart=on-failure
[Install]
WantedBy=multi-user.target
```

Ejecute `sudo systemctl daemon-reload`, `sudo systemctl enable --now pastillero` y `sudo systemctl status pastillero`. Diagnóstico: `sudo journalctl -u pastillero -f`.

4. En la configuración de Caddy agregue, sustituyendo el dominio:

```caddyfile
pastillero.ejemplo.com {
    reverse_proxy 127.0.0.1:8080
}
```

Valide y recargue Caddy según su instalación. Configure el cortafuegos para permitir HTTPS y proteger SSH; no abra el puerto 8080. La autoridad raíz del certificado público de Caddy puede consultarse en el navegador o con `openssl s_client -connect pastillero.ejemplo.com:443 -servername pastillero.ejemplo.com -showcerts`; descargue el **certificado raíz PEM** desde la autoridad emisora oficial y cópielo completo, con las líneas BEGIN/END, en `SERVER_ROOT_CA` de `config.h`. Puede cambiar cuando renueve o cambie el emisor; manténgalo actualizado. Nunca desactive la verificación TLS.

5. Desde otra máquina compruebe autenticación y envío real (esto enviará un SMS y puede generar costo):

```sh
curl -i https://pastillero.ejemplo.com/alert \
  -H 'Authorization: Bearer TOKEN_GENERADO' \
  -H 'Content-Type: application/json' \
  -d '{"event_id":"20261120-1234"}'
```

Un HTTP 200 `accepted` indica aceptación por Twilio, **no entrega**; compruebe la recepción y el estado en Twilio. 202 `unknown` indica que no se sabe si el proveedor recibió la solicitud: revise Twilio y contacte a la persona por otro medio. Para no duplicar mensajes, el servidor registra el evento antes de enviarlo y no reintenta ese evento después de una respuesta incierta o un fallo del proveedor. 401 indica token incorrecto. No reutilice para la prueba el identificador del próximo recordatorio: formato `AAAAMMDD-HHMM`.

## Instalar y cargar el programa

1. Instale Arduino IDE 2 y el paquete **esp32 de Espressif Systems** mediante el gestor de placas. Instale desde el gestor de bibliotecas: **Adafruit GFX Library**, **Adafruit SSD1306** y **DFRobotDFPlayerMini** (y las dependencias que solicite Arduino IDE).
2. Copie `firmware/pastillero/config.example.h` como `firmware/pastillero/config.h`. Complete Wi‑Fi, URL HTTPS sin barra final, token idéntico al servidor y certificado raíz. Cambie `ALARM_HOUR` y `ALARM_MINUTE`. El valor predeterminado `CST6` corresponde a Mérida, Yucatán; si está en otra zona cambie la regla POSIX. La hora se obtiene por NTP a través de internet.
3. Abra `firmware/pastillero/pastillero.ino` en Arduino IDE. Seleccione **ESP32S3 Dev Module**, el puerto correspondiente y los parámetros de memoria que correspondan a su placa N16R8; si no aparece el puerto, revise cable USB de datos y controlador USB. Compile y cargue. Abra el monitor serie a 115200 baudios.
4. Para una prueba rápida, programe una hora 2–3 minutos en el futuro. Para probar la escalación puede reducir temporalmente `RESPONSE_MS` a `60UL * 1000`; después restablezca los 10 minutos y cargue de nuevo. La hora diaria se modifica en `config.h` y requiere volver a cargar el programa.

## Pruebas de aceptación

- Confirme que se ve la hora correcta y Wi‑Fi conectado; compruebe el audio y el volumen. Si falta la hora, revise Wi‑Fi, internet y NTP.
- En una alarma, pulse el botón antes del plazo: debe detenerse la voz y **no** enviarse SMS. Realice otra prueba dejando vencer el plazo y confirme recepción en el teléfono.
- Verifique que el audio se repite, que pulsar el botón después del envío lo detiene y que el servidor no envía duplicados ante una repetición del mismo `event_id`.
- Desconecte el Wi‑Fi durante una prueba y reconéctelo: mientras la alarma siga activa, el dispositivo reintentará hablar con el servidor cada minuto. Compruebe además las fallas de alimentación y las condiciones reales de cobertura. Revise la pantalla y los registros cuando algo falle.

## Comportamiento y límites

- Solo una alarma diaria y una caja; el botón apaga la alarma, no verifica ingestión. Se escucha hasta pulsar incluso después del SMS.
- Sin hora sincronizada no se dispara la alarma. Si recupera la hora dentro del minuto programado se dispara; fuera de ese minuto no recupera la dosis omitida. La hora se mantiene con el reloj interno mientras haya alimentación tras sincronizar NTP, pero deriva.
- El estado de la alarma y la hora no persisten en un corte o reinicio. Un reinicio puede perder una alarma activa o repetirla si ocurre en el minuto programado. La deduplicación del servidor reduce SMS duplicados siempre que se conserve su base de datos, pero una alarma perdida no envía aviso. Considere RTC con batería, alimentación de respaldo y supervisión externa antes de cualquier uso real.
- Tras 10 minutos se intenta enviar mientras la alarma siga activa; si se pulsa antes de que el servidor acepte el aviso, cesan los intentos. El HTTPS tiene tiempos de espera de varios segundos, por lo que la respuesta al botón puede retrasarse durante una solicitud. Si el servidor responde `unknown`, no se reintenta automáticamente. El servidor registra identificadores de fecha y hora, no registra el número ni el texto; proteja servidor y base de datos.
- Si el DFPlayer o la OLED falla, el programa puede continuar sin esa función; el inicializado sin respuesta del reproductor no confirma que exista tarjeta ni que el altavoz funcione. No hay detección de fallas de audio ni aviso automático por corte total. Compruebe físicamente cada elemento. El sistema no verifica la entrega del SMS y no manda WhatsApp ni realiza llamadas.
