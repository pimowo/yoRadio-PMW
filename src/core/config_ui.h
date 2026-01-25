// config_ui.h - General configuration UI for yoRadio settings
#ifndef CONFIG_UI_H
#define CONFIG_UI_H

#include <stdint.h>

// Configuration item types
enum ConfigItemType {
  CIT_UINT8,
  CIT_INT8,
  CIT_BOOL,
  CIT_UINT16,
  CIT_STRING, // for future use
};

// Configuration item definition
struct ConfigItem {
  const char *label;
  ConfigItemType type;
  void *valuePtr; // pointer to the value in config.store
  int minVal;
  int maxVal;
  int step;
};

// External declarations
extern const ConfigItem configItems[];
extern const int numConfigItems;

// UI functions
void configUI_init();
bool configUI_isActive();
bool configUI_isExitConfirm();
bool configUI_getExitSave();
void configUI_processEncoder(int delta, bool clicked);
void configUI_enter();
void configUI_exit(bool save);
void configUI_onLongPress();
int configUI_getSelectedIndex();
const char *configUI_getItemLabel(int index);
const char *configUI_getItemValue(int index);
bool configUI_consumeDirty();
// Helper API used by controls
void configUI_resetActivity();
void configUI_exit_save();

#endif