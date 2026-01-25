/*************************************************************************************
    Plik konfiguracyjny dla wyświetlaczy ST7796 480x320.
    Skopiuj ten plik do yoRadio/src/displays/conf/displayST7789conf_custom.h
    i zmodyfikuj go wedle potrzeb.
    Więcej informacji na https://github.com/e2002/yoradio/wiki/Widgets#widgets-description
*************************************************************************************/

#ifndef displayST7789conf_h
#define displayST7789conf_h

// Podstawowe wymiary wyświetlacza
#define DSP_WIDTH 480                          // Szerokość wyświetlacza w pikselach
#define DSP_HEIGHT 320                         // Wysokość wyświetlacza w pikselach
#define TFT_FRAMEWDT 10                        // Szerokość ramki (margines) w pikselach
#define MAX_WIDTH DSP_WIDTH - TFT_FRAMEWDT * 2 // Maksymalna szerokość dostępna dla treści

// Korekta pozycji tytułu w zależności od pełnego bitrate
#if BITRATE_FULL
#define TITLE_FIX 44 // Przesunięcie tytułu gdy bitrate jest pełny
#else
#define TITLE_FIX 0 // Brak przesunięcia
#endif
#define bootLogoTop 110 // Pozycja górna logo boot w pikselach

/* KONFIGURACJE PRZEWINIĘĆ (SCROLLS) */
/* Format: {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
const ScrollConfig metaConf PROGMEM = {{TFT_FRAMEWDT, TFT_FRAMEWDT, 4, WA_LEFT}, 140, false, MAX_WIDTH, 5000, 2, 28};           // Konfiguracja przewijania metadanych (nazwa stacji)
const ScrollConfig title1Conf PROGMEM = {{TFT_FRAMEWDT, 62, 2, WA_LEFT}, 140, false, MAX_WIDTH, 5000, 2, 28};                   // Konfiguracja przewijania tytułu 1 (np. artysta)
const ScrollConfig title2Conf PROGMEM = {{TFT_FRAMEWDT, 86, 2, WA_LEFT}, 140, false, MAX_WIDTH, 5000, 2, 28};                   // Konfiguracja przewijania tytułu 2 (np. utwór)
const ScrollConfig playlistConf PROGMEM = {{TFT_FRAMEWDT, 146, 3, WA_LEFT}, 140, true, MAX_WIDTH, 5000, 2, 28};                 // Konfiguracja przewijania playlisty
const ScrollConfig apTitleConf PROGMEM = {{TFT_FRAMEWDT, TFT_FRAMEWDT, 4, WA_CENTER}, 140, false, MAX_WIDTH, 0, 2, 28};         // Konfiguracja tytułu AP (Access Point)
const ScrollConfig apSettConf PROGMEM = {{TFT_FRAMEWDT, 320 - TFT_FRAMEWDT - 16, 2, WA_LEFT}, 140, false, MAX_WIDTH, 0, 2, 28}; // Konfiguracja ustawień AP
const ScrollConfig weatherConf PROGMEM = {{TFT_FRAMEWDT, 116, 2, WA_CENTER}, 140, false, MAX_WIDTH, 5000, 2, 28};               // Konfiguracja przewijania pogody

/* KONFIGURACJE TŁA (BACKGROUNDS) */
/* Format: {{ left, top, fontsize, align }, width, height, outlined } */
const FillConfig metaBGConf PROGMEM = {{3, 45, 0, WA_CENTER}, DSP_WIDTH - 6, 1, true};                                 // Linia pod nazwą stacji radiowej
const FillConfig metaBGConfInv PROGMEM = {{0, 50, 0, WA_LEFT}, DSP_WIDTH, 2, false};                                   // Inwersyjne tło metadanych
const FillConfig volbarConf PROGMEM = {{TFT_FRAMEWDT, DSP_HEIGHT - TFT_FRAMEWDT - 8, 0, WA_LEFT}, MAX_WIDTH, 5, true}; // Pasek głośności
const FillConfig playlBGConf PROGMEM = {{0, 138, 0, WA_LEFT}, DSP_WIDTH, 36, false};                                   // Tło playlisty
const FillConfig heapbarConf PROGMEM = {{0, DSP_HEIGHT - 2, 0, WA_LEFT}, DSP_WIDTH, 2, false};                         // Pasek heap (pamięci)

/* KONFIGURACJE WIDGETÓW (WIDGETS) */
/* Format: { left, top, fontsize, align } */
const WidgetConfig bootstrConf PROGMEM = {0, 243, 2, WA_CENTER};                    // Widget boot logo
const WidgetConfig bitrateConf PROGMEM = {TFT_FRAMEWDT, 145, 2, WA_RIGHT};          // Widget bitrate
const WidgetConfig voltxtConf PROGMEM = {0, DSP_HEIGHT - 38, 2, WA_CENTER};         // Widget głośności
const WidgetConfig iptxtConf PROGMEM = {TFT_FRAMEWDT, DSP_HEIGHT - 38, 2, WA_LEFT}; // Widget IP
const WidgetConfig rssiConf PROGMEM = {TFT_FRAMEWDT, DSP_HEIGHT - 34, 2, WA_RIGHT}; // Widget RSSI WiFi
const WidgetConfig rssiNumConf PROGMEM = {(TFT_FRAMEWDT + 30), DSP_HEIGHT - 34, 2, WA_RIGHT}; // Numeric RSSI
const WidgetConfig numConf PROGMEM = {0, 200, 70, WA_CENTER};                       // Widget numeru stacji
const WidgetConfig apNameConf PROGMEM = {TFT_FRAMEWDT, 88, 3, WA_CENTER};           // Widget nazwy AP
const WidgetConfig apName2Conf PROGMEM = {TFT_FRAMEWDT, 120, 3, WA_CENTER};         // Widget nazwy AP 2
const WidgetConfig apPassConf PROGMEM = {TFT_FRAMEWDT, 173, 3, WA_CENTER};          // Widget hasła AP
const WidgetConfig apPass2Conf PROGMEM = {TFT_FRAMEWDT, 205, 3, WA_CENTER};         // Widget hasła AP 2
const WidgetConfig clockConf PROGMEM = {10, 211, 2, WA_RIGHT};                      // Widget zegara (odległość od prawej, góra, rozmiar czcionki)
const WidgetConfig vuConf PROGMEM = {35, 258, 1, WA_CENTER};                        // Widget VU-meter (wyśrodkowany, align nieużywany)
const WidgetConfig bootWdtConf PROGMEM = {0, 216, 1, WA_CENTER};                    // Widget watchdog boot
const WidgetConfig dateConf PROGMEM = {TFT_FRAMEWDT * 2, 226, 1, WA_LEFT};          // Widget daty (modyfikacja dla nowej linii "date")
const ProgressConfig bootPrgConf PROGMEM = {90, 14, 4};                             // Konfiguracja paska postępu boot

// {{ left, top, fontsize, align } dimension}

const BitrateConfig fullbitrateConf PROGMEM = {{10, 142, 2, WA_RIGHT}, 60}; // Konfiguracja pełnego widgetu bitrate

/* KONFIGURACJE PASM VU (BANDS) */
/* Format: { onebandwidth (szerokość), onebandheight (wysokość), bandsHspace (przestrzeń pozioma), bandsVspace (przestrzeń pionowa), numofbands (na wysokość), fadespeed (szybkość zanikania)} */
#ifdef BOOMBOX_STYLE
const VUBandsConfig bandsConf PROGMEM = {200, 7, 4, 2, 20, 12}; // Styl boombox
#else
const VUBandsConfig bandsConf PROGMEM = {300, 7, 3, 2, 30, 6}; // Własny styl {400, 7, 3, 2, 8, 29};
#endif

/* FORMATY STRINGÓW (STRINGS) */
const char numtxtFmt[] PROGMEM = "%d";       // Format numeru
const char rssiFmt[] PROGMEM = "WiFi %ddBm"; // Format RSSI WiFi
// const char rssiFmt[] PROGMEM = "WiFi %d";                                         // Oryginalny format
const char iptxtFmt[] PROGMEM = "%s";            // Format IP
const char voltxtFmt[] PROGMEM = "\023\025%d%%"; // Format głośności (oryginalny "\023\025%d", modyfikacja "vol_step")
const char bitrateFmt[] PROGMEM = "%d kBs";      // Format bitrate

/* KONFIGURACJE RUCHU (MOVES) */
/* Format: { left, top, width } */
const MoveConfig clockMove PROGMEM = {0, 176, -1};             // Ruch zegara
const MoveConfig weatherMove PROGMEM = {10, 116, MAX_WIDTH};   // Ruch pogody gdy VU wyłączone (szeroka pozycja)
const MoveConfig weatherMoveVU PROGMEM = {10, 116, MAX_WIDTH}; // Pozycja widgetu pogody

#endif
