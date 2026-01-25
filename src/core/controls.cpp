// v0.9.670 // Módosítva. "vol_step"
#include "Arduino.h"
#include "options.h"
#include "controls.h"
#include "config.h"
#include "bluetooth_uart.h"
#include "player.h"
#include "display.h"
#include "network.h"
#include "netserver.h"
#include "../pluginsManager/pluginsManager.h"
#include "config_ui.h"

// Dodano funkcję do resetowania inactivity
typedef int TDA_CONFIG_TEST;

// Forward declarations for tdaConfigUI
void tdaConfigUI_init();
void ctrls_on_loop();
bool tdaConfigUI_isActive();
void tdaConfigUI_onBtnClick(int id);
void tdaConfigUI_enter();
void tdaConfigUI_exit_save();
void tdaConfigUI_exit_discard();
void exitConfigAndReturnToNormal(bool save);
void tdaConfigUI_resetActivity();
size_t tdaConfig_getItemCount();
const char *tdaConfig_getItemLabel(size_t idx);
int tdaConfig_getItemValue(size_t idx);
void tdaConfig_setItemValue(size_t idx, int v);
int tdaConfig_getSelectedIndex();
bool tdaConfig_save();
void tdaConfig_discard();
bool tdaConfig_apply();
bool tdaConfig_consumeDirty();
bool tdaConfig_loadToGlobal();
void tdaConfig_loadWorkingFromGlobal();
bool tdaConfig_saveWorkingToGlobal();

TDA_CONFIG_TEST test;

extern HardwareSerial btSerial;  // Dodano dla btSerial

long encOldPosition = 0;
long enc2OldPosition = 0;
int lpId = -1;
// forwarded encoder/button deltas for CONFIG mode
volatile int8_t cfg_enc_delta = 0;
volatile bool cfg_enc_click = false;

// pending non-blocking playlist draw state (set when requesting NEWMODE->STATIONS)
static bool pendingPlistDraw = false;
static uint16_t pendingPlistItem = 0;
static uint8_t pendingPlistSource = 0;
static bool pendingControlsEvent = false;
static bool pendingControlsToRight = false;
static int pendingControlsVolDelta = 0;


// SRC_BTN long-press detection (edge-trigger + millis)
static bool _src_wasDown = false;
static uint32_t _src_pressedAt = 0;
static bool _src_triggered = false;
static bool _src_longHandled = false;  // Dodano dla jednorazowego long-press
static uint32_t _src_lastChange = 0;   // Dodano dla debounce 50ms

#if DSP_MODEL == DSP_DUMMY
#define DUMMYDISPLAY
#endif

#define ISPUSHBUTTONS BTN_LEFT != 255 || BTN_CENTER != 255 || BTN_RIGHT != 255 || ENC_BTNB != 255 || BTN_UP != 255 || BTN_DOWN != 255 || ENC2_BTNB != 255 || BTN_MODE != 255 || SRC_BTN != 255
#if ISPUSHBUTTONS
#include "../OneButton/OneButton.h"
OneButton button[]{{BTN_LEFT, true, BTN_INTERNALPULLUP}, {BTN_CENTER, true, BTN_INTERNALPULLUP}, {BTN_RIGHT, true, BTN_INTERNALPULLUP}, {ENC_BTNB, true, ENC_INTERNALPULLUP}, {BTN_UP, true, BTN_INTERNALPULLUP}, {BTN_DOWN, true, BTN_INTERNALPULLUP}, {ENC2_BTNB, true, ENC2_INTERNALPULLUP}, {BTN_MODE, true, BTN_INTERNALPULLUP}, {SRC_BTN, true, true}};
constexpr uint8_t nrOfButtons = sizeof(button) / sizeof(button[0]);
#endif

#if ENC_HALFQUARD == false
#define ENCODER_STEPS 4
#elif ENC_HALFQUARD == true
#define ENCODER_STEPS 2
#elif ENC_HALFQUARD == 255
#define ENCODER_STEPS 1
#endif
#if ENC2_HALFQUARD == false
#define ENCODER2_STEPS 4
#elif ENC2_HALFQUARD == true
#define ENCODER2_STEPS 2
#elif ENC2_HALFQUARD == 255
#define ENCODER2_STEPS 1
#endif

#if (ENC_BTNL != 255 && ENC_BTNR != 255) || (ENC2_BTNL != 255 && ENC2_BTNR != 255)
#include "../yoEncoder/yoEncoder.h"
#if (ENC_BTNL != 255 && ENC_BTNR != 255)
yoEncoder encoder = yoEncoder(ENC_BTNL, ENC_BTNR, ENCODER_STEPS, ENC_INTERNALPULLUP);
#endif
#if (ENC2_BTNL != 255 && ENC2_BTNR != 255)
yoEncoder encoder2 = yoEncoder(ENC2_BTNL, ENC2_BTNR, ENCODER2_STEPS, ENC2_INTERNALPULLUP);
#endif
#endif

#if (TS_MODEL != TS_MODEL_UNDEFINED) && (DSP_MODEL != DSP_DUMMY)
#include "touchscreen.h"
TouchScreen touchscreen;
#endif

#if IR_PIN != 255
#include <assert.h>

#include "../IRremoteESP8266/IRrecv.h"
#include "../IRremoteESP8266/IRremoteESP8266.h"
#include "../IRremoteESP8266/IRac.h"
#include "../IRremoteESP8266/IRtext.h"
#include "../IRremoteESP8266/IRutils.h"
uint8_t irVolRepeat = 0;
// const uint16_t kCaptureBufferSize = 1024;
const uint16_t kMinUnknownSize = 12;
#define LEGACY_TIMING_INFO false

IRrecv irrecv(IR_PIN, IR_BUFSIZE, IR_TIMEOUT, true);
decode_results irResults;
#endif

#if ENC_BTNL != 255
void IRAM_ATTR readEncoderISR()
{
  if ((SDC_CS == 255 && display.mode() == LOST) || display.mode() == UPDATING)
    return;
  encoder.readEncoder_ISR();
}
#endif
#if ENC2_BTNL != 255
void IRAM_ATTR readEncoder2ISR()
{
  if ((SDC_CS == 255 && display.mode() == LOST) || display.mode() == UPDATING)
    return;
  encoder2.readEncoder_ISR();
}
#endif

void initControls()
{

#if ENC_BTNL != 255
  encoder.begin();
  encoder.setup(readEncoderISR);
  encoder.setBoundaries(0, 254, true);
  encoder.setAcceleration(config.store.encacc);
#endif
#if ENC2_BTNL != 255
  encoder2.begin();
  encoder2.setup(readEncoder2ISR);
  encoder2.setBoundaries(0, 254, true);
  encoder2.setAcceleration(config.store.encacc);
#endif

#if ISPUSHBUTTONS
  for (int i = 0; i < nrOfButtons; i++)
  {
    // Skip if pin not present or if this is the SRC button — SRC is handled manually in loopControls()
    if (i == 8 || (i == 0 && BTN_LEFT == 255) || (i == 1 && BTN_CENTER == 255) || (i == 2 && BTN_RIGHT == 255) || (i == 3 && ENC_BTNB == 255) || (i == 4 && BTN_UP == 255) || (i == 5 && BTN_DOWN == 255) || (i == 6 && ENC2_BTNB == 255) || (i == 7 && BTN_MODE == 255) || (i == 8 && SRC_BTN == 255))
      continue;
    button[i].attachClick([](void *p)
                          { onBtnClick((int)p); }, (void *)i);
    button[i].attachDoubleClick([](void *p)
                                { onBtnDoubleClick((int)p); }, (void *)i);
    button[i].attachLongPressStart([](void *p)
                                   { onBtnLongPressStart((int)p); }, (void *)i);
    button[i].attachLongPressStop([](void *p)
                                  { onBtnLongPressStop((int)p); }, (void *)i);
    button[i].setClickTicks(BTN_CLICK_TICKS);
    button[i].setPressTicks(BTN_PRESS_TICKS);
  }
#endif
#if (TS_MODEL != TS_MODEL_UNDEFINED) && (DSP_MODEL != DSP_DUMMY)
  touchscreen.init(display.width(), display.height());
#endif
#if IR_PIN != 255
  pinMode(IR_PIN, INPUT);
  assert(irutils::lowLevelSanityCheck() == 0);
#if DECODE_HASH
  irrecv.setUnknownThreshold(kMinUnknownSize);
#endif // DECODE_HASH
  irrecv.setTolerance(config.store.irtlp);
  irrecv.enableIRIn();
#endif // IR_PIN!=255
}

void loopControls()
{
  if (display.mode() == UPDATING || display.mode() == SDCHANGE)
    return;
  if (SDC_CS == 255 && display.mode() == LOST)
    return;
  if (ctrls_on_loop)
    ctrls_on_loop();

  // pending non-blocking playlist draw state (set when requesting NEWMODE->STATIONS)

  // If we previously requested a playlist draw after switching to STATIONS, perform it now
  if (pendingPlistDraw && display.mode() == STATIONS) {
    display.currentPlItem = pendingPlistItem;
    display.putRequest(DRAWPLAYLIST, pendingPlistItem);
    pendingPlistDraw = false;
    pendingPlistSource = 0;
    if (pendingControlsEvent) {
      controlsEvent(pendingControlsToRight, pendingControlsVolDelta);
      pendingControlsEvent = false;
      pendingControlsVolDelta = 0;
    }
  }

  // Do not entirely return when config UI is active — allow encoder loops to run.
  // Normal button polling and SRC handling will be skipped later when needed.

  // SRC_BTN long-press handling: edge-triggered, non-blocking, 3000 ms threshold, debounce 50ms
  if (SRC_BTN != 255) {
    bool curDown = (digitalRead(SRC_BTN) == LOW);
    uint32_t now = millis();
    if (curDown != _src_wasDown) {
      if (now - _src_lastChange > 50) {  // Debounce 50ms
        _src_wasDown = curDown;
        _src_lastChange = now;
        if (curDown) {
          // press start
          _src_pressedAt = now;
          _src_triggered = false;
          _src_longHandled = false;
          Serial.printf("[CTRL] SRC press start at %lu\n", _src_pressedAt);
          if (configUI_isActive()) configUI_resetActivity();  // Reset inactivity
        } else {
          // release
          uint32_t duration = now - _src_pressedAt;
          if (duration >= 50 && duration < 3000 && !_src_longHandled) {
            // short press
            Serial.printf("[CTRL] SRC manual short-press -> invoking onBtnClick at %lu\n", now);
            onBtnClick(EVT_BTNSOURCE);
            Serial.printf("[CTRL] SRC short press completed\n");
          }
          _src_pressedAt = 0;
          _src_triggered = false;
          _src_longHandled = false;
        }
      }
    }
    if (curDown && !_src_longHandled && _src_pressedAt != 0 && now - _src_pressedAt >= 3000) {
      // long press detected (jednorazowo)
      if (!tdaConfigUI_isActive()) {
        // Enter TDA configuration modal immediately (per spec: only TDA settings available)
        tdaConfigUI_enter();
        Serial.printf("[CTRL] SRC entered TDA_CONFIG_UI at %lu\n", now);
      } else {
        // If already in TDA UI, open save-confirm (do not directly save)
        Serial.printf("[CTRL] SRC manual TDA exit_save requested at %lu\n", now);
        tdaConfigUI_exit_save();
        Serial.printf("[CTRL] SRC requested TDA save-confirm at %lu\n", now);
      }
      _src_longHandled = true;
      if (tdaConfigUI_isActive()) tdaConfigUI_resetActivity();  // Reset inactivity
    }
  }

  // Manage display mode transitions for CONFIG modal: when config becomes active
  // controls are responsible for UI takeover (lock display, hide clock, switch to STATIONS)
  static bool prevCfgActive = false;
  bool curCfgActive = tdaConfigUI_isActive();
  if (curCfgActive && !prevCfgActive) {
    // entering config: stop playback, lock audio pipeline and take over display
    player.sendCommand({PR_STOP, 0});
    player.lockOutput = true;
    display.hideClock();
    display.putRequest(NEWMODE, STATIONS);
    // Non-blocking: request initial playlist draw once display reaches STATIONS
    {
      int sel = tdaConfig_getSelectedIndex();
      pendingPlistDraw = true;
      pendingPlistSource = 1;
      pendingPlistItem = (uint16_t)(sel + 1);
    }
  }
  if (!curCfgActive && prevCfgActive) {
    // exiting config: restore display and audio pipeline
    Serial.printf("[CTRL] Exiting CONFIG_MODE, switching to PLAYER\n");
    display.showClock();
    display.putRequest(NEWMODE, PLAYER);
    display.putRequest(NEWTITLE);
    player.lockOutput = false;
    // drain any residual encoder ticks so they don't affect global controls
#if (ENC_BTNL != 255)
    {
      int drained = 0;
      while (encoder.encoderChanged() != 0) drained++;
      noInterrupts();
      cfg_enc_delta = 0;
      cfg_enc_click = false;
      interrupts();
      Serial.printf("[CTRL] drained encoder ticks=%d at %lu\n", drained, millis());
    }
#endif
  }
  prevCfgActive = curCfgActive;

  // If configuration data changed, request redraw of playlist renderer
  if (tdaConfig_consumeDirty()) {
    if (tdaConfigUI_isActive()) {
      int sel = tdaConfig_getSelectedIndex();
      display.currentPlItem = (uint16_t)(sel + 1);
      display.putRequest(DRAWPLAYLIST, sel + 1);
    } else {
      display.putRequest(DRAWPLAYLIST, 0);
    }
  }

  // Manage display mode transitions for CONFIG_UI modal: when config becomes active
  // controls are responsible for UI takeover (lock display, hide clock, switch to STATIONS)
  static bool prevCfgUIActive = false;
  bool curCfgUIActive = configUI_isActive();
  if (curCfgUIActive && !prevCfgUIActive) {
    // entering config: stop playback, lock audio pipeline and take over display
    player.sendCommand({PR_STOP, 0});
    player.lockOutput = true;
    display.hideClock();
    display.putRequest(NEWMODE, STATIONS);
    // Non-blocking: request initial playlist draw once display reaches STATIONS
    {
      int sel = configUI_getSelectedIndex();
      pendingPlistDraw = true;
      pendingPlistSource = 2;
      pendingPlistItem = (uint16_t)(sel + 1);
    }
  }
  if (!curCfgUIActive && prevCfgUIActive) {
    // exiting config: restore display and audio pipeline
    Serial.printf("[CTRL] Exiting CONFIG_UI mode, switching to PLAYER\n");
    display.showClock();
    display.putRequest(NEWMODE, PLAYER);
    display.putRequest(NEWTITLE);
    player.lockOutput = false;
    // drain any residual encoder ticks so they don't affect global controls
#if (ENC_BTNL != 255)
    {
      int drained = 0;
      while (encoder.encoderChanged() != 0) drained++;
      noInterrupts();
      cfg_enc_delta = 0;
      cfg_enc_click = false;
      interrupts();
      Serial.printf("[CTRL] drained encoder ticks=%d at %lu\n", drained, millis());
    }
#endif
  }
  prevCfgUIActive = curCfgUIActive;

  // If configUI data changed, request redraw of playlist renderer
  if (configUI_consumeDirty()) {
    if (configUI_isActive()) {
      int sel = configUI_getSelectedIndex();
      display.currentPlItem = (uint16_t)(sel + 1);
      display.putRequest(DRAWPLAYLIST, sel + 1);
    } else {
      display.putRequest(DRAWPLAYLIST, 0);
    }
  }
#if ENC_BTNL != 255
  encoder1Loop();
#endif
#if ENC2_BTNL != 255
  encoder2Loop();
#endif
#if ISPUSHBUTTONS
  if (!configUI_isActive()) {
    for (unsigned i = 0; i < nrOfButtons; i++)
    {
      if ((i == 0 && BTN_LEFT == 255) || (i == 1 && BTN_CENTER == 255) || (i == 2 && BTN_RIGHT == 255) || (i == 3 && ENC_BTNB == 255) || (i == 4 && BTN_UP == 255) || (i == 5 && BTN_DOWN == 255) || (i == 6 && ENC2_BTNB == 255))
        continue;
      button[i].tick();
      if (lpId >= 0)
      {
        if (DSP_MODEL == DSP_DUMMY && (lpId == 4 || lpId == 5))
          continue;
        onBtnDuringLongPress(lpId);
      }
    }
  }
#endif
#if IR_PIN != 255
  irLoop();
#endif
#if (TS_MODEL != TS_MODEL_UNDEFINED) && (DSP_MODEL != DSP_DUMMY)
  if (network.status == CONNECTED || network.status == SDREADY)
    touchscreen.loop();
#endif
}
#if ENC_BTNL != 255 || ENC2_BTNL != 255
void encodersLoop(yoEncoder *enc, bool first)
{
  // Allow encoder handling while config UIs are active even if network is disconnected
  if (!configUI_isActive() && !tdaConfigUI_isActive() && network.status != CONNECTED && network.status != SDREADY)
    return;
  if (display.mode() == LOST)
    return;
  int8_t encoderDelta = enc->encoderChanged();
  if (encoderDelta != 0) {
    Serial.printf("[CTRL] enc raw delta=%d first=%d net=%d mode=%d cfgUI=%d tdaCfg=%d\n", encoderDelta, first, network.status, display.mode(), configUI_isActive(), tdaConfigUI_isActive());
  }
  if (encoderDelta != 0 && configUI_isActive())
  {
    configUI_processEncoder(encoderDelta, false);
    return;
  }
  if (encoderDelta != 0 && tdaConfigUI_isActive())
  {
    cfg_enc_delta = encoderDelta;
    tdaConfigUI_resetActivity();  // Reset inactivity przy rotate
    Serial.printf("[CTRL] forward encoder delta=%d\n", encoderDelta);
    return;
  }
  if (encoderDelta != 0)
  {
    uint8_t encBtnState = digitalRead(first ? ENC_BTNB : ENC2_BTNB);
#if defined(DUMMYDISPLAY)
    first = first ? (first && encBtnState) : (!encBtnState);
    if (first)
    {
      int nv = config.store.volume + encoderDelta;
      if (nv < 0)
        nv = 0;
      if (nv > 254)
        nv = 254;
      player.setVol((uint8_t)nv);
    }
    else
    {
      if (encoderDelta > 0)
        player.next();
      else
        player.prev();
    }
#else
    if (first)
    {
      controlsEvent(encoderDelta > 0, encoderDelta);
    }
    else
    {
      if (encBtnState == HIGH && display.mode() == PLAYER)
      {
        if (config.store.skipPlaylistUpDown)
        {
          if (encoderDelta > 0)
            player.next();
          else
            player.prev();
          return;
        }
        display.putRequest(NEWMODE, STATIONS);
        // Non-blocking: remember to perform controlsEvent once STATIONS is active
        pendingPlistDraw = true;
        pendingPlistSource = 0; // generic
        // schedule controlsEvent for when STATIONS arrives
        pendingControlsEvent = true;
        pendingControlsToRight = (encoderDelta > 0);
        pendingControlsVolDelta = encoderDelta;
        return;
      }
      controlsEvent(encoderDelta > 0, encoderDelta);
    }
#endif
  }
}
#endif

#if ENC_BTNL != 255
void encoder1Loop()
{
  encodersLoop(&encoder, true);
}
#endif

#if ENC2_BTNL != 255
void encoder2Loop()
{
  encodersLoop(&encoder2, false);
}
#endif

#if IR_PIN != 255
void irBlink()
{
  if (REAL_LEDBUILTIN == 255)
    return;
  if (player.status() == STOPPED)
  {
    for (uint8_t i = 0; i < 7; i++)
    {
      digitalWrite(REAL_LEDBUILTIN, !digitalRead(REAL_LEDBUILTIN));
      delay(100);
    }
  }
}

void irNumber(uint8_t num)
{
  uint16_t s;
  if (display.numOfNextStation == 0 && num == 0)
    return;
  display.putRequest(NEWMODE, NUMBERS);
  if (display.numOfNextStation > UINT16_MAX / 10)
    return;
  s = display.numOfNextStation * 10 + num;
  if (s > config.playlistLength())
    return;
  display.numOfNextStation = s;
  display.putRequest(NEXTSTATION, s);
}

void irLoop()
{
  if (irrecv.decode(&irResults))
  {
    if (irResults.value < 256)
      return;
    if (netserver.irRecordEnable)
    {
      Serial.print(resultToHumanReadableBasic(&irResults));
      Serial.println("--------------------------");
      config.ircodes.irVals[config.irindex][config.irchck] = irResults.value;
      netserver.irToWs(typeToString(irResults.decode_type, irResults.repeat).c_str(), irResults.value);
      return;
    }
    if (!irResults.repeat /* && irResults.command!=0*/)
    {
      irVolRepeat = 0;
    }
    switch (irVolRepeat)
    {
    case 1:
    {
      controlsEvent(display.mode() == STATIONS ? false : true);
      break;
    }
    case 2:
    {
      controlsEvent(display.mode() == STATIONS ? true : false);
      break;
    }
    }
    for (int target = 0; target < 17; target++)
    {
      for (int j = 0; j < 3; j++)
      {
        if (config.ircodes.irVals[target][j] == irResults.value)
        {
          if (network.status != CONNECTED && network.status != SDREADY && target != IR_AST)
            return;
          if (target != IR_AST && display.mode() == LOST)
            return;
          if (display.mode() == SCREENSAVER || display.mode() == SCREENBLANK)
          {
            display.putRequest(NEWMODE, PLAYER);
            return;
          }
          switch (target)
          {
          case IR_PLAY:
            static bool pendingPlistDraw = false; // pending non-blocking playlist draw state (set when requesting NEWMODE->STATIONS)
            static uint16_t pendingPlistItem = 0;
            static uint8_t pendingPlistSource = 0;
            static bool pendingControlsEvent = false;
            static bool pendingControlsToRight = false;
            static int pendingControlsVolDelta = 0;
              break;
            }
            onBtnClick(1);
            break;
          }
          case IR_PREV:
          {
            player.prev();
            break;
          }
          case IR_NEXT:
          {
            player.next();
            break;
          }
          case IR_UP:
          {
            controlsEvent(display.mode() == STATIONS ? false : true);
            irVolRepeat = 1;
            break;
          }
          case IR_DOWN:
          {
            controlsEvent(display.mode() == STATIONS ? true : false);
            irVolRepeat = 2;
            break;
          }
          case IR_HASH:
          {
            if (display.mode() == NUMBERS)
            {
              display.putRequest(NEWMODE, PLAYER);
              display.numOfNextStation = 0;
              break;
            }
            display.putRequest(NEWMODE, display.mode() == PLAYER ? STATIONS : PLAYER);
            break;
          }
          case IR_0:
          {
            irNumber(0);
            break;
          }
          case IR_1:
          {
            irNumber(1);
            break;
          }
          case IR_2:
          {
            irNumber(2);
            break;
          }
          case IR_3:
          {
            irNumber(3);
            break;
          }
          case IR_4:
          {
            irNumber(4);
            break;
          }
          case IR_5:
          {
            irNumber(5);
            break;
          }
          case IR_6:
          {
            irNumber(6);
            break;
          }
          case IR_7:
          {
            irNumber(7);
            break;
          }
          case IR_8:
          {
            irNumber(8);
            break;
          }
          case IR_9:
          {
            irNumber(9);
            break;
          }
          case IR_AST:
          {
            // ESP.restart();
            onBtnClick(EVT_BTNMODE);
            break;
          }
          } /* switch (target) */
          target = 17;
          break;
        } /* if(config.ircodes.irVals[target][j]==irResults.value) */
      } /* for(int j=0; j<3; j++) */
    } /* for(int target=0; target<16; target++) */
  } /* if (irrecv.decode(&irResults)) */
}
#endif // if IR_PIN!=255

void onBtnLongPressStart(int id)
{
  if (configUI_isActive()) {
    if (id == EVT_ENCBTNB) {
      configUI_onLongPress();
    }
    return;
  }
  switch ((controlEvt_e)id)
  {
  case EVT_BTNSOURCE:
  {
    // SRC_BTN long-press handled by non-blocking millis-based logic in loopControls()
    break;
  }
  case EVT_BTNLEFT:
  case EVT_BTNRIGHT:
  case EVT_BTNUP:
  case EVT_BTNDOWN:
  {
    lpId = id;
    break;
  }
  case EVT_BTNCENTER:
  case EVT_ENCBTNB:
  {
#if defined(DUMMYDISPLAY)
    break;
#endif
    // Dla źródeł zewnętrznych (BT, AUX1, AUX2) przytrzymanie nic nie robi
    if (config.getMode() == PM_BLUETOOTH || config.getMode() == PM_TV || config.getMode() == PM_AUX)
    {
      break;
    }
    display.putRequest(NEWMODE, display.mode() == PLAYER ? STATIONS : PLAYER);
    break;
  }
  case EVT_ENC2BTNB:
  {
#if defined(DUMMYDISPLAY)
    break;
#endif
    // Dla źródeł zewnętrznych (BT, AUX1, AUX2) przytrzymanie nic nie robi
    if (config.getMode() == PM_BLUETOOTH || config.getMode() == PM_TV || config.getMode() == PM_AUX)
    {
      break;
    }
    display.putRequest(NEWMODE, display.mode() == PLAYER ? VOL : PLAYER);
    break;
  }
  case EVT_BTNMODE:
  {
    // config.doSleepW();
    display.putRequest(NEWMODE, SLEEPING);
    break;
  }
  default:
    break;
  }
}

void onBtnLongPressStop(int id)
{
  switch ((controlEvt_e)id)
  {
  case EVT_BTNLEFT:
  case EVT_BTNRIGHT:
  case EVT_BTNUP:
  case EVT_BTNDOWN:
  {
    lpId = -1;
    break;
  }
  case EVT_BTNMODE:
  {
    config.doSleepW();
    break;
  }
  default:
    break;
  }
}

unsigned long lpdelay;
boolean checklpdelay(int m, unsigned long &tstamp)
{
  if (millis() - tstamp > m)
  {
    tstamp = millis();
    return true;
  }
  else
  {
    return false;
  }
}

void onBtnDuringLongPress(int id)
{
  if (network.status != CONNECTED && network.status != SDREADY)
    return;
  if (checklpdelay(BTN_LONGPRESS_LOOP_DELAY, lpdelay))
  {
    switch ((controlEvt_e)id)
    {
    case EVT_BTNLEFT:
    {
      controlsEvent(false);
      break;
    }
    case EVT_BTNRIGHT:
    {
      controlsEvent(true);
      break;
    }
    case EVT_BTNUP:
    case EVT_BTNDOWN:
    {
      if (display.mode() == PLAYER)
      {
        display.putRequest(NEWMODE, STATIONS);
      }
      if (display.mode() == STATIONS)
      {
        controlsEvent(id == EVT_BTNDOWN);
      }
      break;
    }
    default:
      break;
    }
  }
}

void controlsEvent(bool toRight, int8_t volDelta)
{
  if (display.mode() == NUMBERS)
  {
    display.numOfNextStation = 0;
    display.putRequest(NEWMODE, PLAYER);
  }
  if (display.mode() != STATIONS)
  {
#if !defined(DUMMYDISPLAY)
    display.putRequest(NEWMODE, VOL);
#endif
    if (volDelta != 0)
    {
      int nv = config.store.volume + volDelta * config.store.volsteps;
      if (nv < 0)
        nv = 0;
      if (nv > 100) // Módosítva. "vol_step"
        nv = 100;
      player.setVol((uint8_t)nv);
    }
    else
    {
      player.stepVol(toRight);
    }
  }
  if (display.mode() == STATIONS)
  {
    display.resetQueue();
    int p = toRight ? display.currentPlItem + 1 : display.currentPlItem - 1;
    uint16_t cs = (config.getMode() == PM_AUX3) ? config.getFmStationCount() : config.playlistLength();
    if (cs == 0) {
      // no items
      return;
    }
    // Clamp at ends instead of wrapping: stop at first/last
    if (p < 1)
      p = 1;
    if (p > (int)cs)
      p = cs;
    display.currentPlItem = p;
    display.putRequest(DRAWPLAYLIST, p);
  }
}

void onBtnClick(int id)
{
  if (configUI_isActive())
  {
    if (id == EVT_ENCBTNB) configUI_processEncoder(0, true);
    return;
  }
  if (tdaConfigUI_isActive())
  {
    tdaConfigUI_onBtnClick(id);
    return;
  }
  bool passBnCenter = (controlEvt_e)id == EVT_BTNCENTER || (controlEvt_e)id == EVT_ENCBTNB || (controlEvt_e)id == EVT_ENC2BTNB;
  controlEvt_e btnid = static_cast<controlEvt_e>(id);
  pm.on_btn_click(btnid);
  if (network.status != CONNECTED && network.status != SDREADY && (controlEvt_e)id != EVT_BTNMODE && (controlEvt_e)id != EVT_BTNSOURCE && !passBnCenter)
    return;
  switch (btnid)
  {
  case EVT_BTNLEFT:
  {
    controlsEvent(false);
    break;
  }
  case EVT_BTNCENTER:
  case EVT_ENCBTNB:
  case EVT_ENC2BTNB:
  {
    if (display.mode() == NUMBERS)
    {
      display.numOfNextStation = 0;
      display.putRequest(NEWMODE, PLAYER);
    }
    if (display.mode() == PLAYER)
    {
      if (config.getMode() == PM_BLUETOOTH)
      {
        // Snapshot entire metadata for decision
        bt_metadata_t local;
        bt_meta_snapshot(&local);

        if (local.connected)
        {
          Serial.println("ENC click: BT center");
          String cmd = local.playing ? "PAUSE" : "PLAY";
          bool doInitialSend = false;
          bool doResend = false;
          uint8_t prevRetries = 0;
          // Check state under mutex
          if (btMetaMutex)
            xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
          bool awaiting = btMeta.awaitingAck;
          bool expected = btMeta.expectedPlaying;
          prevRetries = btMeta.ackRetries;
          if (!awaiting)
          {
            doInitialSend = true;
          }
          else
          {
            // if awaiting and expected differs from desired, allow a user-triggered resend
            bool desiredPlaying = (cmd == "PLAY");
            const uint8_t MAX_ACK_RETRIES_LOCAL = 2;
            if (expected != desiredPlaying && prevRetries < MAX_ACK_RETRIES_LOCAL)
            {
              doResend = true;
            }
          }
          if (btMetaMutex)
            xSemaphoreGive(btMetaMutex);

          if (doInitialSend)
          {
            Serial.printf("BT: sending %s (initial)\n", cmd.c_str());
            btSerial.println(cmd);
            // set flags under mutex
            if (btMetaMutex)
              xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
            btMeta.playing = (cmd == "PLAY");
            btMeta.awaitingAck = true;
            btMeta.expectedPlaying = (cmd == "PLAY");
            btMeta.ackDeadline = millis() + bt_ack_timeout_ms;
            btMeta.ackRetries = 0;
            if (btMetaMutex)
              xSemaphoreGive(btMetaMutex);
            if (config.getMode() == PM_BLUETOOTH)
            {
              display.putRequest(DBITRATE);
              display.putRequest(NEWTITLE);
            }
          }
          else if (doResend)
          {
            Serial.printf("BT: resending %s (user) retry %u\n", cmd.c_str(), (unsigned)prevRetries + 1);
            btSerial.println(cmd);
            if (btMetaMutex)
              xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
            // optimistic update + bump retries
            btMeta.playing = (cmd == "PLAY");
            btMeta.awaitingAck = true;
            btMeta.expectedPlaying = (cmd == "PLAY");
            btMeta.ackRetries = prevRetries + 1;
            btMeta.ackDeadline = millis() + bt_ack_timeout_ms;
            if (btMetaMutex)
              xSemaphoreGive(btMetaMutex);
            if (config.getMode() == PM_BLUETOOTH)
            {
              display.putRequest(DBITRATE);
              display.putRequest(NEWTITLE);
            }
          }
          else
          {
            Serial.println("BT: click ignored — awaiting ACK");
          }
        }
      }
      else
      {
        player.toggle();
      }
    }
    if (display.mode() == SCREENSAVER || display.mode() == SCREENBLANK)
    {
      display.putRequest(NEWMODE, PLAYER);
    }
    if (display.mode() == STATIONS)
    {
      if (config.getMode() == PM_AUX3) {
        // For FM stations, just set the current station and return to PLAYER
        config.setCurrentFmStation(display.currentPlItem - 1); // 0-based
        display.putRequest(NEWMODE, PLAYER);
        display.putRequest(NEWTITLE);
      } else {
        display.putRequest(NEWMODE, PLAYER);
        display.putRequest(CLOSEPLAYLIST, display.currentPlItem);
        // player.sendCommand({PR_PLAY, display.currentPlItem});
      }
    }
    if (network.status == SOFT_AP || display.mode() == LOST)
    {
#ifdef USE_SD
      config.changeMode();
#endif
    }
    break;
  }
  case EVT_BTNRIGHT:
  {
    controlsEvent(true);
    break;
  }
  case EVT_BTNUP:
  case EVT_BTNDOWN:
  {
    if (DSP_MODEL == DSP_DUMMY)
    {
      if (id == EVT_BTNUP)
      {
        player.next();
      }
      else
      {
        player.prev();
      }
    }
    else
    {
      if (display.mode() == PLAYER)
      {
        if (config.store.skipPlaylistUpDown || ENC2_BTNL != 255)
        {
          if (id == EVT_BTNUP)
          {
            player.prev();
          }
          else
          {
            player.next();
          }
        }
        else
        {
          display.putRequest(NEWMODE, STATIONS);
        }
      }
      if (display.mode() == STATIONS)
      {
        controlsEvent(id == EVT_BTNDOWN);
      }
    }
    break;
  }
#ifdef USE_SD
  case EVT_BTNMODE:
  {
    config.changeMode();
    break;
  }
#endif
  case EVT_BTNSOURCE:
  {
    Serial.printf("[CTRL] onBtnClick EVT_BTNSOURCE invoked at %lu\n", millis());
    config.changeMode();
    break;
  }
  default:
    break;
  }
}

void onBtnDoubleClick(int id)
{
  if (configUI_isActive()) return;
  if (display.mode() == SCREENSAVER || display.mode() == SCREENBLANK)
  {
    display.putRequest(NEWMODE, PLAYER);
    return;
  }
  switch ((controlEvt_e)id)
  {
  case EVT_ENCBTNB:
  {
    // Dwuklik PRZYCISKU enkodera tylko przełącza źródło dźwięku
    onBtnClick(EVT_BTNSOURCE);
    break;
  }
  default:
    break;
  }
}

void setIRTolerance(uint8_t tl)
{
  config.saveValue(&config.store.irtlp, tl);
#if IR_PIN != 255
  irrecv.setTolerance(config.store.irtlp);
#endif
}
void setEncAcceleration(uint16_t acc)
{
  config.saveValue(&config.store.encacc, acc);
#if ENC_BTNL != 255
  encoder.setAcceleration(config.store.encacc);
#endif
#if ENC2_BTNL != 255
  encoder2.setAcceleration(config.store.encacc);
#endif
}
void flipTS()
{
#if (TS_MODEL != TS_MODEL_UNDEFINED) && (DSP_MODEL != DSP_DUMMY)
  touchscreen.flip();
#endif
}