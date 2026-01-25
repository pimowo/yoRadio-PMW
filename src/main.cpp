// v0.9.720 Módosítva! "clock_tts"
#include "Arduino.h"
#include "core/options.h"
#include "core/config.h"
#include "pluginsManager/pluginsManager.h"
#include "core/telnet.h"
#include "core/player.h"
#include "core/display.h"
#include "core/bluetooth_uart.h"

#include "core/network.h"
#include "core/netserver.h"
#include "core/controls.h"
#include "core/config_ui.h"
#include "core/tda_config_ui.h"
// #include "core/mqtt.h"
#include "core/optionschecker.h"
#include "core/timekeeper.h"
#include "core/audiohandlers.h" //"audio_change"

extern void pollInactivity();
#if USE_OTA
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
#include <NetworkUdp.h>
#else
#include <WiFiUdp.h>
#endif
#include <ArduinoOTA.h>
#endif

#if DSP_HSPI || TS_HSPI || VS_HSPI
SPIClass SPI2(HSPI);
#endif

// UART for Blutadata
HardwareSerial btSerial(2); // UART2, pins 16 RX, 17 TX

extern __attribute__((weak)) void yoradio_on_setup();

// Bluetooth UART parser
void parseBTMessage(String msg)
{
  Serial.print("BT MSG: ");
  Serial.println(msg); // Debug: pokaż odebrane wiadomości
  if (msg.startsWith("BT:"))
  {
    int colon1 = msg.indexOf(':', 3);
    String cmd;
    String value;
    if (colon1 == -1)
    {
      cmd = msg.substring(3);
    }
    else
    {
      cmd = msg.substring(3, colon1);
      value = msg.substring(colon1 + 1);
      value.trim();
    }

    if (cmd == "CONNECTED")
    {
      if (btMetaMutex)
        xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
      btMeta.connected = true;
      memset(btMeta.artist, 0, sizeof(btMeta.artist));
      memset(btMeta.title, 0, sizeof(btMeta.title));
      if (btMetaMutex)
        xSemaphoreGive(btMetaMutex);
      Serial.println("BT: Connected set to true"); // Debug
      // Update display if in BT mode
      if (config.getMode() == PM_BLUETOOTH)
      {
        display.putRequest(NEWTITLE);
      }
    }
    else if (cmd == "DISCONNECTED")
    {
      if (btMetaMutex)
        xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
      btMeta.connected = false;
      memset(btMeta.deviceName, 0, sizeof(btMeta.deviceName));
      memset(btMeta.deviceMAC, 0, sizeof(btMeta.deviceMAC));
      memset(btMeta.artist, 0, sizeof(btMeta.artist));
      memset(btMeta.title, 0, sizeof(btMeta.title));
      if (btMetaMutex)
        xSemaphoreGive(btMetaMutex);
      if (config.getMode() == PM_BLUETOOTH)
      {
        display.putRequest(NEWTITLE);
      }
    }
    else if (cmd == "NAME")
    {
      if (btMetaMutex)
        xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
      if (!btMeta.connected)
      {
        btMeta.connected = true;
        Serial.println("BT: Connected set to true from NAME"); // Debug
      }
      strlcpy(btMeta.deviceName, value.c_str(), sizeof(btMeta.deviceName));
      if (btMetaMutex)
        xSemaphoreGive(btMetaMutex);
      if (config.getMode() == PM_BLUETOOTH)
      {
        display.putRequest(NEWTITLE);
      }
    }
    else if (cmd == "MAC")
    {
      if (btMetaMutex)
        xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
      strlcpy(btMeta.deviceMAC, value.c_str(), sizeof(btMeta.deviceMAC));
      if (btMetaMutex)
        xSemaphoreGive(btMetaMutex);
    }
    else if (cmd == "ARTIST")
    {
      if (btMetaMutex)
        xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
      if (!btMeta.connected)
      {
        btMeta.connected = true;
        Serial.println("BT: Connected set to true from ARTIST"); // Debug
      }
      strlcpy(btMeta.artist, value.c_str(), sizeof(btMeta.artist));
      if (btMetaMutex)
        xSemaphoreGive(btMetaMutex);
      if (config.getMode() == PM_BLUETOOTH)
      {
        display.putRequest(NEWTITLE);
      }
    }
    else if (cmd == "TITLE")
    {
      if (btMetaMutex)
        xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
      if (!btMeta.connected)
      {
        btMeta.connected = true;
        Serial.println("BT: Connected set to true from TITLE"); // Debug
      }
      strlcpy(btMeta.title, value.c_str(), sizeof(btMeta.title));
      // copy local copy of artist and title to build meta safely
      char localArtist[sizeof(btMeta.artist)];
      char localTitle[sizeof(btMeta.title)];
      strlcpy(localArtist, btMeta.artist, sizeof(localArtist));
      strlcpy(localTitle, btMeta.title, sizeof(localTitle));
      if (btMetaMutex)
        xSemaphoreGive(btMetaMutex);
      if (config.getMode() == PM_BLUETOOTH)
      {
        display.putRequest(NEWTITLE);
        char meta[512]; // Increase buffer size to prevent truncation
        snprintf(meta, sizeof(meta), "%s - %s", localArtist, localTitle);
        strlcpy(config.station.title, meta, sizeof(config.station.title));
        netserver.requestOnChange(TITLE, 0);
        telnet.printf("##CLI.META#: %s\r\n", meta);
      }
    }
    else if (cmd == "PLAYING")
    {
      if (btMetaMutex)
        xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
      btMeta.playing = true;
      if (btMetaMutex)
        xSemaphoreGive(btMetaMutex);
    }
    else if (cmd == "STOPPED")
    {
      if (btMetaMutex)
        xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
      btMeta.playing = false;
      if (btMetaMutex)
        xSemaphoreGive(btMetaMutex);
      if (config.getMode() == PM_BLUETOOTH)
      {
        memset(config.station.title, 0, sizeof(config.station.title));
        netserver.requestOnChange(TITLE, 0);
        telnet.printf("##CLI.META#: \r\n");
      }
    }
  }
}

#if USE_OTA
void setupOTA()
{
  if (strlen(config.store.mdnsname) > 0)
    ArduinoOTA.setHostname(config.store.mdnsname);
#ifdef OTA_PASS
  ArduinoOTA.setPassword(OTA_PASS);
#endif
  ArduinoOTA
      .onStart([]()
               {
      player.sendCommand({PR_STOP, 0});
      display.putRequest(NEWMODE, UPDATING);
      telnet.printf("Start OTA updating %s\r\n", ArduinoOTA.getCommand() == U_FLASH?"firmware":"filesystem"); })
      .onEnd([]()
             {
      telnet.printf("\nEnd OTA update, Rebooting...\r\n");
      ESP.restart(); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { telnet.printf("Progress OTA: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
      telnet.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        telnet.printf("Auth Failed\r\n");
      } else if (error == OTA_BEGIN_ERROR) {
        telnet.printf("Begin Failed\r\n");
      } else if (error == OTA_CONNECT_ERROR) {
        telnet.printf("Connect Failed\r\n");
      } else if (error == OTA_RECEIVE_ERROR) {
        telnet.printf("Receive Failed\r\n");
      } else if (error == OTA_END_ERROR) {
        telnet.printf("End Failed\r\n");
      } });
  ArduinoOTA.begin();
}
#endif

void setup()
{
  Serial.begin(115200);
  btSerial.begin(115200, SERIAL_8N1, 15, 16); // RX=15, TX=16
  // init btMeta mutex
  if (btMetaMutex == NULL)
  {
    btMetaMutex = xSemaphoreCreateMutex();
  }
  if (REAL_LEDBUILTIN != 255)
    pinMode(REAL_LEDBUILTIN, OUTPUT);
  if (yoradio_on_setup)
    yoradio_on_setup();
  pm.on_setup();
  config.init();
  // Force default source to radio on every boot as requested
  config.saveValue(&config.store.play_mode, (uint8_t)PM_WEB, true, true);
  display.init();
  player.init();
  network.begin();
  if (network.status != CONNECTED && network.status != SDREADY)
  {
    netserver.begin();
    initControls();
    // Check for config mode entry during startup
    if (ENC_BTNB != 255 && digitalRead(ENC_BTNB) == LOW) {
      Serial.println("Entering CONFIG mode on startup");
      configUI_enter();
    }
    display.putRequest(DSP_START);
    while (!display.ready())
      delay(10);
    configUI_init();
    tdaConfigUI_init();
    return;
  }
  // SD boot message suppressed — WAITFORSD is unused.
  config.initPlaylistMode();
  netserver.begin();
  telnet.begin();
  initControls();
  // Check for config mode entry during startup
  if (ENC_BTNB != 255 && digitalRead(ENC_BTNB) == LOW) {
    Serial.println("Entering CONFIG mode on startup");
    configUI_enter();
  }
  display.putRequest(DSP_START);
  while (!display.ready())
    delay(10);
  configUI_init();
  tdaConfigUI_init();
#if USE_OTA
  setupOTA();
#endif
  if (config.getMode() == PM_SDCARD)
    player.initHeaders(config.station.url);
  player.lockOutput = false;
  // Nie uruchamiaj automatycznie radia po starcie zasilania.
  // Poprzednio `smartstart == 1` uruchamiał odtwarzanie tutaj; usuwamy
  // to wywołanie aby urządzenie zawsze startowało w stanie zatrzymanym.
  // Jeśli chcesz ponownie przywrócić auto-start, odkomentuj poniższą linię.
  // if (config.store.smartstart == 1) player.sendCommand({PR_PLAY, config.lastStation()});
#if CLOCK_TTS_ENABLED
  clock_tts_setup(); // Módosítás: plussz sor. "clock_tts"
#endif
  if (psramFound())
  {
    Serial.println("✅ PSRAM elérhető!");
  }
  else
  {
    Serial.println("❌ PSRAM nem található!");
  }
  Audio::audio_info_callback = my_audio_info; // "audio_change" audiohandlers.h ban kezelve.
  Serial.printf("Total heap : %lu\n", ESP.getHeapSize());
  Serial.printf("Free heap  : %lu\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %lu\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM : %lu\n", ESP.getFreePsram());
  pm.on_end_setup();
}

void loop()
{
  timekeeper.loop1();
  telnet.loop();

  // Parse Bluetooth UART messages using C buffer (avoid String)
  while (btSerial.available())
  {
    char buf[256];
    int len = btSerial.readBytesUntil('\n', buf, sizeof(buf) - 1);
    if (len <= 0)
      break;
    buf[len] = '\0';
    // trim leading/trailing whitespace
    char *s = buf;
    while (*s && isspace((unsigned char)*s))
      s++;
    char *end = s + strlen(s) - 1;
    while (end >= s && isspace((unsigned char)*end))
    {
      *end = '\0';
      end--;
    }
    if (strlen(s) > 0)
    {
      bluetooth_handle_line(s);
    }
  }

  // Check for Bluetooth PLAY/PAUSE ACK timeout
  if (btMeta.awaitingAck && btMeta.ackDeadline > 0 && millis() > btMeta.ackDeadline)
  {
    // Try to resend PLAY/PAUSE a couple of times before giving up
    const uint8_t MAX_ACK_RETRIES = 2;
    if (btMetaMutex)
      xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
    if (btMeta.ackRetries < MAX_ACK_RETRIES)
    {
      // resend last expected command
      if (btMeta.expectedPlaying)
      {
        Serial.println("BT: ACK timeout — retrying PLAY");
        btSerial.println("PLAY");
      }
      else
      {
        Serial.println("BT: ACK timeout — retrying PAUSE");
        btSerial.println("PAUSE");
      }
      btMeta.ackRetries++;
      btMeta.ackDeadline = millis() + bt_ack_timeout_ms;
    }
    else
    {
      // exhausted retries — give up and clear awaiting flag
      btMeta.awaitingAck = false;
      btMeta.ackDeadline = 0;
      btMeta.ackRetries = 0;
      Serial.println("BT: ACK timeout for PLAY/PAUSE — giving up");
    }
    if (btMetaMutex)
      xSemaphoreGive(btMetaMutex);
  }

  // Heartbeat/timeout: only mark disconnected when we had active playback
  // or while awaiting an ACK. If the phone is connected but idle (not sending metadata),
  // keep `connected` true so the link appears persistent.
  // Snapshot btMeta under mutex to avoid torn reads while deciding heartbeat
  {
    // Snapshot whole metadata for heartbeat decision
    bt_metadata_t local;
    bt_meta_snapshot(&local);
    bool connected = local.connected;
    uint32_t lastSeen = local.lastSeen;
    bool awaitingAck = local.awaitingAck;
    bool playing = local.playing;
    bool probeSent = local.probeSent;
    uint32_t probeDeadline = local.probeDeadline;

    if (connected && lastSeen > 0 && (millis() - lastSeen) > bt_heartbeat_timeout_ms)
    {
      bool shouldDisconnect = false;
      if (awaitingAck)
        shouldDisconnect = true;
      if (playing)
        shouldDisconnect = true;

      if (shouldDisconnect)
      {
        // if we haven't probed yet, send a single STATUS probe and wait for response
        if (!probeSent)
        {
          Serial.println("BT: sending STATUS probe before disconnect");
          btSerial.println("STATUS");
          if (btMetaMutex)
            xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
          btMeta.probeSent = true;
          btMeta.probeDeadline = millis() + bt_ack_timeout_ms;
          if (btMetaMutex)
            xSemaphoreGive(btMetaMutex);
        }
        else
        {
          // probe already sent and expired -> perform disconnect
          if (probeDeadline > 0 && millis() > probeDeadline)
          {
            if (btMetaMutex)
              xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
            btMeta.connected = false;
            btMeta.playing = false;
            btMeta.awaitingAck = false;
            btMeta.ackDeadline = 0;
            btMeta.probeSent = false;
            btMeta.probeDeadline = 0;
            // set artist to indicate no connection
            strlcpy(btMeta.artist, "Brak połaczenia", sizeof(btMeta.artist));
            memset(btMeta.title, 0, sizeof(btMeta.title));
            if (btMetaMutex)
              xSemaphoreGive(btMetaMutex);
            Serial.println("BT: heartbeat probe expired — marked disconnected");
            if (config.getMode() == PM_BLUETOOTH)
            {
              display.putRequest(CLEARALLBITRATE);
              display.putRequest(NEWTITLE);
            }
          }
        }
      }
    }
  }
  {
    // idle connected device: keep `connected=true`; optionally log low-frequency
    static uint32_t lastIdleLog = 0;
    if (millis() - lastIdleLog > 60000)
    {
      lastIdleLog = millis();
      Serial.println("BT: idle — no metadata recently, keeping connected");
    }
  }

  if (!configUI_isActive() && (network.status == CONNECTED || network.status == SDREADY))
  {
    player.loop();
#if USE_OTA
    ArduinoOTA.handle();
#endif
  }
  pollInactivity();
  loopControls();
#ifdef NETSERVER_LOOP1
  netserver.loop();
#endif
#if CLOCK_TTS_ENABLED
  clock_tts_loop(); // Módosítás: plussz sor.  "clock_tts"
#endif
}
