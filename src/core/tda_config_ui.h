#ifndef TDA_CONFIG_UI_H
#define TDA_CONFIG_UI_H

#include <cstddef>

typedef int TDA_CONFIG_TEST;

// Public API for TDA7719 configuration UI (modal)
void tdaConfigUI_init();

// Called from controls.loop() every tick. Handles config-mode input/state.
void ctrls_on_loop();

// Simple API to query active state (controls will manage UI mode changes)
bool tdaConfigUI_isActive();

// Called by controls to forward simple button click events when CONFIG mode active
void tdaConfigUI_onBtnClick(int id);

// Helpers to programmatically enter/exit config mode (no UI code inside)
void tdaConfigUI_enter();
void tdaConfigUI_exit_save();
void tdaConfigUI_exit_discard();
void exitConfigAndReturnToNormal(bool save);
void tdaConfigUI_resetActivity();

// Accessors used by existing playlist renderer to switch data source.
size_t tdaConfig_getItemCount();
const char *tdaConfig_getItemLabel(size_t idx);

// Get/set item numeric value (for controls to modify staged settings)
int tdaConfig_getItemValue(size_t idx);
void tdaConfig_setItemValue(size_t idx, int v);

// Return currently selected item index (0-based) in staged list
int tdaConfig_getSelectedIndex();

// Persist / discard / apply staged settings
bool tdaConfig_save();
void tdaConfig_discard();
bool tdaConfig_apply();

// Dirty flag: set when data changed and UI should redraw. Controls will consume it.
bool tdaConfig_consumeDirty();
// Persistence and mapping helpers
bool tdaConfig_loadToGlobal();
void tdaConfig_loadWorkingFromGlobal();
bool tdaConfig_saveWorkingToGlobal();

// Additional accessors for renderer/UI modes
bool tdaConfigUI_isEditMode();
// Is the UI currently showing save confirmation
bool tdaConfigUI_isExitConfirm();
// Current selection for save confirm (true=TAK, false=NIE)
bool tdaConfigUI_getExitSave();
// Provide base label (without value) and a formatted value string for edit view
const char *tdaConfig_getItemBaseLabel(size_t idx);
const char *tdaConfig_getItemValueStr(size_t idx);

#endif // TDA_CONFIG_UI_H
