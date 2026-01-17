// Módosítva v0.9.710 "vol_step"
#include "Arduino.h"
#include "options.h"
#include "WiFi.h"
#include "time.h"
#include "config.h"
#include "display.h"
#include "player.h"
#include "network.h"
#include "netserver.h"
#include "bluetooth_uart.h"
#include "timekeeper.h"
#include "../pluginsManager/pluginsManager.h"
#include "../displays/dspcore.h"
#include "../displays/widgets/widgets.h"
#include "../displays/widgets/pages.h"
#include "../displays/tools/l10n.h"

Display display;

#ifndef CORE_STACK_SIZE
#define CORE_STACK_SIZE 1024 * 4
#endif
#ifndef DSP_TASK_PRIORITY
#define DSP_TASK_PRIORITY 2 //"task_prioritas"
#endif
#ifndef DSP_TASK_CORE_ID
#define DSP_TASK_CORE_ID 0
#endif
#ifndef DSP_TASK_DELAY
#define DSP_TASK_DELAY pdMS_TO_TICKS(10) // cap for 50 fps
#endif

#define DSP_QUEUE_TICKS 0

#ifndef DSQ_SEND_DELAY
// #define DSQ_SEND_DELAY portMAX_DELAY
#define DSQ_SEND_DELAY pdMS_TO_TICKS(200)
#endif

QueueHandle_t displayQueue;

static void loopDspTask(void *pvParameters)
{
  while (true)
  {
#ifndef DUMMYDISPLAY
    if (displayQueue == NULL)
    {
      break;
    }
    if (timekeeper.loop0())
    {
      display.loop();
#ifndef NETSERVER_LOOP1
      netserver.loop();
#endif
    }
#else
    timekeeper.loop0();
#ifndef NETSERVER_LOOP1
    netserver.loop();
#endif
#endif
    vTaskDelay(DSP_TASK_DELAY);
  }
  vTaskDelete(NULL);
}

void Display::_createDspTask()
{
  xTaskCreatePinnedToCore(loopDspTask, "DspTask", CORE_STACK_SIZE, NULL, DSP_TASK_PRIORITY, NULL, DSP_TASK_CORE_ID); //"task_prioritas"
}

#ifndef DUMMYDISPLAY
//============================================================================================================================
DspCore dsp;

Page *pages[] = {new Page(), new Page(), new Page(), new Page()};

#if !( \
    (DSP_MODEL == DSP_ST7735 && DTYPE == INITR_BLACKTAB) || DSP_MODEL == DSP_ST7789 || DSP_MODEL == DSP_ST7796 || DSP_MODEL == DSP_ILI9488 || DSP_MODEL == DSP_ILI9486 || DSP_MODEL == DSP_ILI9341 || DSP_MODEL == DSP_ILI9225 || DSP_MODEL == DSP_ST7789_170)
#undef BITRATE_FULL
#define BITRATE_FULL false
#endif

void returnPlayer()
{
  display.putRequest(NEWMODE, PLAYER);
}

Display::~Display()
{
  delete _pager;
  delete _footer;
  delete _plwidget;
  delete _nums;
  delete _clock;
  delete _meta;
  delete _title1;
  delete _title2;
  delete _plcurrent;
}

void Display::init()
{
  Serial.print("##[BOOT]#\tdisplay.init\t");
#if LIGHT_SENSOR != 255
  analogSetAttenuation(ADC_0db);
#endif
  _bootStep = 0;
  dsp.initDisplay();
  displayQueue = NULL;
  displayQueue = xQueueCreate(5, sizeof(requestParams_t));
  while (displayQueue == NULL)
  {
    ;
  }
  _createDspTask();
  while (!_bootStep == 0)
  {
    delay(10);
  }
  //_pager.begin();
  //_bootScreen();
  _pager = new Pager();
  _footer = new Page();
  _plwidget = new PlayListWidget();
  _nums = new NumWidget();
  _clock = new ClockWidget();
  _meta = new ScrollWidget();
  _title1 = new ScrollWidget();
  _plcurrent = new ScrollWidget();
  Serial.println("done");
}

uint16_t Display::width()
{
  return dsp.width();
}
uint16_t Display::height()
{
  return dsp.height();
}
#if TIME_SIZE > 19
#if DSP_MODEL == DSP_SSD1322
#define BOOT_PRG_COLOR WHITE
#define BOOT_TXT_COLOR WHITE
#define PINK WHITE
#elif DSP_MODEL == DSP_SSD1327
#define BOOT_PRG_COLOR 0x07
#define BOOT_TXT_COLOR 0x3f
#define PINK 0x02
#else
#define BOOT_PRG_COLOR 0xE68B
#define BOOT_TXT_COLOR 0xFFFF
#define PINK 0xF97F
#endif
#endif

void Display::_bootScreen()
{
  _boot = new Page();
  _boot->addWidget(new ProgressWidget(bootWdtConf, bootPrgConf, BOOT_PRG_COLOR, 0));
  _bootstring = (TextWidget *)&_boot->addWidget(new TextWidget(bootstrConf, 50, true, BOOT_TXT_COLOR, 0));
  _pager->addPage(_boot);
  _pager->setPage(_boot, true);
  dsp.drawLogo(bootLogoTop);
  _bootStep = 1;
}

void Display::_buildPager()
{
  _meta->init("*", metaConf, config.theme.meta, config.theme.metabg);
  _title1->init("*", title1Conf, config.theme.title1, config.theme.background);
  _clock->init(clockConf, 0, 0);
#if DSP_MODEL == DSP_NOKIA5110
  _plcurrent->init("*", playlistConf, 0, 1);
#else
  _plcurrent->init("*", playlistConf, config.theme.plcurrent, config.theme.plcurrentbg);
#endif
  _plwidget->init(_plcurrent);
#if !defined(DSP_LCD)
  _plcurrent->moveTo({TFT_FRAMEWDT, (uint16_t)(_plwidget->currentTop()), (int16_t)playlistConf.width});
#endif
#ifndef HIDE_TITLE2
  _title2 = new ScrollWidget("*", title2Conf, config.theme.title2, config.theme.background);
#endif
#if !defined(DSP_LCD) && DSP_MODEL != DSP_NOKIA5110
  _plbackground = new FillWidget(playlBGConf, config.theme.plcurrentfill);
#if DSP_INVERT_TITLE || defined(DSP_OLED)
  _metabackground = new FillWidget(metaBGConf, config.theme.metafill);
#else
  _metabackground = new FillWidget(metaBGConfInv, config.theme.metafill);
#endif
#endif
#if DSP_MODEL == DSP_NOKIA5110
  _plbackground = new FillWidget(playlBGConf, 1);
  //_metabackground = new FillWidget(metaBGConf, 1);
#endif
#ifndef HIDE_VOLBAR
  _volbar = new SliderWidget(volbarConf, config.theme.volbarin, config.theme.background, 100, config.theme.volbarout); // "vol_step"
#endif
#ifndef HIDE_HEAPBAR
  _heapbar = new SliderWidget(heapbarConf, config.theme.buffer, config.theme.background, psramInit() ? 300000 : 1600 * config.store.abuff);
#endif
#ifndef HIDE_VOL
  _voltxt = new TextWidget(voltxtConf, 10, false, config.theme.vol, config.theme.background);
#endif
#ifndef HIDE_IP
  _volip = new TextWidget(iptxtConf, 30, false, config.theme.ip, config.theme.background);
#endif
#ifndef HIDE_RSSI
  _rssi = new TextWidget(rssiConf, 20, false, config.theme.rssi, config.theme.background);
#endif
  _nums->init(numConf, 10, false, config.theme.digit, config.theme.background);

  if (_volbar)
  {
    _footer->addWidget(_volbar);
  }
  if (_voltxt)
  {
    _footer->addWidget(_voltxt);
  }
  if (_volip)
  {
    _footer->addWidget(_volip);
  }
  if (_rssi)
  {
    _footer->addWidget(_rssi);
  }
  if (_heapbar)
  {
    _footer->addWidget(_heapbar);
  }

  if (_metabackground)
  {
    pages[PG_PLAYER]->addWidget(_metabackground);
  }
  pages[PG_PLAYER]->addWidget(_meta);
  pages[PG_PLAYER]->addWidget(_title1);
  if (_title2)
  {
    pages[PG_PLAYER]->addWidget(_title2);
  }

#if BITRATE_FULL
  _fullbitrate = new BitrateWidget(fullbitrateConf, config.theme.bitrate, config.theme.background);
  pages[PG_PLAYER]->addWidget(_fullbitrate);
#else
  _bitrate = new TextWidget(bitrateConf, 30, false, config.theme.bitrate, config.theme.background);
  pages[PG_PLAYER]->addWidget(_bitrate);
#endif
  pages[PG_PLAYER]->addWidget(_clock);
  pages[PG_SCREENSAVER]->addWidget(_clock);
  pages[PG_PLAYER]->addPage(_footer);

  if (_metabackground)
  {
    pages[PG_DIALOG]->addWidget(_metabackground);
  }
  pages[PG_DIALOG]->addWidget(_meta);
  pages[PG_DIALOG]->addWidget(_nums);

#if !defined(DSP_LCD) && DSP_MODEL != DSP_NOKIA5110
  pages[PG_DIALOG]->addPage(_footer);
#endif
#if !defined(DSP_LCD)
  if (_plbackground)
  {
    pages[PG_PLAYLIST]->addWidget(_plbackground);
    _plbackground->setHeight(_plwidget->itemHeight());
    _plbackground->moveTo({0, (uint16_t)(_plwidget->currentTop() - playlistConf.widget.textsize * 2), (int16_t)playlBGConf.width});
  }
#endif
  pages[PG_PLAYLIST]->addWidget(_plcurrent);
  pages[PG_PLAYLIST]->addWidget(_plwidget);
  for (const auto &p : pages)
  {
    _pager->addPage(p);
  }
}

void Display::_apScreen()
{
  if (_boot)
  {
    _pager->removePage(_boot);
  }
#ifndef DSP_LCD
  _boot = new Page();
#if DSP_MODEL != DSP_NOKIA5110
#if DSP_INVERT_TITLE || defined(DSP_OLED)
  _boot->addWidget(new FillWidget(metaBGConf, config.theme.metafill));
#else
  _boot->addWidget(new FillWidget(metaBGConfInv, config.theme.metafill));
#endif
#endif
  ScrollWidget *bootTitle = (ScrollWidget *)&_boot->addWidget(new ScrollWidget("*", apTitleConf, config.theme.meta, config.theme.metabg));
  bootTitle->setText("yoRadio AP Mode");
  TextWidget *apname = (TextWidget *)&_boot->addWidget(new TextWidget(apNameConf, 30, false, config.theme.title1, config.theme.background));
  apname->setText(LANG::apNameTxt);
  TextWidget *apname2 = (TextWidget *)&_boot->addWidget(new TextWidget(apName2Conf, 30, false, config.theme.clock, config.theme.background));
  apname2->setText(apSsid);
  TextWidget *appass = (TextWidget *)&_boot->addWidget(new TextWidget(apPassConf, 30, false, config.theme.title1, config.theme.background));
  appass->setText(LANG::apPassTxt);
  TextWidget *appass2 = (TextWidget *)&_boot->addWidget(new TextWidget(apPass2Conf, 30, false, config.theme.clock, config.theme.background));
  appass2->setText(apPassword);
  ScrollWidget *bootSett = (ScrollWidget *)&_boot->addWidget(new ScrollWidget("*", apSettConf, config.theme.title2, config.theme.background));
  bootSett->setText(config.ipToStr(WiFi.softAPIP()), LANG::apSettFmt);
  _pager->addPage(_boot);
  _pager->setPage(_boot);
#else
  dsp.apScreen();
#endif
}

void Display::_start()
{
  if (_boot)
  {
    _pager->removePage(_boot);
  }
  if (network.status != CONNECTED && network.status != SDREADY)
  {
    _apScreen();
    _bootStep = 2;
    return;
  }
  _buildPager();
  _mode = PLAYER;
  config.setTitle(LANG::const_PlReady);

  if (_heapbar)
  {
    _heapbar->lock(!config.store.audioinfo);
  }

  if (_rssi)
  {
    _setRSSI(WiFi.RSSI());
  }
#ifndef HIDE_IP
  if (_volip)
  {
    _volip->setText(config.ipToStr(WiFi.localIP()), iptxtFmt);
  }
#endif
  _pager->setPage(pages[PG_PLAYER]);
  _volume();
  _station();
  _time(false);
  _bootStep = 2;
  pm.on_display_player();
}

void Display::_showDialog(const char *title)
{
  dsp.setScrollId(NULL);
  _pager->setPage(pages[PG_DIALOG]);
#ifdef META_MOVE
  _meta->moveTo(metaMove);
#endif
  _meta->setAlign(WA_CENTER);
  _meta->setText(title);
}

void Display::_swichMode(displayMode_e newmode)
{
  if (newmode == _mode || (network.status != CONNECTED && network.status != SDREADY))
  {
    return;
  }
  _mode = newmode;
  dsp.setScrollId(NULL);
  if (newmode == PLAYER)
  {
    if (player.isRunning())
    {
      if (clockMove.width < 0)
      {
        _clock->moveBack();
      }
      else
      {
        _clock->moveTo(clockMove);
      }
    }
    else
    {
      _clock->moveBack();
    }
#ifdef DSP_LCD
    dsp.clearDsp();
#endif
    numOfNextStation = 0;
#ifdef META_MOVE
    _meta->moveBack();
#endif
    _meta->setAlign(metaConf.widget.align);
    //_meta->setText(config.station.name);
    // _nums->setText("");
    _station();
    config.isScreensaver = false;
    _pager->setPage(pages[PG_PLAYER]);
    config.setDspOn(config.store.dspon, false);
    pm.on_display_player();
  }
  if (newmode == SCREENSAVER || newmode == SCREENBLANK)
  {
    config.isScreensaver = true;
    _pager->setPage(pages[PG_SCREENSAVER]);
    if (newmode == SCREENBLANK)
    {
      // dsp.clearClock();
      _clock->clear();
      config.setDspOn(false, false);
    }
  }
  else
  {
    config.screensaverTicks = SCREENSAVERSTARTUPDELAY;
    config.screensaverPlayingTicks = SCREENSAVERSTARTUPDELAY;
    config.isScreensaver = false;
#if PWR_AMP != 255 // "PWR_AMP"
    digitalWrite(PWR_AMP, HIGH);
#endif
  }
  if (newmode == VOL)
  {
#ifndef HIDE_IP
    _showDialog(LANG::const_DlgVolume);
#else
    _showDialog(config.ipToStr(WiFi.localIP()));
#endif
    _nums->setText(config.store.volume, numtxtFmt);
  }
  if (newmode == LOST)
  {
    _showDialog(LANG::const_DlgLost);
  }
  if (newmode == UPDATING)
  {
    _showDialog(LANG::const_DlgUpdate);
  }
  if (newmode == SLEEPING)
  {
    _showDialog("SLEEPING");
  }
  if (newmode == SDCHANGE)
  {
    _showDialog(LANG::const_waitForSD);
  }
  if (newmode == INFO || newmode == SETTINGS || newmode == TIMEZONE || newmode == WIFI)
  {
    _showDialog(LANG::const_DlgNextion);
  }
  if (newmode == NUMBERS)
  {
    _showDialog("");
  }
  if (newmode == STATIONS)
  {
    _pager->setPage(pages[PG_PLAYLIST]);
    _plcurrent->setText("");
    currentPlItem = config.lastStation();
    _drawPlaylist();
  }
}

void Display::resetQueue()
{
  if (displayQueue != NULL)
  {
    xQueueReset(displayQueue);
  }
}

void Display::_drawPlaylist()
{
  // dsp.drawPlaylist(currentPlItem);
  _plwidget->drawPlaylist(currentPlItem);
  uint8_t stations_list_return_time = STATIONS_LIST_RETURN_TIME;
  if (stations_list_return_time < 1)
  {
    stations_list_return_time = 1;
  }
  timekeeper.waitAndReturnPlayer(stations_list_return_time); // "stations_list_return_time"
                                                             //  Serial.printf(" Display::_drawPlaylist \n");
}

void Display::_drawNextStationNum(uint16_t num)
{
  timekeeper.waitAndReturnPlayer(30); // Visszatérési idő a főképernyőre.
  _meta->setText(config.stationByNum(num));
  _nums->setText(num, "%d");
}

void Display::putRequest(displayRequestType_e type, int payload)
{
  if (displayQueue == NULL)
  {
    return;
  }
  requestParams_t request;
  request.type = type;
  request.payload = payload;
  xQueueSend(displayQueue, &request, DSQ_SEND_DELAY);
}

void Display::_layoutChange(bool played)
{
  if (played)
  {
    if (clockMove.width < 0)
    {
      _clock->moveBack();
    }
    else
    {
      _clock->moveTo(clockMove);
    }
    //_clock->moveBack();
  }
  else
  {
    _clock->moveBack();
  }
}

void Display::loop()
{
  if (_bootStep == 0)
  {
    _pager->begin();
    _bootScreen();
    return;
  }
  if (displayQueue == NULL || _locked)
  {
    return;
  }
  _pager->loop();
  requestParams_t request;
  if (xQueueReceive(displayQueue, &request, DSP_QUEUE_TICKS))
  {
    bool pm_result = true;
    pm.on_display_queue(request, pm_result);
    if (pm_result)
    {
      switch (request.type)
      {
      case NEWMODE:
        _swichMode((displayMode_e)request.payload);
        break;
      case CLOSEPLAYLIST:
        player.sendCommand({PR_PLAY, request.payload});
        break;
      case CLOCK:
        if (_mode == PLAYER || _mode == SCREENSAVER)
        {
          _time(request.payload == 1);
        }
        break;
      case NEWTITLE:
        _title();
        break;
      case NEWSTATION:
        _station();
        break;
      case NEXTSTATION:
        _drawNextStationNum(request.payload);
        break;
      case DRAWPLAYLIST:
        _drawPlaylist();
        break;
      case DRAWVOL:
        _volume();
        break;
      case DBITRATE:
      {
        if (_mode == PLAYER)
        { // csak a lejátszás képernyőn frissíti a bitrateWidgetet
          if (config.getMode() == PM_WEB || config.getMode() == PM_SDCARD)
          {
            // If bitrate was cleared while switching sources, try to read
            // current value from the player audio engine (useful when
            // returning to WEB mode without a fresh evt_bitrate).
            if (config.station.bitrate == 0 && config.getMode() == PM_WEB)
            {
              uint32_t cur = player.getBitRate();
              if (cur > 0)
              {
                if (cur > 3000) // convert b/s to kb/s like audio_bitrate()
                  cur = cur / 1000;
                config.station.bitrate = static_cast<uint16_t>(cur);
              }
            }
            char buf[20];
            snprintf(buf, 20, bitrateFmt, config.station.bitrate);
            if (_bitrate)
            {
              _bitrate->setText(config.station.bitrate == 0 ? "" : buf);
            }
          }
          else
          {
            config.station.bitrate = 0;
            if (_bitrate)
            {
              _bitrate->setText("");
            }
          }
          if (_fullbitrate)
          {
            // For BT mode we want the full bitrate widget active so it can
            // draw the small play/pause icon even when bitrate==0.
            if (config.getMode() == PM_WEB || config.getMode() == PM_SDCARD)
            {
              _fullbitrate->setActive(true, false);
              _fullbitrate->setBitrate(config.station.bitrate);
              _fullbitrate->setFormat(config.configFmt);
            }
            else
            {
              // enable widget and set bitrate to 0 so the widget's BT-specific
              // drawing code can render the play/pause icon when in BT mode.
              _fullbitrate->setActive(true, false);
              _fullbitrate->setBitrate(0);
              _fullbitrate->setFormat(config.configFmt);
            }
          }
        }
      }
      break;
      case CLEARALLBITRATE:
      { // "nameday"
        if (_mode == PLAYER)
        { // csak a lejátszás képernyőn
          if (_bitrate)
          {
            _bitrate->setText("");
          }
          if (_fullbitrate)
          {
            _fullbitrate->clearAll();
          }
        }
      }
      break;
      case AUDIOINFO:
        if (_heapbar)
        {
          _heapbar->lock(!config.store.audioinfo);
          _heapbar->setValue(player.inBufferFilled());
        }
        break;

      case BOOTSTRING:
      {
        if (_bootstring)
        {
          _bootstring->setText(config.ssids[request.payload].ssid, LANG::bootstrFmt);
        }
        break;
      }
      case WAITFORSD:
      {
        if (_bootstring)
        {
          _bootstring->setText(LANG::const_waitForSD);
        }
        break;
      }
      case SDFILEINDEX:
      {
        if (_mode == SDCHANGE)
        {
          _nums->setText(request.payload, "%d");
        }
        break;
      }
      case DSPRSSI:
        if (_rssi)
        {
          _setRSSI(request.payload);
        }
        if (_heapbar && config.store.audioinfo)
        {
          _heapbar->setValue(player.isRunning() ? player.inBufferFilled() : 0);
        }
        break;
      case PSTART:
        _layoutChange(true);
        break;
      case PSTOP:
        _layoutChange(false);
        break;
      case DSP_START:
        _start();
        break;
      case NEWIP:
      {
#ifndef HIDE_IP
        if (_volip)
        {
          _volip->setText(config.ipToStr(WiFi.localIP()), iptxtFmt);
        }
#endif
        break;
      }
      default:
        break;

        // check if there are more messages waiting in the Q, in this case break the loop() and go
        // for another round to evict next message, do not waste time to redraw the screen, etc...
        if (uxQueueMessagesWaiting(displayQueue))
        {
          return;
        }
      }
    }
  }

  dsp.loop();
}

void Display::_setRSSI(int rssi)
{
  if (!_rssi)
  {
    return;
  }
#if RSSI_DIGIT
  _rssi->setText(rssi, rssiFmt);
  return;
#endif
  char rssiG[3];
  int rssi_steps[] = {RSSI_STEPS};
  if (rssi >= rssi_steps[0])
  {
    strlcpy(rssiG, "\004\006", 3);
  }
  if (rssi >= rssi_steps[1] && rssi < rssi_steps[0])
  {
    strlcpy(rssiG, "\004\005", 3);
  }
  if (rssi >= rssi_steps[2] && rssi < rssi_steps[1])
  {
    strlcpy(rssiG, "\004\002", 3);
  }
  if (rssi >= rssi_steps[3] && rssi < rssi_steps[2])
  {
    strlcpy(rssiG, "\003\002", 3);
  }
  if (rssi < rssi_steps[3] || rssi >= 0)
  {
    strlcpy(rssiG, "\001\002", 3);
  }
  _rssi->setText(rssiG);
}

void Display::_station()
{
  _meta->setAlign(metaConf.widget.align);
  if (config.getMode() == PM_WEB)
  {
    // Prefer explicit station name when available — show it immediately
    // even if player.isRunning() may briefly be false during transitions.
    if (config.station.name[0] != '\0')
    {
      if (config.station.name[0] == '.')
      {
        _meta->setText(config.station.name + 1);
      }
      else
      {
        _meta->setText(config.station.name);
      }
      return;
    }
    // Fallback: show placeholder when no station name available and player not running
    if (!player.isRunning())
    {
      _meta->setText(SRC_WEB_NAME);
      // Clear title/artist area so it doesn't show previous metadata
      if (_title1)
        _title1->setText("");
      if (_title2)
        _title2->setText("");
      return;
    }
  }
  else if (config.getMode() == PM_SDCARD)
  {
    _meta->setText(SRC_SD_NAME);
  }
  else
  {
    const char *modeName = config.getModeName(config.getMode());
    _meta->setText(modeName);
    netserver.requestOnChange(STATIONNAME, 0);
  }
}

char *split(char *str, const char *delim)
{
  char *dmp = strstr(str, delim);
  if (dmp == NULL)
  {
    return NULL;
  }
  *dmp = '\0';
  return dmp + strlen(delim);
}

void Display::_title()
{
  // Ha üres a title, használja a playlistben tárolt nevet.
  if (config.getMode() == PM_BLUETOOTH)
  {
    // Bluetooth mode
    String stationText = SRC_BT_NAME;

    // Snapshot entire btMeta for consistent view
    bt_metadata_t local;
    bt_meta_snapshot(&local);

    // Treat device as "recently seen" even if `connected` was flipped,
    // to avoid showing generic text during short disconnect flaps.
    bool recentlySeen = false;
    if (local.lastSeen > 0)
    {
      uint32_t age = millis() - local.lastSeen;
      if (age <= bt_heartbeat_timeout_ms)
        recentlySeen = true;
    }

    if (local.connected)
    {
      if (strlen(local.deviceName) > 0)
      {
        stationText = String("BT:") + local.deviceName;
      }
      else if (strlen(local.deviceMAC) > 0)
      {
        stationText = String("BT:") + local.deviceMAC;
      }
    }
    _meta->setText(stationText.c_str());
    netserver.requestOnChange(STATIONNAME, 0);

    // For the title lines, require an actual connection.
    // If the device is disconnected (even if recentlySeen), show the
    // configured "no connection" text on the title and clear metadata.
    if (!local.connected)
    {
      _title1->setText(SRC_BT_NAME2);
      if (_title2)
      {
        _title2->setText("");
      }
      strlcpy(config.station.title, "", sizeof(config.station.title));
      netserver.requestOnChange(TITLE, 0);
      return;
    }

    // Connected: show pause state or metadata as before
    if (!local.playing)
    {
      _title1->setText("[Zatrzymany]");
      if (_title2)
        _title2->setText("");
      strlcpy(config.station.title, "", sizeof(config.station.title));
      netserver.requestOnChange(TITLE, 0);
      return;
    }
    // Use snapshot's artist/title
    char localArtist[sizeof(local.artist)];
    char localTitle[sizeof(local.title)];
    strlcpy(localArtist, local.artist, sizeof(localArtist));
    strlcpy(localTitle, local.title, sizeof(localTitle));

    // Treat common placeholder values from some phones as empty
    if (strcasecmp(localTitle, "Not Provided") == 0)
    {
      localTitle[0] = '\0';
    }
    if (strcasecmp(localArtist, "Not Provided") == 0)
    {
      localArtist[0] = '\0';
    }
    // If the phone reports the device name as ARTIST/TITLE (common on Android),
    // treat it as empty so we don't duplicate the device name on all lines.
    if (stationText.length() > 0)
    {
      if (strcasecmp(localArtist, stationText.c_str()) == 0)
        localArtist[0] = '\0';
      if (strcasecmp(localTitle, stationText.c_str()) == 0)
        localTitle[0] = '\0';
    }

    // If no metadata yet, show device name + 'Połączony' on second line
    if (strlen(localArtist) == 0 && strlen(localTitle) == 0)
    {
      _title1->setText("Połączony");
      if (_title2)
        _title2->setText("");
      strlcpy(config.station.title, "", sizeof(config.station.title));
    }
    else
    {
      String artistText = "";
      if (strlen(localArtist) > 0)
      {
        artistText = localArtist;
      }
      else
      {
        artistText = stationText; // fallback
      }
      _title1->setText(artistText.c_str());

      String titleText = "";
      if (strlen(localTitle) > 0)
      {
        titleText = localTitle;
      }
      else
      {
        titleText = stationText; // fallback
      }
      if (_title2)
      {
        _title2->setText(titleText.c_str());
      }
      String meta = artistText + " - " + titleText;
      strlcpy(config.station.title, meta.c_str(), sizeof(config.station.title));
    }
  }
  netserver.requestOnChange(TITLE, 0);

  if (config.getMode() == PM_TV)
  {
    // TV/AUX1: show source name on station line (handled in _station())
    // and secondary name on artist line; clear title line
    _title1->setText(SRC_AUX1_NAME2);
    if (_title2)
      _title2->setText("");
    strlcpy(config.station.title, "", sizeof(config.station.title));
    netserver.requestOnChange(TITLE, 0);
    return;
  }
  if (config.getMode() == PM_AUX)
  {
    // AUX2: same as above but use AUX2 names
    _title1->setText(SRC_AUX2_NAME2);
    if (_title2)
      _title2->setText("");
    strlcpy(config.station.title, "", sizeof(config.station.title));
    netserver.requestOnChange(TITLE, 0);
    return;
  }
  if (config.getMode() == PM_SDCARD)
  {
    if (digitalRead(SD_DETECT_PIN) == HIGH)
    {
      _title1->setText(SRC_SD_NAME2);
      if (_title2)
        _title2->setText("");
      strlcpy(config.station.title, "", sizeof(config.station.title));
      netserver.requestOnChange(TITLE, 0);
      return;
    }
    else
    {
      _title1->setText("");
      if (_title2)
        _title2->setText("");
      strlcpy(config.station.title, "", sizeof(config.station.title));
      netserver.requestOnChange(TITLE, 0);
      return;
    }
  }
  if (strlen(config.station.title) == 0)
  {
    strlcpy(config.station.title, config.station.name, sizeof(config.station.title));
  }
  if (strlen(config.station.title) > 0)
  {
    char tmpbuf[strlen(config.station.title) + 1];
    strlcpy(tmpbuf, config.station.title, sizeof(tmpbuf));
    char *stitle = split(tmpbuf, " - ");
    if (stitle && _title2)
    {
      _title1->setText(tmpbuf);
      _title2->setText(stitle);
    }
    else
    {
      _title1->setText(config.station.title);
      if (_title2)
      {
        _title2->setText("");
      }
    }
  }
  else
  {
    _title1->setText("");
    if (_title2)
    {
      _title2->setText("");
    }
  }
  if (player_on_track_change)
  {
    player_on_track_change();
  }
  pm.on_track_change();
}

void Display::_time(bool redraw)
{

#if LIGHT_SENSOR != 255
  if (config.store.dspon)
  {
    config.store.brightness = AUTOBACKLIGHT(analogRead(LIGHT_SENSOR));
    _title1->setText(SRC_BT_NAME2);
  }
#endif
  if (config.isScreensaver && network.timeinfo.tm_sec % 60 == 0)
  {
#if TIME_SIZE < 19
    uint16_t ft = static_cast<uint16_t>(random(TFT_FRAMEWDT, (dsp.height() - TIME_SIZE * CHARHEIGHT - TFT_FRAMEWDT)));
#else
    uint16_t ft = static_cast<uint16_t>(random(TFT_FRAMEWDT + TIME_SIZE, (dsp.height() - _clock->dateSize() - TFT_FRAMEWDT * 2)));
#endif
    uint16_t lt = static_cast<uint16_t>(random(TFT_FRAMEWDT, (dsp.width() - _clock->clockWidth() - TFT_FRAMEWDT)));
    if (clockConf.align == WA_CENTER)
    {
      lt -= (dsp.width() - _clock->clockWidth()) / 2;
    }
    //_clock->moveTo({clockConf.left, ft, 0});
    _clock->moveTo({lt, ft, 0});
  }
  _clock->draw(redraw);
}

void Display::_volume()
{
  if (_volbar)
  { // Módosítás "vol_step"
    int vol = (config.store.volume);
    if (vol > 100)
    {
      vol = 100;
    }
    if (vol < 0)
    {
      vol = 0;
    }
    _volbar->setValue(vol);
  }
#ifndef HIDE_VOL
  if (_voltxt)
  {                                                   // ha az alapképernyő van
    _voltxt->setText(config.store.volume, voltxtFmt); // Módosítás "vol_step"
  }
#endif
  if (_mode == VOL)
  {
    timekeeper.waitAndReturnPlayer(2);
    _nums->setText(config.store.volume, numtxtFmt);
  }
}

void Display::flip()
{
  dsp.flip();
}

void Display::invert()
{
  dsp.invert();
}

void Display::setContrast()
{
#if DSP_MODEL == DSP_NOKIA5110
  dsp.setContrast(config.store.contrast);
#endif
}

bool Display::deepsleep()
{
#if defined(LCD_I2C) || defined(DSP_OLED) || BRIGHTNESS_PIN != 255
  dsp.sleep();
  return true;
#endif
  return false;
}

void Display::wakeup()
{
#if defined(LCD_I2C) || defined(DSP_OLED) || BRIGHTNESS_PIN != 255
  dsp.wake();
#endif
}

void Display::switchToPlayer()
{
  // For DUMMYDISPLAY, do nothing or minimal
}
//============================================================================================================================
#else  // !DUMMYDISPLAY
//============================================================================================================================
void Display::init()
{
  _createDspTask();
}
void Display::_start()
{
  config.setTitle(LANG::const_PlReady);
}

void Display::putRequest(displayRequestType_e type, int payload)
{
  if (type == DSP_START)
  {
    _start();
  }
  if (type == NEWMODE)
  {
    mode((displayMode_e)payload);
  }
}

void Display::switchToPlayer()
{
  _swichMode(PLAYER);
}
//============================================================================================================================
#endif // DUMMYDISPLAY
