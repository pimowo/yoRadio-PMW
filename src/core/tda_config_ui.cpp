// Move TDA_UI_LOG macro to the top of the file

// Dodano stałe i zmienne dla inactivity

// Declare freeItemLabels before its usage
static void freeItemLabels();

// Define freeItemLabels function
static void freeItemLabels() {
  // Implementation of freeItemLabels
}

// TDA7719 configuration UI (modal) - full implementation
// Includes and implementation follow further below.
#include "tda_config_ui.h"
#include "tda_config_types.h"
#include "config.h"
#include "display.h"
#include "player.h"
#include "SPIFFS.h"
#include "../yoEncoder/yoEncoder.h"
#include "options.h"
#include "telnet.h"

#define TDA_UI_LOG(...) Serial.printf(__VA_ARGS__)
#define INACTIVITY_MS TDA_CONFIG_INACTIVITY_MS
// Note: this module is strictly a data/provider + input-state handler.
// It must NOT perform any drawing or change layout/fonts/colors. All
// rendering is done by the existing playlist renderer which will request
// labels via the accessors below.

// Implementation notes:
// - This module provides a non-blocking, state-machine-driven configuration UI
//   that is controlled entirely by the main encoder (rotation + push).
// - It uses the existing global `encoder` instance (defined in controls.cpp)
//   by declaring it extern so we can read rotation deltas.
// - It locks the main `display` while in settings and draws directly to `dsp`.
// - Settings are staged in RAM and saved atomically to SPIFFS at explicit user request.
// - All timing is non-blocking (millis()). Debounce and long-press detection implemented.

// NOTE: this file intentionally keeps UI drawing simple and relies on theme colors
// from `config.theme` so header / fonts match existing screens visually.

// -------------------- Configuration data --------------------
// TDASettings and InputCfg are defined in tda_config_types.h

static const char *SETTINGS_PATH = "/data/tda7719.bin";
// avoid macro name collision with existing TMP_PATH macro from config.h
static const char *SETTINGS_TMP_PATH = "/data/tda7719.tmp";

// Defaults
static TDASettings defaultSettings()
{
  TDASettings s;
  s.masterVolume = -10; // default -10dB
  s.mute = false;
  s.balance = 0;
  s.fader = 0;
  s.bassGain = 0;
  s.bassCenterIdx = 0;
  s.bassQIdx = 0;
  s.bassDC = false;
  s.midGain = 0;
  s.midCenterIdx = 0;
  s.midQIdx = 0;
  s.trebleGain = 0;
  s.trebleCenterIdx = 0;
  s.loudnessLevel = 0;
  s.loudnessFreqIdx = 0;
  s.loudnessHighBoost = false;
  s.outL = 0;
  s.outR = 0;
  s.outSub = 0;
  s.subFilterIdx = 0;
  s.softStepTimeIdx = 0;
  s.softMute = false;
  for (int i = 0; i < 5; i++) {
    s.inputs[i].enabled = true;
    s.inputs[i].gain = 0;
    s.inputs[i].routeToSub = false;
    s.inputInvert[i] = false;
  }
  s.dcDetect = false;
  s.levelMeterOut = false;
  s.winIn = true;
  s.schema_version = 1;
  s.crc32 = 0;
  return s;
}

// -------------------- CRC32 (small, deterministic) --------------------
static uint32_t crc32(const void *data, size_t n_bytes)
{
  const uint8_t *p = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < n_bytes; ++i) {
    crc ^= p[i];
    for (int k = 0; k < 8; k++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
    }
  }
  return ~crc;
}

// -------------------- Storage (atomic) --------------------
// Weak hardware hook: driver can provide a strong implementation to apply
// settings directly to the TDA7719. Default (weak) implementation does nothing
// which allows the UI + persistence to work without physical TDA presence.
extern "C" bool tdaDriver_apply(const TDASettings &s) __attribute__((weak));
extern "C" bool tdaDriver_apply(const TDASettings &s) { (void)s; return false; }

// Weak UI beep hook: platform may implement `uiBeep()` to emit a short tone.
extern "C" void uiBeep() __attribute__((weak));
extern "C" void uiBeep() { /* default: no-op */ }

static bool saveSettingsToFS(const TDASettings &s)
{
  // write to temporary file then rename
  File f = SPIFFS.open(SETTINGS_TMP_PATH, FILE_WRITE);
  if (!f) {
    Serial.println("[TDA_UI] Failed to open temporary file for writing");
    return false;
  }
  if (f.write((const uint8_t *)&s, sizeof(TDASettings)) != sizeof(TDASettings)) {
    Serial.println("[TDA_UI] Failed to write settings to temporary file");
    f.close();
    SPIFFS.remove(SETTINGS_TMP_PATH);
    return false;
  }
  f.close();
  // atomic rename (remove target then rename)
  SPIFFS.remove(SETTINGS_PATH);
  return SPIFFS.rename(SETTINGS_TMP_PATH, SETTINGS_PATH);
}

static bool loadSettingsFromFS(TDASettings &out)
{
  if (!SPIFFS.exists(SETTINGS_PATH)) return false;
  File f = SPIFFS.open(SETTINGS_PATH, FILE_READ);
  if (!f) return false;
  if (f.size() != sizeof(TDASettings)) { f.close(); return false; }
  TDASettings tmp;
  if (f.read((uint8_t *)&tmp, sizeof(TDASettings)) != sizeof(TDASettings)) { f.close(); return false; }
  f.close();
  uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  uint32_t calc = crc32(&tmp, sizeof(TDASettings));
  if (calc != crc) return false;
  tmp.crc32 = crc; // restore
  out = tmp;
  return true;
}

// -------------------- UI state machine --------------------
enum UIState { UI_IDLE, UI_PRESS_DETECT, UI_ENTERED, UI_LIST, UI_EDIT, UI_SAVING, UI_EXIT };
static UIState state = UI_IDLE;
// timestamp when we entered CONFIG mode (for debouncing stray clicks)
static uint32_t enteredAt = 0;

// timers and constants
static const uint32_t ENTER_HOLD_MS = 3000; // strict requirement
static const uint32_t SAVE_HOLD_MS = 3000; // saving long-press in list

// Note: encoder button handling is performed by `controls` and forwarded via
// `tdaConfigUI_onBtnClick()`. This module does not poll ENC_BTNB directly.

// activity timer
static uint32_t lastActivity = 0;

// staged settings
static TDASettings staged;
static bool stagedLoaded = false;

// list / edit cursor
static int listIndex = 0; // index in flattened list of fields
static int editIndex = -1; // -1 = not editing

// visible entries: typed pointer to field + small type enum to avoid unaligned writes
enum ValType : uint8_t { VT_INT8=1, VT_UINT8=2, VT_UINT16=3, VT_BOOL=4, VT_INT16=5 };
struct ItemDef { const char *label; int minv; int maxv; int step; void *ptr; uint8_t vtype; bool isInputField; int inputIdx; int inputField; };
// We'll allocate a local vector of items at runtime mapping to fields in `staged`.
static ItemDef items[64];
static int itemsCount = 0;
// dirty flag indicates data changed and renderer should redraw
static bool dirty = false;
// Save/exit confirmation
static bool saveSelection = false; // false = NIE, true = TAK

// extern references
extern yoEncoder encoder; // global encoder defined in controls.cpp
// forwarded encoder delta from controls when CONFIG mode active
extern volatile int8_t cfg_enc_delta;
extern volatile bool cfg_enc_click;

// forward declarations
static void buildItems();
static void applySettingsToHardware();

void tdaConfigUI_init()
{
  // ensure SPIFFS is mounted
  if (!SPIFFS.begin(true)) {
    Serial.println("[TDA_UI] SPIFFS begin failed");
  }
  staged = defaultSettings();
  stagedLoaded = loadSettingsFromFS(staged);
  if (!stagedLoaded) {
    staged = defaultSettings();
    staged.crc32 = 0;
  }
  // precompute crc for staged (not saved yet)
  staged.crc32 = 0;
  staged.crc32 = crc32(&staged, sizeof(TDASettings));
  buildItems();
}

// New accessors


// Build flat item list mapping to staged structure. We store pointers to fields where possible.
static void buildItems()
{
  itemsCount = 0;
  // Master group (masterVolume omitted — global volume controlled from main screen)
  items[itemsCount++] = {"Wycisz", 0, 1, 1, (void *)&staged.mute, VT_BOOL, false, 0, 0};
  items[itemsCount++] = {"Balans", -15, 15, 1, (void *)&staged.balance, VT_INT8, false, 0, 0};
  items[itemsCount++] = {"Fader", -15, 15, 1, (void *)&staged.fader, VT_INT8, false, 0, 0};
  // Tone - Bas
  items[itemsCount++] = {"Bas - Wzmocnienie", -15, 15, 1, (void *)&staged.bassGain, VT_INT8, false, 0, 0};
  items[itemsCount++] = {"Bas - Częstotliwość", 0, 3, 1, (void *)&staged.bassCenterIdx, VT_UINT8, false, 0, 0};
  items[itemsCount++] = {"Bas - Q", 0, 3, 1, (void *)&staged.bassQIdx, VT_UINT8, false, 0, 0};
  items[itemsCount++] = {"Bas - DC", 0, 1, 1, (void *)&staged.bassDC, VT_BOOL, false, 0, 0};
  // Średnie
  items[itemsCount++] = {"Średnie - Wzmocnienie", -15, 15, 1, (void *)&staged.midGain, VT_INT8, false, 0, 0};
  items[itemsCount++] = {"Średnie - Częstotliwość", 0, 3, 1, (void *)&staged.midCenterIdx, VT_UINT8, false, 0, 0};
  items[itemsCount++] = {"Średnie - Q", 0, 3, 1, (void *)&staged.midQIdx, VT_UINT8, false, 0, 0};
  // Wysokie
  items[itemsCount++] = {"Wysokie - Wzmocnienie", -15, 15, 1, (void *)&staged.trebleGain, VT_INT8, false, 0, 0};
  items[itemsCount++] = {"Wysokie - Częstotliwość", 0, 3, 1, (void *)&staged.trebleCenterIdx, VT_UINT8, false, 0, 0};
  // Loudness
  items[itemsCount++] = {"Loudness - Poziom", 0, 15, 1, (void *)&staged.loudnessLevel, VT_INT8, false, 0, 0};
  items[itemsCount++] = {"Loudness - Częstotliwość", 0, 3, 1, (void *)&staged.loudnessFreqIdx, VT_UINT8, false, 0, 0};
  items[itemsCount++] = {"Loudness - Hi boost", 0, 1, 1, (void *)&staged.loudnessHighBoost, VT_BOOL, false, 0, 0};
  // Outputs
  items[itemsCount++] = {"Wyjście L (dB)", -79, 0, 1, (void *)&staged.outL, VT_INT16, false, 0, 0};
  items[itemsCount++] = {"Wyjście R (dB)", -79, 0, 1, (void *)&staged.outR, VT_INT16, false, 0, 0};
  items[itemsCount++] = {"Wyjście Sub (dB)", -79, 0, 1, (void *)&staged.outSub, VT_INT16, false, 0, 0};
  // Sub filter
  items[itemsCount++] = {"Filtr Sub", 0, 3, 1, (void *)&staged.subFilterIdx, VT_UINT8, false, 0, 0};
  // Soft
  items[itemsCount++] = {"Soft - Czas kroku", 0, 1, 1, (void *)&staged.softStepTimeIdx, VT_UINT8, false, 0, 0};
  items[itemsCount++] = {"Soft - Wyciszenie", 0, 1, 1, (void *)&staged.softMute, VT_BOOL, false, 0, 0};
  // Inputs (5)
  const char* inputNames[5] = {SRC_WEB_SD_NAME, SRC_BT_NAME, SRC_AUX1_NAME, SRC_AUX2_NAME, SRC_AUX3_NAME};
  for (int i = 0; i < 5; i++) {
    char buf[64]; // Zwiększony rozmiar bufora
    char buf2[64];
    snprintf(buf, sizeof(buf), "%s - Aktywne", inputNames[i]);
    items[itemsCount++] = {strdup(buf), 0, 1, 1, (void *)&staged.inputs[i].enabled, VT_BOOL, true, i, 0};
    snprintf(buf, sizeof(buf), "%s - Wzmocnienie", inputNames[i]);
    items[itemsCount++] = {strdup(buf), -24, 24, 1, (void *)&staged.inputs[i].gain, VT_INT8, true, i, 1};
    snprintf(buf, sizeof(buf), "%s - Sub", inputNames[i]);
    items[itemsCount++] = {strdup(buf), 0, 1, 1, (void *)&staged.inputs[i].routeToSub, VT_BOOL, true, i, 2};
    snprintf(buf2, sizeof(buf2), "%s - Odwróć fazę", inputNames[i]);
    items[itemsCount++] = {strdup(buf2), 0, 1, 1, (void *)&staged.inputInvert[i], VT_BOOL, true, i, 3};
  }
  // Diagnostics
  items[itemsCount++] = {"Detekcja DC", 0, 1, 1, (void *)&staged.dcDetect, VT_BOOL, false, 0, 0};
  items[itemsCount++] = {"Wyjście LevelMeter", 0, 1, 1, (void *)&staged.levelMeterOut, VT_BOOL, false, 0, 0};
  items[itemsCount++] = {"BEEP (dźwięk)", 0, 1, 1, (void *)&staged.winIn, VT_BOOL, false, 0, 0};
  TDA_UI_LOG("[TDA_UI] buildItems itemsCount=%d\n", itemsCount);
}

// Access helpers to read/write typed item values safely
static int getItemValue(const ItemDef &it)
{
  switch ((ValType)it.vtype) {
    case VT_INT8: return *(int8_t *)it.ptr;
    case VT_UINT8: return *(uint8_t *)it.ptr;
    case VT_UINT16: return *(uint16_t *)it.ptr;
    case VT_INT16: return *(int16_t *)it.ptr;
    case VT_BOOL: return *(bool *)it.ptr ? 1 : 0;
    default: return 0;
  }
}

static void setItemValue(ItemDef &it, int v)
{
  switch ((ValType)it.vtype) {
    case VT_INT8: *(int8_t *)it.ptr = (int8_t)v; break;
    case VT_UINT8: *(uint8_t *)it.ptr = (uint8_t)v; break;
    case VT_UINT16: *(uint16_t *)it.ptr = (uint16_t)v; break;
    case VT_INT16: *(int16_t *)it.ptr = (int16_t)v; break;
    case VT_BOOL: *(bool *)it.ptr = (v != 0); break;
    default: break;
  }
}

// New accessors (placed after typed helpers so getItemValue is available)
bool tdaConfigUI_isEditMode() { return state == UI_EDIT; }
bool tdaConfigUI_isExitConfirm() { return state == UI_SAVING; }
bool tdaConfigUI_getExitSave() { return saveSelection; }

const char *tdaConfig_getItemBaseLabel(size_t idx)
{
  if (idx >= (size_t)itemsCount) return "";
  return items[idx].label;
}

const char *tdaConfig_getItemValueStr(size_t idx)
{
  if (idx >= (size_t)itemsCount) return "";
  static char vbuf[64];
  int v = getItemValue(items[idx]);
  if (items[idx].isInputField && items[idx].inputField == 1) {
    snprintf(vbuf, sizeof(vbuf), "%ddB", v);
    return vbuf;
  }
  if (strstr(items[idx].label, "(dB)")) {
    snprintf(vbuf, sizeof(vbuf), "%ddB", v);
    return vbuf;
  }
  if (((ValType)items[idx].vtype) == VT_BOOL) {
    return (v ? "TAK" : "NIE");
  }
  snprintf(vbuf, sizeof(vbuf), "%d", v);
  return vbuf;
}

// No rendering helpers in this module by design.

// NOTE: drawing code removed. This module marks data as dirty and exposes
// labels/values; the existing playlist renderer uses those accessors.

// forward declarations for internal state-machine functions
static void enterConfigMode();
void exitConfigAndReturnToNormal(bool save);


// Public API wrappers (match header)
void tdaConfigUI_enter()
{
  enterConfigMode();
}

void tdaConfigUI_exit_save()
{
  // Enter save-confirmation state (non-blocking). Actual save happens on confirm.
  if (state == UI_IDLE) return;
  state = UI_SAVING;
  saveSelection = false; // default to NIE
  dirty = true;
}

void tdaConfigUI_exit_discard()
{
  exitConfigAndReturnToNormal(false);
}

bool tdaConfigUI_isActive()
{
  return state != UI_IDLE;
}

void tdaConfigUI_onBtnClick(int id)
{
  // simple forward: update activity and toggle edit/list
  (void)id;
  uint32_t now = millis();
  // ignore clicks that happen immediately after entering CONFIG (debounce)
  if (now - enteredAt < 250) {
    Serial.printf("[TDA_UI] Ignored click at %lu (entered %lu)\n", now, enteredAt);
    lastActivity = now;
    return;
  }
  lastActivity = now;
  if (state == UI_LIST) {
    editIndex = listIndex;
    state = UI_EDIT;
    dirty = true;
  } else if (state == UI_EDIT) {
    state = UI_LIST;
    editIndex = -1;
    dirty = true;
  } else if (state == UI_SAVING) {
    // confirm/cancel save
    if (saveSelection) {
      // save and apply
      tdaConfig_save();
    }
    // exit regardless
    state = UI_IDLE;
    dirty = true;
  }
}

// (Accessors implemented below — composite label/value version)

static void enterConfigMode()
{
  if (state != UI_IDLE) return;
  // load staged settings or defaults
  if (!stagedLoaded) {
    staged = defaultSettings();
  }
  listIndex = 0;
  editIndex = -1;
  lastActivity = millis();
  state = UI_LIST;
  saveSelection = false;
  dirty = true;
  TDA_UI_LOG("[TDA_UI] ENTER Config at %lu\n", millis());
  enteredAt = millis();
}

void exitConfigAndReturnToNormal(bool save)
{
  if (save) {
    saveSettingsToFS(staged);
    TDA_UI_LOG("[TDA_UI] Settings saved\n");
  } else {
    TDA_UI_LOG("[TDA_UI] Settings not saved\n");
  }
  state = UI_IDLE;
  // Reset transient flags
  listIndex = 0;
  editIndex = -1;
  lastActivity = 0;
  TDA_UI_LOG("[TDA_UI] EXIT Config (save=%d) at %lu\n", save?1:0, millis());
}

static void applySettingsToHardware()
{
  // separated logic: updating audio hardware should be done here.
  // We'll update player volume/balance/tone via existing config APIs.
  // Note: this function only applies staged values to runtime config, but
  // persistence already handled in exitConfigMode.
  // Map masterVolume dB (-79..0) to internal 0..100 volume scale
  int volPercent = 0;
  if (staged.masterVolume <= -79) volPercent = 0;
  else volPercent = (int)((staged.masterVolume + 79) * 100 / 79);
  if (volPercent < 0) volPercent = 0;
  if (volPercent > 100) volPercent = 100;
  // Apply runtime-only effects (no config persistence here)
  player.setVol(volPercent);
  // Hardware hook (weak) - driver may apply full register map
  tdaDriver_apply(staged);
}

// Map working (staged) settings into global `config.store` and persist via EEPROM
static void mapWorkingToGlobal(const TDASettings &w)
{
  // Volume: convert from -79..0 dB to 0..100 percent scale and persist
  uint8_t volPercent = 0;
  if (w.masterVolume <= -79) volPercent = 0;
  else volPercent = (uint8_t)((w.masterVolume + 79) * 100 / 79);
  config.saveValue(&config.store.volume, volPercent, true, true);
  // Basic tone & balance
  config.saveValue(&config.store.balance, (int8_t)w.balance, true, true);
  config.saveValue(&config.store.bass, (int8_t)w.bassGain, true, true);
  config.saveValue(&config.store.middle, (int8_t)w.midGain, true, true);
  config.saveValue(&config.store.trebble, (int8_t)w.trebleGain, true, true);
  // Persist full TDA7719 struct into config.store.tda7719 (field-by-field)
  config.saveValue(&config.store.tda7719.masterVolume, (int16_t)w.masterVolume, true, true);
  config.saveValue(&config.store.tda7719.mute, (bool)w.mute, true, true);
  config.saveValue(&config.store.tda7719.balance, (int8_t)w.balance, true, true);
  config.saveValue(&config.store.tda7719.fader, (int8_t)w.fader, true, true);
  config.saveValue(&config.store.tda7719.bassGain, (int8_t)w.bassGain, true, true);
  config.saveValue(&config.store.tda7719.bassCenterIdx, (uint8_t)w.bassCenterIdx, true, true);
  config.saveValue(&config.store.tda7719.bassQIdx, (uint8_t)w.bassQIdx, true, true);
  config.saveValue(&config.store.tda7719.bassDC, (bool)w.bassDC, true, true);
  config.saveValue(&config.store.tda7719.midGain, (int8_t)w.midGain, true, true);
  config.saveValue(&config.store.tda7719.midCenterIdx, (uint8_t)w.midCenterIdx, true, true);
  config.saveValue(&config.store.tda7719.midQIdx, (uint8_t)w.midQIdx, true, true);
  config.saveValue(&config.store.tda7719.trebleGain, (int8_t)w.trebleGain, true, true);
  config.saveValue(&config.store.tda7719.trebleCenterIdx, (uint8_t)w.trebleCenterIdx, true, true);
  config.saveValue(&config.store.tda7719.loudnessLevel, (int8_t)w.loudnessLevel, true, true);
  config.saveValue(&config.store.tda7719.loudnessFreqIdx, (uint8_t)w.loudnessFreqIdx, true, true);
  config.saveValue(&config.store.tda7719.loudnessHighBoost, (bool)w.loudnessHighBoost, true, true);
  config.saveValue(&config.store.tda7719.outL, (int16_t)w.outL, true, true);
  config.saveValue(&config.store.tda7719.outR, (int16_t)w.outR, true, true);
  config.saveValue(&config.store.tda7719.outSub, (int16_t)w.outSub, true, true);
  config.saveValue(&config.store.tda7719.subFilterIdx, (uint8_t)w.subFilterIdx, true, true);
  config.saveValue(&config.store.tda7719.softStepTimeIdx, (uint8_t)w.softStepTimeIdx, true, true);
  config.saveValue(&config.store.tda7719.softMute, (bool)w.softMute, true, true);
  for (int i=0;i<5;i++) {
    config.saveValue(&config.store.tda7719.inputs[i].enabled, (bool)w.inputs[i].enabled, true, true);
    config.saveValue(&config.store.tda7719.inputs[i].gain, (int8_t)w.inputs[i].gain, true, true);
    config.saveValue(&config.store.tda7719.inputs[i].routeToSub, (bool)w.inputs[i].routeToSub, true, true);
    config.saveValue(&config.store.tda7719.inputInvert[i], (bool)w.inputInvert[i], true, true);
  }
  config.saveValue(&config.store.tda7719.dcDetect, (bool)w.dcDetect, true, true);
  config.saveValue(&config.store.tda7719.levelMeterOut, (bool)w.levelMeterOut, true, true);
  config.saveValue(&config.store.tda7719.winIn, (bool)w.winIn, true, true);
  config.saveValue(&config.store.tda7719.schema_version, (uint8_t)w.schema_version, true, true);
  config.saveValue(&config.store.tda7719.crc32, (uint32_t)w.crc32, true, true);
}

// Load persisted TDA settings from FS and map to global config.store (if present)
bool tdaConfig_loadToGlobal()
{
  TDASettings tmp;
  if (!loadSettingsFromFS(tmp)) return false;
  mapWorkingToGlobal(tmp);
  // Also mirror persisted raw struct into config.store.tda7719 for completeness
  // (mapWorkingToGlobal already writes all fields into config.store.tda7719)
  return true;
}

// Load current global config.store values into working copy (staged)
void tdaConfig_loadWorkingFromGlobal()
{
  int16_t volU = config.store.volume; // Deklaracja i inicjalizacja volU
  // Start from defaults then override mapped fields
  staged = defaultSettings();
  // volume -> masterVolume
  staged.masterVolume = (int16_t)((volU * 79) / 100) - 79;
  staged.balance = config.store.balance;
  staged.fader = config.store.tda7719.fader;
  staged.bassGain = config.store.bass;
  staged.bassCenterIdx = config.store.tda7719.bassCenterIdx;
  staged.bassQIdx = config.store.tda7719.bassQIdx;
  staged.bassDC = config.store.tda7719.bassDC;
  staged.midGain = config.store.middle;
  staged.midCenterIdx = config.store.tda7719.midCenterIdx;
  staged.midQIdx = config.store.tda7719.midQIdx;
  staged.trebleGain = config.store.trebble;
  staged.trebleCenterIdx = config.store.tda7719.trebleCenterIdx;
  staged.loudnessLevel = config.store.tda7719.loudnessLevel;
  staged.loudnessFreqIdx = config.store.tda7719.loudnessFreqIdx;
  staged.loudnessHighBoost = config.store.tda7719.loudnessHighBoost;
  staged.outL = config.store.tda7719.outL;
  staged.outR = config.store.tda7719.outR;
  staged.outSub = config.store.tda7719.outSub;
  staged.subFilterIdx = config.store.tda7719.subFilterIdx;
  staged.softStepTimeIdx = config.store.tda7719.softStepTimeIdx;
  staged.softMute = config.store.tda7719.softMute;
  for (int i=0;i<5;i++) {
    staged.inputs[i].enabled = config.store.tda7719.inputs[i].enabled;
    staged.inputs[i].gain = config.store.tda7719.inputs[i].gain;
    staged.inputs[i].routeToSub = config.store.tda7719.inputs[i].routeToSub;
    staged.inputInvert[i] = config.store.tda7719.inputInvert[i];
  }
  staged.dcDetect = config.store.tda7719.dcDetect;
  staged.levelMeterOut = config.store.tda7719.levelMeterOut;
  staged.winIn = config.store.tda7719.winIn;
  staged.schema_version = config.store.tda7719.schema_version;
  staged.crc32 = config.store.tda7719.crc32;
  // mark CRC zero until saved
  // leave staged.crc32 as copied from store (or zero)
  dirty = true;
}

// Save working copy to FS and map to global config (but do NOT apply to hardware)
bool tdaConfig_saveWorkingToGlobal()
{
  staged.crc32 = 0;
  staged.crc32 = crc32(&staged, sizeof(TDASettings));
  bool ok = saveSettingsToFS(staged);
  if (ok) {
    mapWorkingToGlobal(staged);
  }
  dirty = true;
  return ok;
}

// Clicks are handled via `tdaConfigUI_onBtnClick()` called from `onBtnClick()`
// in `controls.cpp`. No direct pin reads for ENC_BTNB are performed here.

// ctrls_on_loop hook - called frequently from controls.loop()
void ctrls_on_loop()
{
  uint32_t now = millis();

  // state machine
  switch (state) {
    case UI_IDLE:
    {
      // Idle: nothing to do; entering CONFIG is triggered by SRC long-press
      // handled in `controls.cpp` which will call `tdaConfigUI_enter()`.
      break;
    }
    case UI_LIST:
    {
      lastActivity = now;
      // rotation scrolls the list (only consume encoder while in list)
      {
        int delta = 0;
        noInterrupts();
        delta = cfg_enc_delta;
        cfg_enc_delta = 0;
        interrupts();
        if (delta != 0) {
          listIndex += delta;
          if (listIndex < 0) listIndex = 0;
          if (listIndex >= itemsCount) listIndex = itemsCount-1;
          dirty = true;
          TDA_UI_LOG("[TDA_UI] LIST delta=%d idx=%d\n", delta, listIndex);
        }
      }
      // Clicks are handled by `tdaConfigUI_onBtnClick()` invoked from
      // `controls.onBtnClick()`; it toggles edit state. SRC long-press saves
      // and exits (handled in `controls`).
      // inactivity
      if (now - lastActivity > INACTIVITY_MS) {
        exitConfigAndReturnToNormal(false);
      }
      break;
    }
    case UI_EDIT:
    {
      lastActivity = now;
      // rotation changes value (only consume encoder while editing)
      {
        int delta = 0;
        noInterrupts();
        delta = cfg_enc_delta;
        cfg_enc_delta = 0;
        interrupts();
        if (delta != 0 && editIndex >=0 && editIndex < itemsCount) {
          ItemDef &it = items[editIndex];
          int cur = getItemValue(it);
          int nv = cur + delta * it.step;
          if (nv < it.minv) nv = it.minv;
          if (nv > it.maxv) nv = it.maxv;
          setItemValue(it, nv);
          dirty = true;
          TDA_UI_LOG("[TDA_UI] EDIT idx=%d delta=%d new=%d\n", editIndex, delta, nv);
        }
      }
      // Clicks (encoder push) are handled externally and forwarded via
      // `tdaConfigUI_onBtnClick()`; nothing to poll here.
      // inactivity
      if (now - lastActivity > INACTIVITY_MS) {
        exitConfigAndReturnToNormal(false);
      }
      break;
    }
    case UI_SAVING:
    {
      lastActivity = now;
      int delta = 0;
      noInterrupts();
      delta = cfg_enc_delta;
      cfg_enc_delta = 0;
      interrupts();
      if (delta != 0) {
        // toggle selection on any rotation
        saveSelection = !saveSelection;
        dirty = true;
        TDA_UI_LOG("[TDA_UI] SAVING toggle sel=%d\n", saveSelection);
      }
      if (now - lastActivity > INACTIVITY_MS) {
        // timeout: exit without save
        exitConfigAndReturnToNormal(false);
      }
      break;
    }
    default:
      break;
  }
}

// Expose labels as combined "label + value" strings for renderer to print.
static char labelCache[64][128];

size_t tdaConfig_getItemCount()
{
  Serial.printf("[TDA_UI] getItemCount -> %d\n", itemsCount);
  return (size_t)itemsCount;
}

const char *tdaConfig_getItemLabel(size_t idx)
{
  if (idx >= (size_t)itemsCount) return "";
  // compose: base label + spacing + value
  int v = getItemValue(items[idx]);
  char valbuf[64];
  bool printed = false;
  if (items[idx].isInputField && items[idx].inputField==1) {
    snprintf(valbuf, sizeof(valbuf), "%ddB", v);
    printed = true;
  }
  if (!printed && strstr(items[idx].label, "(dB)")) {
    snprintf(valbuf, sizeof(valbuf), "%ddB", v);
    printed = true;
  }
  if (!printed) {
    if (((ValType)items[idx].vtype) == VT_BOOL) {
      snprintf(valbuf, sizeof(valbuf), v ? " Tak" : " Nie");
    } else {
      snprintf(valbuf, sizeof(valbuf), " %d", v);
    }
  }
  snprintf(labelCache[idx], sizeof(labelCache[idx]), "%s%s", items[idx].label, valbuf);
  TDA_UI_LOG("[TDA_UI] getItemLabel idx=%u -> %s\n", (unsigned)idx, labelCache[idx]);
  return labelCache[idx];
}

int tdaConfig_getItemValue(size_t idx)
{
  if (idx >= (size_t)itemsCount) return 0;
  return getItemValue(items[idx]);
}

void tdaConfig_setItemValue(size_t idx, int v)
{
  if (idx >= (size_t)itemsCount) return;
  setItemValue(items[idx], v);
  dirty = true;
}

int tdaConfig_getSelectedIndex()
{
  return listIndex;
}

bool tdaConfig_save()
{
  staged.crc32 = 0;
  staged.crc32 = crc32(&staged, sizeof(TDASettings));
  bool ok = saveSettingsToFS(staged);
  if (ok) applySettingsToHardware();
  dirty = true;
  return ok;
}

void tdaConfig_discard()
{
  if (!loadSettingsFromFS(staged)) {
    staged = defaultSettings();
  }
  dirty = true;
}

bool tdaConfig_apply()
{
  applySettingsToHardware();
  dirty = true;
  return true;
}

// Dirty flag consumer
bool tdaConfig_consumeDirty()
{
  if (!dirty) return false;
  dirty = false;
  return true;
}

int16_t volU = config.store.volume; // Deklaracja i inicjalizacja volU na początku funkcji

// Funkcja do sprawdzania inactivity
void pollInactivity() {
  if (tdaConfigUI_isActive() && millis() - lastActivity > INACTIVITY_MS) {
    Serial.printf("[TDA_UI] Inactivity timeout at %lu, exiting CONFIG_MODE without save\n", millis());
    exitConfigAndReturnToNormal(false);
  }
}

void tdaConfigUI_resetActivity() {
  lastActivity = millis();
}

