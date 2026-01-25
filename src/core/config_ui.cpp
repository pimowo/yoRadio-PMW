// config_ui.cpp - General configuration UI implementation
// Updated
#include "config_ui.h"
#include "config.h"
#include "display.h"
#include "player.h"
#include "../yoEncoder/yoEncoder.h"
#include "options.h"
#include "tda_config_ui.h"

extern Config config;

// Configuration items
const ConfigItem configItems[] = {
  {"Volume", CIT_UINT8, &config.store.volume, 0, 21, 1},
  {"Brightness", CIT_UINT8, &config.store.brightness, 0, 100, 5},
  {"Contrast", CIT_UINT8, &config.store.contrast, 0, 100, 5},
  {"Balance", CIT_INT8, &config.store.balance, -16, 16, 1},
  {"Treble", CIT_INT8, &config.store.trebble, -16, 16, 1},
  {"Middle", CIT_INT8, &config.store.middle, -16, 16, 1},
  {"Bass", CIT_INT8, &config.store.bass, -16, 16, 1},
  {"Vol Steps", CIT_UINT8, &config.store.volsteps, 1, 10, 1},
  {"Enc Acc", CIT_UINT16, &config.store.encacc, 0, 100, 5},
  {"Screensaver", CIT_BOOL, &config.store.screensaverEnabled, 0, 1, 1},
  {"Flip Screen", CIT_BOOL, &config.store.flipscreen, 0, 1, 1},
  {"Invert Disp", CIT_BOOL, &config.store.invertdisplay, 0, 1, 1},
  {"Num Playlist", CIT_BOOL, &config.store.numplaylist, 0, 1, 1},
  {"VU Meter", CIT_BOOL, &config.store.vumeter, 0, 1, 1},
  {"Audio Info", CIT_BOOL, &config.store.audioinfo, 0, 1, 1},
  {"Telnet", CIT_BOOL, &config.store.telnet, 0, 1, 1},
  {"Watchdog", CIT_BOOL, &config.store.watchdog, 0, 1, 1},
  {"Nameday", CIT_BOOL, &config.store.nameday, 0, 1, 1},
  {"TDA7719 settings", CIT_BOOL, nullptr, 0, 1, 1},
};

const int numConfigItems = sizeof(configItems) / sizeof(configItems[0]);

// State
static bool active = false;
static int selectedIndex = 0;
static bool inSubMenu = false;
static bool exitConfirm = false;
static bool exitSave = false; // true for YES, false for NO
static bool dirty = false;

// Forward declarations
static void updateDisplay();

void configUI_init() {
  active = false;
  selectedIndex = 0;
  inSubMenu = false;
}

bool configUI_isActive() {
  return active;
}

bool configUI_isExitConfirm() {
  return exitConfirm;
}

void configUI_enter() {
  if (active) return;
  active = true;
  selectedIndex = 0;
  inSubMenu = false;
  updateDisplay();
}

void configUI_exit(bool save) {
  if (save && dirty) {
    // Save all modified values
    for (int i = 0; i < numConfigItems; i++) {
      const ConfigItem &item = configItems[i];
      if (item.valuePtr == nullptr) continue;
      switch (item.type) {
        case CIT_UINT8: config.saveValue((uint8_t*)item.valuePtr, *(uint8_t*)item.valuePtr, false); break;
        case CIT_INT8: config.saveValue((int8_t*)item.valuePtr, *(int8_t*)item.valuePtr, false); break;
        case CIT_BOOL: config.saveValue((bool*)item.valuePtr, *(bool*)item.valuePtr, false); break;
        case CIT_UINT16: config.saveValue((uint16_t*)item.valuePtr, *(uint16_t*)item.valuePtr, false); break;
        default: break;
      }
    }
    EEPROM.commit();
    dirty = false;
  }
  active = false;
  selectedIndex = 0;
  inSubMenu = false;
  exitConfirm = false;
  display.putRequest(NEWMODE, PLAYER);
}

bool configUI_getExitSave() {
  return exitSave;
}

void configUI_onLongPress() {
  if (!active) return;
  if (exitConfirm) {
    // Already in confirm, do nothing or reset
    return;
  }
  exitConfirm = true;
  exitSave = false; // default to NO
  updateDisplay();
}

void configUI_processEncoder(int delta, bool clicked) {
  if (!active) return;
  Serial.printf("[CFGUI] processEncoder delta=%d clicked=%d selected=%d inSub=%d\n", delta, clicked, selectedIndex, inSubMenu);

  if (exitConfirm) {
    // Navigate YES/NO
    if (delta != 0) {
      exitSave = !exitSave;
    }
    if (clicked) {
      configUI_exit(exitSave);
    }
    updateDisplay();
    return;
  }

  if (inSubMenu) {
    // Adjust value
    const ConfigItem &item = configItems[selectedIndex];
    switch (item.type) {
      case CIT_UINT8: {
        uint8_t *val = (uint8_t*)item.valuePtr;
        *val = constrain(*val + delta * item.step, item.minVal, item.maxVal);
        dirty = true;
        break;
      }
      case CIT_INT8: {
        int8_t *val = (int8_t*)item.valuePtr;
        *val = constrain(*val + delta * item.step, item.minVal, item.maxVal);
        dirty = true;
        break;
      }
      case CIT_BOOL: {
        bool *val = (bool*)item.valuePtr;
        if (delta != 0) *val = !*val;
        dirty = true;
        break;
      }
      case CIT_UINT16: {
        uint16_t *val = (uint16_t*)item.valuePtr;
        *val = constrain(*val + delta * item.step, item.minVal, item.maxVal);
        dirty = true;
        break;
      }
      default: break;
    }
    updateDisplay();
  } else {
    // Navigate list
    selectedIndex = constrain(selectedIndex + delta, 0, numConfigItems - 1);
    updateDisplay();
  }

  if (clicked) {
    if (inSubMenu) {
      // Exit submenu
      inSubMenu = false;
    } else {
      // If the selected item is the TDA entry, open the TDA modal
      if (selectedIndex == numConfigItems - 1) {
        // exit general config and enter TDA config
        active = false;
        inSubMenu = false;
        exitConfirm = false;
        tdaConfigUI_enter();
        return;
      }
      // Enter submenu
      inSubMenu = true;
    }
    updateDisplay();
  }
}

int configUI_getSelectedIndex() {
  return selectedIndex;
}

const char *configUI_getItemLabel(int index) {
  if (index < 0 || index >= numConfigItems) return "";
  return configItems[index].label;
}

const char *configUI_getItemValue(int index) {
  static char buf[32];
  if (index < 0 || index >= numConfigItems) return "";
  const ConfigItem &item = configItems[index];
  if (item.valuePtr == nullptr) return "";
  switch (item.type) {
    case CIT_UINT8: sprintf(buf, "%d", *(uint8_t*)item.valuePtr); break;
    case CIT_INT8: sprintf(buf, "%d", *(int8_t*)item.valuePtr); break;
    case CIT_BOOL: sprintf(buf, "%s", *(bool*)item.valuePtr ? "ON" : "OFF"); break;
    case CIT_UINT16: sprintf(buf, "%d", *(uint16_t*)item.valuePtr); break;
    default: strcpy(buf, ""); break;
  }
  return buf;
}

static void updateDisplay() {
  if (exitConfirm) {
    // Special display for exit confirmation
    // For now, just use playlist mode with special items
    display.putRequest(DRAWPLAYLIST, 0);
    return;
  }
  // Use playlist mode to display config items
  display.putRequest(DRAWPLAYLIST, selectedIndex + 1);
}

bool configUI_consumeDirty() {
  if (!dirty) return false;
  dirty = false;
  return true;
}

// reset activity timestamp (used by controls to prevent auto-exit)
static uint32_t _config_ui_lastActivity = 0;
void configUI_resetActivity() {
  _config_ui_lastActivity = millis();
}

// convenience to exit and save
void configUI_exit_save() {
  configUI_exit(true);
}