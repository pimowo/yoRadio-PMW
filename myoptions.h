/* =========================================================
   yoRadio – plik konfiguracyjny myoptions.h
   ---------------------------------------------------------
   Generator opcji:
   https://trip5.github.io/ehRadio_myoptions/generator.html

   Repozytorium:
   https://github.com/e2002/yoradio

   KONIECZNIE przeczytaj:
   https://github.com/VaraiTamas/yoRadio/blob/main/README.md
   ========================================================= */

#ifndef myoptions_h
#define myoptions_h

/* =========================================================
   1. PLATFORMA / PŁYTKA
   ========================================================= */

/* Wymuszamy ESP32-S3 */
#ifndef ARDUINO_ESP32S3_DEV
#define ARDUINO_ESP32S3_DEV
#endif

/* =========================================================
   2. DEBUG / DIAGNOSTYKA
   ========================================================= */

// #define HEAP_DBG        // Debug sterty (tylko do testów)

/* =========================================================
   3. JĘZYK I LOKALIZACJA
   ========================================================= */

/* Język interfejsu
   Dostępne: HU NL PL RU EN GR SK DE */
#define L10N_LANGUAGE PL

/* =========================================================
   4. DIODY / LED
   ========================================================= */

/* Wbudowana dioda RGB (jeśli dostępna na płytce) */
#define USE_BUILTIN_LED false // true = używaj, false = ignoruj

/* =========================================================
   5. SIEĆ / OTA / BEZPIECZEŃSTWO
   ========================================================= */

/* Aktualizacja OTA z Arduino IDE */
#define USE_OTA false // true = włącz, false = wyłącz
#define OTA_PASS "12345987" // Hasło OTA (Zmień!)

/* Autoryzacja panelu WWW */
// #define HTTP_USER "admin"
// #define HTTP_PASS "haslo"

/* =========================================================
   6. WYŚWIETLACZ TFT
   ========================================================= */

/* Model wyświetlacza */
// #define DSP_MODEL DSP_ILI9488
// #define DSP_MODEL DSP_ILI9341
#define DSP_MODEL DSP_ST7796

/* Piny TFT */
#define BRIGHTNESS_PIN 14 // Podświetlenie (BL)
#define TFT_CS 10         // Chip Select
#define TFT_DC 9          // Data / Command
#define TFT_RST -1        // Reset (-1 = brak)

/*
   SPI (sprzętowe):
   GPIO 11 – MOSI
   GPIO 12 – SCLK
   GPIO 13 – MISO (NIE podłączać do TFT!)
*/

/* =========================================================
   7. PANEL DOTYKOWY (opcjonalnie)
   ========================================================= */

// #define TS_MODEL TS_MODEL_XPT2046
// #define TS_CS    3

/* =========================================================
   8. AUDIO – DAC PCM5102A (I2S)
   ========================================================= */

#define I2S_BCLK 5 // Bit Clock (BCK)
#define I2S_DOUT 4 // Dane audio (DIN)
#define I2S_LRC 6  // Left/Right Clock (LCK)

/* =========================================================
   9. STEROWANIE – ENKODERY
   ========================================================= */

/* Enkoder główny */
#define ENC_BTNR 41 // Obrót w prawo
#define ENC_BTNL 40 // Obrót w lewo
#define ENC_BTNB 39 // Przycisk
#define ENC_INTERNALPULLUP false

/* Drugi enkoder (opcjonalny) */
// #define ENC2_BTNR 47
// #define ENC2_BTNL 48
// #define ENC2_BTNB 21
// #define ENC2_INTERNALPULLUP true

/* =========================================================
   10. PRZYCISK FUNKCYJNY
   ========================================================= */

/* Przycisk zmiany źródła audio
   yoRadio / SD / Bluetooth / AUX1 / AUX2 */
#define SRC_BTN 42

/* =========================================================
   11. ŹRÓDŁA AUDIO
   ========================================================= */

/* Włączanie źródeł audio do przełączania */
#define SRC_BT true   // Bluetooth
#define SRC_AUX1 true // AUX1 S/PDIF
#define SRC_AUX2 false // AUX3 RCA 
#define SRC_AUX3 false // AUX2 RCA

// Karta SD
#define SRC_SD 0   // 1 = używaj SD, 0 = nie używaj

// Nazwa na potrzeby konfiguracji - yoRadio i karta SD jest na jednym źródle audio
#define SRC_WEB_SD_NAME "WEB/SD"

/* Nazwy źródeł audio */
#define SRC_WEB_NAME "Radio WEB"
#define SRC_WEB_NAME2 "Radio internetowe"

#define SRC_SD_NAME "Karta SD"
#define SRC_SD_NAME2 "Brak karty SD!"

#define SRC_BT_NAME "Bluetooth"
#define SRC_BT_NAME2 "Oczekuję na połączenie..."

//#define SRC_AUX1_NAME "AUX 1 S/PDIF"0
//#define SRC_AUX1_NAME2 "Wejście audio cyfrowe"
#define SRC_AUX1_NAME "Telewizor"
#define SRC_AUX1_NAME2 "Wejście audio S/PDIF"

#define SRC_AUX2_NAME "AUX 2 RCA"
#define SRC_AUX2_NAME2 "Wejście audio analogowe"

#define SRC_AUX3_NAME "Radio FM"
#define SRC_AUX3_NAME2 "Radio analogowe"
/*
ESP32_BT TX (GPIO 16) → yoRadio RX (GPIO 15)
ESP32_BT RX (GPIO 17) → yoRadio TX (GPIO 16)
GND wspólny między urządzeniami
*/

/* =========================================================
   12. RTC – ZEGAR CZASU RZECZYWISTEGO
   ========================================================= */

#define RTC_SCL     7
#define RTC_SDA     8
#define RTC_MODULE DS3231

/* Automatyczna zmiana czasu (zimowy/letni)
   true  = automatyczna korekta czasu według reguły DST (jeśli zaimplementowane)
   false = brak automatycznej korekty (czas ustawiany ręcznie przez tzHour/tzMin)
   Ustawienie można zmienić przed kompilacją w tym pliku `myoptions.h`.
*/
#define AUTO_TIME_SYNC true

/* =========================================================
   12. PILOT IR / WYBUDZANIE
   ========================================================= */

// #define IR_PIN   38
// #define WAKE_PIN 42

/* =========================================================
   13. KARTA SD
   ========================================================= */

#if SRC_SD
   // SD_DETECT_PIN: choose a free GPIO on your board. Avoid UART pins.
   // Using GPIO2 by default (safe on many devboards). Change if needed.
   #define SD_DETECT_PIN 2      // Pin wykrywania karty SD
   #define SDC_CS        18     // Chip Select karty SD
   #define SDSPISPEED   4000000 // 4 MHz – stabilniej przy SPI shared
#endif

/* =========================================================
   14. ZEGAR – WYGLĄD I FUNKCJE
   ========================================================= */

#define CLOCKFONT_MONO false // Segmenty nieaktywne (true = widoczne)

// #define AM_PM_STYLE          // Format 12h
// #define CLOCKFONT VT_DIGI_OLD // Stara czcionka 7-segmentowa

/* =========================================================
   17. LISTA STACJI
   ========================================================= */

// #define DIRECT_CHANNEL_CHANGE     // Zmiana bez klikania
// #define STATIONS_LIST_RETURN_TIME 2

/* =========================================================
   18. WZMACNIACZ
   ========================================================= */

// #define PWR_AMP 38 // (złącze IR) załączenie wzmacniacza

/* =========================================================
   19. TDA7719 CONFIG
   ========================================================= */

/* Czas wyjścia z konfiguracji TDA7719 bez aktywności (ms) */
#define TDA_CONFIG_INACTIVITY_MS 5000

#endif // myoptions_h