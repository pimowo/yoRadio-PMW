TDA7719 CONFIG: data-model + mapping + persistence

Summary
- Implemented modal CONFIG mode as data-provider only (no UI rendering changes).
- Added full working copy `TDASettings` and mapping to `config.store.tda7719`.
- Persisted working copy to SPIFFS (`/data/tda7719.bin`) with CRC32 and atomic save.
- Implemented APIs in `src/core/tda_config_ui.*`:
  - Data provider: `tdaConfig_getItemCount()`, `tdaConfig_getItemLabel()`, `tdaConfig_getItemValue()`, `tdaConfig_setItemValue()`
  - Persistence/mapping: `tdaConfig_loadToGlobal()`, `tdaConfig_loadWorkingFromGlobal()`, `tdaConfig_saveWorkingToGlobal()`
  - Save/apply: `tdaConfig_save()`, `tdaConfig_discard()`, `tdaConfig_apply()`
  - Dirty flag: `tdaConfig_consumeDirty()`
- All UI rendering continues to use `PlayListWidget` without changes; CONFIG only swaps data source.
- `tdaDriver_apply()` remains a weak stub (no I2C/hardware access). 

Files changed (high level)
- src/core/tda_config_ui.h/.cpp  (data-provider, mapping, persistence)
- src/displays/widgets/widgets.cpp  (`PlayListWidget::_fillPlMenu` switched data source when CONFIG active)
- src/core/controls.cpp  (display takeover + redraw requests when CONFIG active)
- src/core/config.h  (added `tda7719` struct to `config.store`)

Testing notes
- Build and run on device (no physical TDA required).
- Enter CONFIG (hold `SRC_BTN` 3s), edit values, save with long `SRC_BTN`; verify `/data/tda7719.bin` updates and `config.store.tda7719` fields updated.

Suggested git commands (run locally):
```bash
git add src/core/tda_config_ui.* src/core/config.h src/displays/widgets/widgets.cpp src/core/controls.cpp TDA7719_CONFIG_CHANGELOG.md
git commit -m "tda7719: add config data-model, full mapping and SPIFFS persistence; UI uses existing playlist renderer"
git push origin <your-branch>
# then create a PR from your branch
```

Notes
- No UI/renderer/layout changes were made.
- `tdaDriver_apply()` is intentionally a stub to allow later hardware integration.
