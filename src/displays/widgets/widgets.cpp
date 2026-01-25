// Módosítva! v0.9.710 "bitrate"
#include "../../core/options.h"
#if DSP_MODEL != DSP_DUMMY
#ifdef NAMEDAYS_FILE
#include "../../core/namedays.h"
#endif

#include "../../core/config.h"
#include "../../core/network.h" //  for Clock widget
#include "../../core/bluetooth_uart.h"
#include "../dspcore.h"
#include "../tools/l10n.h"
#include "../tools/psframebuffer.h"
#include "Arduino.h"
#include "widgets.h"
#include "../../core/config_ui.h"
#include "../../core/tda_config_ui.h"

/************************
      FILL WIDGET
 ************************/
void FillWidget::init(FillConfig conf, uint16_t bgcolor)
{
  Widget::init(conf.widget, bgcolor, bgcolor);
  _width = conf.width;
  _height = conf.height;
}

void FillWidget::_draw()
{
  if (!_active)
  {
    return;
  }
  dsp.fillRect(_config.left, _config.top, _width, _height, _bgcolor);
}

void FillWidget::setHeight(uint16_t newHeight)
{
  _height = newHeight;
  //_draw();
}
/************************
      TEXT WIDGET
 ************************/
TextWidget::~TextWidget()
{
  free(_text);
  free(_oldtext);
}

void TextWidget::_charSize(uint8_t textsize, uint8_t &width, uint16_t &height)
{
#ifndef DSP_LCD
  width = textsize * CHARWIDTH;
  height = textsize * CHARHEIGHT;
#else
  width = 1;
  height = 1;
#endif
}

void TextWidget::init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor)
{
  Widget::init(wconf, fgcolor, bgcolor);
  _buffsize = buffsize;
  _text = (char *)malloc(sizeof(char) * _buffsize);
  memset(_text, 0, _buffsize);
  _oldtext = (char *)malloc(sizeof(char) * _buffsize);
  memset(_oldtext, 0, _buffsize);
  _charSize(_config.textsize, _charWidth, _textheight);
  _textwidth = _oldtextwidth = _oldleft = 0;
  _uppercase = uppercase;
}

void TextWidget::setText(const char *txt)
{
  strlcpy(_text, utf8To(txt, _uppercase), _buffsize);
  _textwidth = strlen(_text) * _charWidth;
  if (strcmp(_oldtext, _text) == 0)
  {
    return;
  }
  if (_active)
  {
    dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()), _config.top, max(_oldtextwidth, _textwidth), _textheight, _bgcolor);
  }
  _oldtextwidth = _textwidth;
  _oldleft = _realLeft();
  if (_active)
  {
    _draw();
  }
}

void TextWidget::setText(int val, const char *format)
{
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, val);
  setText(buf);
}

void TextWidget::setText(const char *txt, const char *format)
{
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, txt);
  setText(buf);
}

uint16_t TextWidget::_realLeft(bool w_fb)
{
  uint16_t realwidth = (_width > 0 && w_fb) ? _width : dsp.width();
  switch (_config.align)
  {
  case WA_CENTER:
    return (realwidth - _textwidth) / 2;
    break;
  case WA_RIGHT:
    return (realwidth - _textwidth - (!w_fb ? _config.left : 0));
    break;
  default:
    return !w_fb ? _config.left : 0;
    break;
  }
}

void TextWidget::_draw()
{
  if (!_active)
  {
    return;
  }
  dsp.setTextColor(_fgcolor, _bgcolor);
  dsp.setCursor(_realLeft(), _config.top);
  dsp.setFont();
  dsp.setTextSize(_config.textsize);
  dsp.print(_text);
  strlcpy(_oldtext, _text, _buffsize);
}

/************************
      SCROLL WIDGET
 ************************/
ScrollWidget::ScrollWidget(const char *separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor)
{
  init(separator, conf, fgcolor, bgcolor);
}

ScrollWidget::~ScrollWidget()
{
  free(_fb);
  free(_sep);
  free(_window);
}

void ScrollWidget::init(const char *separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor)
{
  TextWidget::init(conf.widget, conf.buffsize, conf.uppercase, fgcolor, bgcolor);
  _sep = (char *)malloc(sizeof(char) * 4);
  memset(_sep, 0, 4);
  snprintf(_sep, 4, " %.*s ", 1, separator);
  _x = conf.widget.left;
  _startscrolldelay = conf.startscrolldelay;
  _scrolldelta = conf.scrolldelta;
  _scrolltime = conf.scrolltime;
  _charSize(_config.textsize, _charWidth, _textheight);
  _sepwidth = strlen(_sep) * _charWidth;
  _width = conf.width;
  _backMove.width = _width;
  _window = (char *)malloc(sizeof(char) * (MAX_WIDTH / _charWidth + 1));
  memset(_window, 0, (MAX_WIDTH / _charWidth + 1)); // +1?
  _doscroll = false;
#ifdef PSFBUFFER
  _fb = new psFrameBuffer(dsp.width(), dsp.height());
  uint16_t _rl = (_config.align == WA_CENTER) ? (dsp.width() - _width) / 2 : _config.left;
  _fb->begin(&dsp, _rl, _config.top, _width, _textheight, _bgcolor);
#endif
}

void ScrollWidget::_setTextParams()
{
  if (_config.textsize == 0)
  {
    return;
  }
  if (_fb->ready())
  {
#ifdef PSFBUFFER
    _fb->setTextSize(_config.textsize);
    _fb->setTextColor(_fgcolor, _bgcolor);
#endif
  }
  else
  {
    dsp.setTextSize(_config.textsize);
    dsp.setTextColor(_fgcolor, _bgcolor);
  }
}

bool ScrollWidget::_checkIsScrollNeeded()
{
  return _textwidth > _width;
}

void ScrollWidget::setText(const char *txt)
{
  // Serial.printf("widget.cpp -> setText() -> txt %s\r\n", txt);
  strlcpy(_text, utf8To(txt, _uppercase), _buffsize - 1);
  if (strcmp(_oldtext, _text) == 0)
  {
    return;
  }
  _textwidth = strlen(_text) * _charWidth;
  _x = _fb->ready() ? 0 : _config.left;
  _doscroll = _checkIsScrollNeeded();
  if (dsp.getScrollId() == this)
  {
    dsp.setScrollId(NULL);
  }
  _scrolldelay = millis();
  if (_active)
  {
    _setTextParams();
    if (_doscroll)
    {
      if (_fb->ready())
      {
#ifdef PSFBUFFER
        _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
        _fb->setCursor(0, 0);
        snprintf(_window, _width / _charWidth + 1, "%s", _text); // TODO
        _fb->print(_window);
        _fb->display();
#endif
      }
      else
      {
        dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
        dsp.setCursor(_config.left, _config.top);
        snprintf(_window, _width / _charWidth + 1, "%s", _text); // TODO
        dsp.setClipping({_config.left, _config.top, _width, _textheight});
        dsp.print(_window);
        dsp.clearClipping();
      }
    }
    else
    {
      if (_fb->ready())
      {
#ifdef PSFBUFFER
        _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
        _fb->setCursor(_realLeft(true), 0);
        _fb->print(_text);
        _fb->display();
#endif
      }
      else
      {
        dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
        dsp.setCursor(_realLeft(), _config.top);
        // dsp.setClipping({_config.left, _config.top, _width, _textheight});
        dsp.print(_text);
        // dsp.clearClipping();
      }
    }
    strlcpy(_oldtext, _text, _buffsize);
  }
}

void ScrollWidget::setText(const char *txt, const char *format)
{
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, txt);
  setText(buf);
}

void ScrollWidget::loop()
{
  if (_locked)
  {
    return;
  }
  if (!_doscroll || _config.textsize == 0 || (dsp.getScrollId() != NULL && dsp.getScrollId() != this))
  {
    return;
  }
  uint16_t fbl = _fb->ready() ? 0 : _config.left;
  if (_checkDelay(_x == fbl ? _startscrolldelay : _scrolltime, _scrolldelay))
  {
    _calcX();
    if (_active)
    {
      _draw();
    }
  }
}

void ScrollWidget::_clear()
{
  if (_fb->ready())
  {
#ifdef PSFBUFFER
    _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
    _fb->display();
#endif
  }
  else
  {
    dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
  }
}

void ScrollWidget::_draw()
{
  if (!_active || _locked)
  {
    return;
  }
  _setTextParams();
  if (_doscroll)
  {
    uint16_t fbl = _fb->ready() ? 0 : _config.left;
    uint16_t _newx = fbl - _x;
    const char *_cursor = _text + _newx / _charWidth;
    uint16_t hiddenChars = _cursor - _text;
    uint8_t addChars = _fb->ready() ? 2 : 1;
    if (hiddenChars < strlen(_text))
    {
// TODO
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation="
      snprintf(_window, _width / _charWidth + addChars, "%s%s%s", _cursor, _sep, _text);
#pragma GCC diagnostic pop
    }
    else
    {
      const char *_scursor = _sep + (_cursor - (_text + strlen(_text)));
      snprintf(_window, _width / _charWidth + addChars, "%s%s", _scursor, _text);
    }
    if (_fb->ready())
    {
#ifdef PSFBUFFER
      _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
      _fb->setCursor(_x + hiddenChars * _charWidth, 0);
      _fb->print(_window);
      _fb->display();
#endif
    }
    else
    {
      dsp.setCursor(_x + hiddenChars * _charWidth, _config.top);
      dsp.setClipping({_config.left, _config.top, _width, _textheight});
      dsp.print(_window);
#ifndef DSP_LCD
      dsp.print(" ");
#endif
      dsp.clearClipping();
    }
  }
  else
  {
    if (_fb->ready())
    {
#ifdef PSFBUFFER
      _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
      _fb->setCursor(_realLeft(true), 0);
      _fb->print(_text);
      _fb->display();
#endif
    }
    else
    {
      dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
      dsp.setCursor(_realLeft(), _config.top);
      dsp.setClipping({_realLeft(), _config.top, _width, _textheight});
      dsp.print(_text);
      dsp.clearClipping();
    }
  }
}

void ScrollWidget::_calcX()
{
  if (!_doscroll || _config.textsize == 0)
  {
    return;
  }
  _x -= _scrolldelta;
  uint16_t fbl = _fb->ready() ? 0 : _config.left;
  if (-_x > _textwidth + _sepwidth - fbl)
  {
    _x = fbl;
    dsp.setScrollId(NULL);
  }
  else
  {
    dsp.setScrollId(this);
  }
}

bool ScrollWidget::_checkDelay(int m, uint32_t &tstamp)
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

void ScrollWidget::_reset()
{
  dsp.setScrollId(NULL);
  _x = _fb->ready() ? 0 : _config.left;
  _scrolldelay = millis();
  _doscroll = _checkIsScrollNeeded();
#ifdef PSFBUFFER
  _fb->freeBuffer();
  uint16_t _rl = (_config.align == WA_CENTER) ? (dsp.width() - _width) / 2 : _config.left;
  _fb->begin(&dsp, _rl, _config.top, _width, _textheight, _bgcolor);
#endif
}

/************************
      SLIDER WIDGET
 ************************/
void SliderWidget::init(FillConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint32_t maxval, uint16_t oucolor)
{
  Widget::init(conf.widget, fgcolor, bgcolor);
  _width = conf.width;
  _height = conf.height;
  _outlined = conf.outlined;
  _oucolor = oucolor, _max = maxval;
  _oldvalwidth = _value = 0;
}

void SliderWidget::setValue(uint32_t val)
{
  _value = val;
  if (_active && !_locked)
  {
    _drawslider();
  }
}

void SliderWidget::_drawslider()
{
  uint16_t valwidth = map(_value, 0, _max, 0, _width - _outlined * 2);
  if (_oldvalwidth == valwidth)
  {
    return;
  }
  dsp.fillRect(
      _config.left + _outlined + min(valwidth, _oldvalwidth), _config.top + _outlined, abs(_oldvalwidth - valwidth), _height - _outlined * 2,
      _oldvalwidth > valwidth ? _bgcolor : _fgcolor);
  _oldvalwidth = valwidth;
}

void SliderWidget::_draw()
{
  if (_locked)
  {
    return;
  }
  _clear();
  if (!_active)
  {
    return;
  }
  if (_outlined)
  {
    dsp.drawRect(_config.left, _config.top, _width, _height, _oucolor);
  }
  uint16_t valwidth = map(_value, 0, _max, 0, _width - _outlined * 2);
  dsp.fillRect(_config.left + _outlined, _config.top + _outlined, valwidth, _height - _outlined * 2, _fgcolor);
}

void SliderWidget::_clear()
{
  //  _oldvalwidth = 0;
  dsp.fillRect(_config.left, _config.top, _width, _height, _bgcolor);
}
void SliderWidget::_reset()
{
  _oldvalwidth = 0;
}

/************************
    NUM & CLOCK
************************/
#if !defined(DSP_LCD)
#if TIME_SIZE < 19 // 19->NOKIA
const GFXfont *Clock_GFXfontPtr = nullptr;
#define CLOCKFONT5x7
#else
const GFXfont *Clock_GFXfontPtr = &Clock_GFXfont;
const GFXfont *Clock_GFXfontPtr_Sec = &Clock_GFXfont_sec; // Módosítás saját betű másodperchez. "font"
#endif
#endif //! defined(DSP_LCD)

#if !defined(CLOCKFONT5x7) && !defined(DSP_LCD)
inline GFXglyph *pgm_read_glyph_ptr(const GFXfont *gfxFont, uint8_t c)
{
  return gfxFont->glyph + c;
}
uint8_t _charWidth(unsigned char c)
{
  GFXglyph *glyph = pgm_read_glyph_ptr(&Clock_GFXfont, c - 0x20);
  return pgm_read_byte(&glyph->xAdvance);
}
uint16_t _textHeight()
{
  GFXglyph *glyph = pgm_read_glyph_ptr(&Clock_GFXfont, '8' - 0x20);
  return pgm_read_byte(&glyph->height);
}
#else //! defined(CLOCKFONT5x7) && !defined(DSP_LCD)
uint8_t _charWidth(unsigned char c)
{
#ifndef DSP_LCD
  return CHARWIDTH * TIME_SIZE;
#else
  return 1;
#endif
}
uint16_t _textHeight()
{
  return CHARHEIGHT * TIME_SIZE;
}
#endif
uint16_t _textWidth(const char *txt)
{
  uint16_t w = 0, l = strlen(txt);
  for (uint16_t c = 0; c < l; c++)
  {
    w += _charWidth(txt[c]);
  }
  //  #if DSP_MODEL==DSP_ILI9225
  //  return w+l;
  //  #else
  return w;
  //  #endif
}

/************************
      NUM WIDGET
 ************************/
void NumWidget::init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor)
{
  Widget::init(wconf, fgcolor, bgcolor);
  _buffsize = buffsize;
  _text = (char *)malloc(sizeof(char) * _buffsize);
  memset(_text, 0, _buffsize);
  _oldtext = (char *)malloc(sizeof(char) * _buffsize);
  memset(_oldtext, 0, _buffsize);
  _textwidth = _oldtextwidth = _oldleft = 0;
  _uppercase = uppercase;
  _textheight = TIME_SIZE /*wconf.textsize*/;
}

void NumWidget::setText(const char *txt)
{
  strlcpy(_text, txt, _buffsize);
  _getBounds();
  if (strcmp(_oldtext, _text) == 0)
  {
    return;
  }
  uint16_t realth = _textheight;
#if defined(DSP_OLED) && DSP_MODEL != DSP_SSD1322
  if (Clock_GFXfontPtr == nullptr)
  {
    realth = _textheight * 8; // CHARHEIGHT
  }
#endif
  if (_active)
#ifndef CLOCKFONT5x7
    dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()), _config.top - _textheight + 1, max(_oldtextwidth, _textwidth), realth, _bgcolor);
#else
    dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()), _config.top, max(_oldtextwidth, _textwidth), realth, _bgcolor);
#endif

  _oldtextwidth = _textwidth;
  _oldleft = _realLeft();
  if (_active)
  {
    _draw();
  }
}

void NumWidget::setText(int val, const char *format)
{
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, val);
  setText(buf);
}

void NumWidget::_getBounds()
{
  _textwidth = _textWidth(_text);
}

void NumWidget::_draw()
{
#ifndef DSP_LCD
  if (!_active || TIME_SIZE < 2)
  {
    return;
  }
  dsp.setTextSize(Clock_GFXfontPtr == nullptr ? TIME_SIZE : 1);
  dsp.setFont(Clock_GFXfontPtr);
  dsp.setTextColor(_fgcolor, _bgcolor);
#endif
  if (!_active)
  {
    return;
  }
  dsp.setCursor(_realLeft(), _config.top);
  dsp.print(_text);
  strlcpy(_oldtext, _text, _buffsize);
  dsp.setFont();
}

/**************************
      PROGRESS WIDGET
 **************************/
void ProgressWidget::_progress()
{
  char buf[_width + 1];
  snprintf(buf, _width, "%*s%.*s%*s", _pg <= _barwidth ? 0 : _pg - _barwidth, "", _pg <= _barwidth ? _pg : 5, ".....", _width - _pg, "");
  _pg++;
  if (_pg >= _width + _barwidth)
  {
    _pg = 0;
  }
  setText(buf);
}

bool ProgressWidget::_checkDelay(int m, uint32_t &tstamp)
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

void ProgressWidget::loop()
{
  if (_checkDelay(_speed, _scrolldelay))
  {
    _progress();
  }
}

/**************************
      CLOCK WIDGET
 **************************/
void ClockWidget::init(WidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor)
{
  Widget::init(wconf, fgcolor, bgcolor);
  _timeheight = _textHeight();
  _fullclock = TIME_SIZE > 35 || DSP_MODEL == DSP_ILI9225;
  if (_fullclock)
  {
    _superfont = TIME_SIZE / 17; // magick
  }
  else if (TIME_SIZE == 19 || TIME_SIZE == 2)
  {
    _superfont = 1;
  }
  else
  {
    _superfont = 0;
  }
  _space = (5 * _superfont) / 2; // magick
#ifndef HIDE_DATE
  if (_fullclock)
  {
    _dateheight = _superfont < 4 ? 1 : 2;
    // _clockheight = _timeheight + _space + CHARHEIGHT * _dateheight; //Original
    _clockheight = _timeheight;
  }
  else
  {
    _clockheight = _timeheight;
  }
#else
  _clockheight = _timeheight;
#endif
  _getTimeBounds();
#ifdef PSFBUFFER
  _fb = new psFrameBuffer(dsp.width(), dsp.height());
  _begin();
#endif
}

void ClockWidget::_begin()
{
#ifdef PSFBUFFER
  _fb->begin(&dsp, _clockleft, _config.top - _timeheight, _clockwidth, _clockheight + 1, config.theme.background);
#endif
}

bool ClockWidget::_getTime()
{
#if defined AM_PM_STYLE
  strftime(_timebuffer, sizeof(_timebuffer), "%I:%M", &network.timeinfo);
  if (_timebuffer[0] == '0')
  {
    _timebuffer[0] = ' '; // Ha az eslő számjegy 0 kicseréli szóközre (azonos karakterszélesség szükséges)
  }
#else
  strftime(_timebuffer, sizeof(_timebuffer), "%H:%M", &network.timeinfo);
#endif
  bool ret = network.timeinfo.tm_sec == 0 || _forceflag != network.timeinfo.tm_year;
  _forceflag = network.timeinfo.tm_year;
  return ret;
}

uint16_t ClockWidget::_left()
{
  if (_fb->ready())
  {
    return 0;
  }
  else
  {
    return _clockleft;
  }
}
uint16_t ClockWidget::_top()
{
  if (_fb->ready())
  {
    return _timeheight;
  }
  else
  {
    return _config.top;
  }
}

void ClockWidget::_getTimeBounds()
{
  _timewidth = _textWidth(_timebuffer);
  uint8_t fs = _superfont > 0 ? _superfont : TIME_SIZE;
  uint16_t rightside = CHARWIDTH * fs * 2; // seconds
  if (_fullclock)
  {
    rightside += _space * 2 + 1; // 2space+vline
    _clockwidth = _timewidth + rightside;
  }
  else
  {
    if (_superfont == 0)
    {
      _clockwidth = _timewidth;
    }
    else
    {
      _clockwidth = _timewidth + rightside;
    }
  }
  switch (_config.align)
  {
  case WA_LEFT:
    _clockleft = _config.left;
    break;
  case WA_RIGHT:
    _clockleft = dsp.width() - _clockwidth - _config.left;
    break;
  default:
    _clockleft = (dsp.width() / 2 - _clockwidth / 2) + _config.left;
    break;
  }
  char buf[4];
  strftime(buf, 4, "%H", &network.timeinfo);
  _dotsleft = _textWidth(buf);
}

#ifndef DSP_LCD

Adafruit_GFX &ClockWidget::getRealDsp()
{
#ifdef PSFBUFFER
  if (_fb && _fb->ready())
  {
    return *_fb;
  }
#endif
  return dsp;
}

void ClockWidget::_printClock(bool force)
{
  auto &gfx = getRealDsp();
  gfx.setTextSize(Clock_GFXfontPtr == nullptr ? TIME_SIZE : 1);
  gfx.setFont(Clock_GFXfontPtr);
  bool clockInTitle = !config.isScreensaver && _config.top < _timeheight; // DSP_SSD1306x32
  if (force)
  {
    _clearClock();
    _getTimeBounds();
#ifndef DSP_OLED
    if (CLOCKFONT_MONO)
    {
      gfx.setTextColor(config.theme.clockbg, config.theme.background);
      gfx.setCursor(_left(), _top());
      gfx.print("88:88");
    }
#endif
    if (clockInTitle)
    {
      gfx.setTextColor(config.theme.meta, config.theme.metabg);
    }
    else
    {
      gfx.setTextColor(config.theme.clock, config.theme.background);
    }
    gfx.setCursor(_left(), _top());
    gfx.print(_timebuffer); // Az óra, perc kiírása.
    if (_fullclock)
    {
      bool fullClockOnScreensaver = (!config.isScreensaver || (_fb->ready() && FULL_SCR_CLOCK));
      _linesleft = _left() + _timewidth + _space;
      if (fullClockOnScreensaver)
      {
        gfx.drawFastVLine(_linesleft, _top() - _timeheight, _timeheight, config.theme.div); // A másodperc vertikális vonala.
#ifdef AM_PM_STYLE
        // A másodperc horizontális vonala.
        gfx.drawFastHLine(_linesleft, _top() - (_timeheight / 2), CHARWIDTH * _superfont * 2 + _space, config.theme.div);
        gfx.setTextSize(0);
        gfx.setFont(Clock_GFXfontPtr_Sec);
        char buf[3];
        strftime(buf, sizeof(buf), "%p", &network.timeinfo);
        gfx.setCursor(_linesleft + 8, _top());
        gfx.print(buf); // AM vagy PM kiírása
#else
#if DSP_MODEL == DSP_ILI9341
        constexpr int lineOffset = 17; // 320x240
#else
        constexpr int lineOffset = 25; // 480x320
#endif
        // A másodperc horizontális vonala.
        gfx.drawFastHLine(_linesleft, _top() - (_timeheight / 2) + lineOffset, CHARWIDTH * _superfont * 2 + _space, config.theme.div);
#endif
        if (!config.isScreensaver)
        {
          _formatDate();
#ifndef HIDE_DATE
          memcpy_P(&_dateConf, &dateConf, sizeof(WidgetConfig));
          // Sor törlése teljes szélességben
          int lineHeight = _dateheight * 8;                                                 // kb. 8 pixel per TextSize
          dsp.fillRect(0, _dateConf.top, dsp.width(), lineHeight, config.theme.background); // szürke 0x8410
          strlcpy(_datebuf, utf8To(_tmp, false), sizeof(_datebuf));
          uint16_t _datewidth = strlen(_datebuf) * CHARWIDTH * _dateheight;
          dsp.setFont();
          dsp.setTextSize(_dateheight);
          uint16_t _dateleft = dsp.width() - _datewidth - _dateConf.left;
          dsp.setCursor(_dateleft, _dateConf.top); // Módosítás saját beállítás változó "_dateConf"
          dsp.setTextColor(config.theme.date, config.theme.background);
          dsp.print(_datebuf);
#endif // HIDE_DATE
        }
      }
    }
  }
  if (_fullclock || _superfont > 0)
  {
    // *** Másodperc kiírása ***
    gfx.setTextSize(0);
    gfx.setFont(Clock_GFXfontPtr_Sec);
    if (CLOCKFONT_MONO)
    {
      gfx.setTextColor(config.theme.clockbg, config.theme.background);
    }
    else
    {
      gfx.setTextColor(config.theme.background, config.theme.background);
    }
    uint16_t topSec;
    uint16_t leftSec;
#if DSP_MODEL == DSP_ILI9341 // 320x240
#ifdef AM_PM_STYLE
    topSec = _top() - _timeheight + 20;
    leftSec = _linesleft + 8;
#else
    topSec = _top() - _timeheight + 38;
    leftSec = _linesleft + 3;
#endif
#else // DSP_MODEL DSP_ILI9341  480x320
#ifdef AM_PM_STYLE
    topSec = _top() - _timeheight + 30;
    leftSec = _linesleft + 8;
#else
    topSec = _top() - _timeheight + 50;
    leftSec = _linesleft + 3;
#endif
#endif
    gfx.setCursor(leftSec, topSec);
    gfx.print("88");
    gfx.setTextColor(config.theme.seconds, config.theme.background);
    gfx.setCursor(leftSec, topSec);
    sprintf(_tmp, "%02d", network.timeinfo.tm_sec);
    gfx.print(_tmp); // Másodperc kiírása
  }
  gfx.setTextSize(Clock_GFXfontPtr == nullptr ? TIME_SIZE : 1);
  gfx.setFont(Clock_GFXfontPtr);
#ifndef DSP_OLED
  gfx.setTextColor(dots ? config.theme.clock : (CLOCKFONT_MONO ? config.theme.clockbg : config.theme.background), config.theme.background);
#else
  if (clockInTitle)
  {
    gfx.setTextColor(dots ? config.theme.meta : config.theme.metabg, config.theme.metabg);
  }
  else
  {
    gfx.setTextColor(dots ? config.theme.clock : config.theme.background, config.theme.background);
  }
#endif
  dots = !dots;
  gfx.setCursor(_left() + _dotsleft, _top());
  gfx.print(":");
  gfx.setFont();
  if (_fb->ready())
  {
    _fb->display();
  }
}

void ClockWidget::_formatDate()
{
// "multi_language"
#if L10N_LANGUAGE == RU
  sprintf(_tmp, "%2d %s %d", network.timeinfo.tm_mday, LANG::mnths[network.timeinfo.tm_mon], network.timeinfo.tm_year + 1900);
#elif L10N_LANGUAGE == EN
  sprintf(_tmp, "%2d %s %d", network.timeinfo.tm_mday, LANG::mnths[network.timeinfo.tm_mon], network.timeinfo.tm_year + 1900);
#elif L10N_LANGUAGE == NL
  sprintf(
      _tmp, "%s %2d %s %d", LANG::dowf[network.timeinfo.tm_wday], network.timeinfo.tm_mday, LANG::mnths[network.timeinfo.tm_mon], network.timeinfo.tm_year + 1900);
#elif L10N_LANGUAGE == HU
  sprintf(
      _tmp, "%d. %s %2d. %s", network.timeinfo.tm_year + 1900, LANG::mnths[network.timeinfo.tm_mon], network.timeinfo.tm_mday,
      LANG::dowf[network.timeinfo.tm_wday]);
#elif L10N_LANGUAGE == PL
  sprintf(
      _tmp, "%s %02d %s %04d", LANG::dowf[network.timeinfo.tm_wday], network.timeinfo.tm_mday, LANG::mnths[network.timeinfo.tm_mon],
      network.timeinfo.tm_year + 1900);
#elif L10N_LANGUAGE == GR
  sprintf(_tmp, "%2d %s %d", network.timeinfo.tm_mday, LANG::mnths[network.timeinfo.tm_mon], network.timeinfo.tm_year + 1900);
#elif L10N_LANGUAGE == SK
  sprintf(
      _tmp, "%s %d. %s %2d", LANG::dowf[network.timeinfo.tm_wday], network.timeinfo.tm_mday, LANG::mnths[network.timeinfo.tm_mon], network.timeinfo.tm_year + 1900);
#elif L10N_LANGUAGE == UA
  sprintf(
      _tmp, "%s, %d %s %2d року", LANG::dowf[network.timeinfo.tm_wday], network.timeinfo.tm_mday, LANG::mnths[network.timeinfo.tm_mon],
      network.timeinfo.tm_year + 1900);
#elif L10N_LANGUAGE == DE
  sprintf(
      _tmp, "%s, %02d. %s %d", LANG::dowf[network.timeinfo.tm_wday], network.timeinfo.tm_mday, LANG::mnths[network.timeinfo.tm_mon],
      network.timeinfo.tm_year + 1900);

#endif
}

void ClockWidget::_clearClock()
{
#ifdef PSFBUFFER
  if (_fb->ready())
  {
    _fb->clear();
  }
  else
#endif
#ifndef CLOCKFONT5x7
    // dsp.fillRect(_left(), _top()-_timeheight, _clockwidth, _clockheight+1, 0x8410);
    dsp.fillRect(_left(), _top() - (_timeheight + 2), _clockwidth, _clockheight + 3, config.theme.background);
// Serial.println("Törlés");
#else
  dsp.fillRect(_left(), _top(), _clockwidth + 1, _clockheight + 1, config.theme.background);
#endif
}

void ClockWidget::draw(bool force)
{
  if (!_active)
  {
    return;
  }
  _printClock(_getTime() || force);
}

void ClockWidget::_draw()
{
  if (!_active)
  {
    return;
  }
  _printClock(true);
}

void ClockWidget::_reset()
{
#ifdef PSFBUFFER
  if (_fb->ready())
  {
    _fb->freeBuffer();
    _getTimeBounds();
    _begin();
  }
#endif
}

void ClockWidget::_clear()
{
  _clearClock();
}
#else  // #ifndef DSP_LCD

void ClockWidget::_printClock(bool force)
{
  strftime(_timebuffer, sizeof(_timebuffer), "%H:%M", &network.timeinfo);
  if (force)
  {
    dsp.setCursor(dsp.width() - 5, 0);
    dsp.print(_timebuffer);
  }
  dsp.setCursor(dsp.width() - 5 + 2, 0);
  dsp.print((network.timeinfo.tm_sec % 2 == 0) ? ":" : " ");
}

void ClockWidget::_clearClock() {}

void ClockWidget::draw()
{
  if (!_active)
  {
    return;
  }
  _printClock(true);
}
void ClockWidget::_draw()
{
  if (!_active)
  {
    return;
  }
  _printClock(true);
}
void ClockWidget::_reset() {}
void ClockWidget::_clear() {}
#endif // #ifndef DSP_LCD

/**************************
      BITRATE WIDGET
 **************************/
void BitrateWidget::init(BitrateConfig bconf, uint16_t fgcolor, uint16_t bgcolor)
{
  Widget::init(bconf.widget, fgcolor, bgcolor);
  _dimension = bconf.dimension;
  _bitrate = 0;
  _format = BF_UNKNOWN;
  _charSize(bconf.widget.textsize, _charWidth, _textheight);
  memset(_buf, 0, 6);
  // Serial.printf("widgets.cpp->BitrateWidget _init() _dimension %d\n", _dimension) ;
}

void BitrateWidget::setBitrate(uint16_t bitrate)
{
  //  Serial.printf("widgets.cpp->BitrateWidget setBitrate() bitrate: %d \n", bitrate) ;
  _bitrate = bitrate; // Módosítás
                      //  if(_bitrate>999) _bitrate = 999;
  if (_bitrate > 20000)
  {
    _bitrate = _bitrate / 1000;
  }
  _draw();
}

void BitrateWidget::setFormat(BitrateFormat format)
{
  //  Serial.printf("widgets.cpp->BitrateWidget setFormat() format: %d \n", format) ;
  _format = format;
  _draw();
}

// TODO move to parent
void BitrateWidget::_charSize(uint8_t textsize, uint8_t &width, uint16_t &height)
{
#ifndef DSP_LCD
  width = textsize * CHARWIDTH;
  height = textsize * CHARHEIGHT;
#else
  width = 1;
  height = 1;
#endif
}

void BitrateWidget::_draw()
{ // Módosítás
  _clear();
  // Serial.printf("widgets.cpp->BitrateWidget _draw() _active: %d _format: %d _bitrate %d \n", _active, _format, _bitrate) ;
  if (!_active)
  {
    return;
  }
  // For Bluetooth mode: do not draw bitrate frame/icon when not connected
  if (config.getMode() == PM_BLUETOOTH)
  {
    bt_metadata_t local_check;
    bt_meta_snapshot(&local_check);
    if (!local_check.connected)
    {
      _clear();
      return; // nothing to draw when Bluetooth not connected
    }
  }
  // Normally bail out when unknown format or zero bitrate, but for Bluetooth
  // we want to display the play/pause icon even if bitrate==0.
  if (config.getMode() != PM_BLUETOOTH && (_format == BF_UNKNOWN || _bitrate == 0))
  {
    return;
  }
  if (config.store.nameday)
  { //  Ha be van kapcsolva a nameday Módosítás "nameday"
    dsp.drawRect(_config.left, _config.top, _dimension * 2, (_dimension / 2) - 6, _fgcolor);
    dsp.fillRect(_config.left + _dimension, _config.top, _dimension, (_dimension / 2) - 6, _fgcolor);
    // Serial.printf("widgets.cpp->BitrateWidget _draw() config.store.nameday: %d \n", config.store.nameday) ;
  }
  else
  {
    dsp.drawRect(_config.left, _config.top, _dimension, _dimension, _fgcolor);                              // Eredeti.
    dsp.fillRect(_config.left, _config.top + _dimension / 2 + 1, _dimension, _dimension / 2 - 1, _fgcolor); // Eredeti
    // Serial.printf("widgets.cpp->BitrateWidget _draw() config.store.nameday: %d \n", config.store.nameday) ;
  }
  // If current mode is Bluetooth, draw small play/pause icon instead of bitrate
  if (config.getMode() == PM_BLUETOOTH)
  {
    bt_metadata_t local;
    bt_meta_snapshot(&local);
    // If not connected and not recently seen, do not draw BT icon.
    bool recentlySeen = false;
    if (local.lastSeen > 0)
    {
      uint32_t age = millis() - local.lastSeen;
      if (age <= bt_heartbeat_timeout_ms)
        recentlySeen = true;
    }
    if (!local.connected && !recentlySeen)
    {
      return; // leave the frame but no BT icon when not connected
    }
    bool playing = local.playing;
    // compute icon center inside widget
    int cx = _config.left + _dimension / 2;
    int cy = _config.top + _dimension / 2;
    int s = _dimension / 3; // size
    uint16_t icColor = _bgcolor == 0 ? _fgcolor : _fgcolor;
    // clear inner area
    dsp.fillRect(_config.left + 2, _config.top + 2, _dimension - 4, _dimension - 4, _bgcolor);
    if (playing)
    {
      // draw play triangle when playing (user prefers triangle while playing)
      int px1 = cx - s / 2;
      int py1 = cy - s;
      int px2 = cx - s / 2;
      int py2 = cy + s;
      int px3 = cx + s;
      int py3 = cy;
      dsp.fillTriangle(px1, py1, px2, py2, px3, py3, icColor);
    }
    else
    {
      // draw pause (two vertical bars) when stopped
      int w = max(2, s / 3);
      int h = s * 2;
      int x1 = cx - w - 2;
      int x2 = cx + 2;
      int y = cy - h / 2;
      dsp.fillRect(x1, y, w, h, icColor);
      dsp.fillRect(x2, y, w, h, icColor);
    }
    return;
  }
  dsp.setFont();
  dsp.setTextSize(_config.textsize);
  dsp.setTextColor(_fgcolor, _bgcolor);
  if (_bitrate < 999)
  {
    snprintf(_buf, 6, "%d", _bitrate); // Módisítás "bitrate"
  }
  else
  {
    float _br = (float)_bitrate / 1000;
    snprintf(_buf, 6, "%.1f", _br);
  }
  if (config.store.nameday)
  { //  Ha be van kapcsolva a nameday
    dsp.setCursor(_config.left + _dimension / 2 - _charWidth * strlen(_buf) / 2, _config.top + _dimension / 4 - _textheight / 2 - 2);
  }
  else
  {
    dsp.setCursor(_config.left + _dimension / 2 - _charWidth * 3 / 2 + 1, _config.top + (_dimension / 2) - 3 - _textheight);
  }
  dsp.print(_buf);
  dsp.setTextColor(_bgcolor, _fgcolor);
  if (config.store.nameday)
  { //  Ha be van kapcsolva a nameday
    dsp.setCursor(_config.left + _dimension + _dimension / 2 - _charWidth * 3 / 2, _config.top + _dimension / 4 - _textheight / 2 - 2);
  }
  else
  {
    dsp.setCursor(_config.left + _dimension / 2 - _charWidth * 3 / 2, _config.top + _dimension / 2 + _dimension / 4 - _textheight / 2 + 2);
  }
  switch (_format)
  {
  case BF_MP3:
    dsp.print("MP3");
    break;
  case BF_AAC:
    dsp.print("AAC");
    break;
  case BF_FLAC:
    dsp.print("FLC");
    break;
  case BF_OGG:
    dsp.print("OGG");
    break;
  case BF_WAV:
    dsp.print("WAV");
    break;
  case BF_VOR:
    dsp.print("VOR");
    break; // Módisítás "bitrate"
  case BF_OPU:
    dsp.print("OPU");
    break; // Módisítás "bitrate"
  default:
    break;
  }
}

void BitrateWidget::_clear()
{
  if (config.store.nameday)
  {                                                                                    //  Ha be van kapcsolva a nameday
    dsp.fillRect(_config.left, _config.top, _dimension * 2, _dimension / 2, _bgcolor); // lapos forma törlése
    // Serial.printf("widgets.cpp->BitrateWidget _clear() (lapos törlés) config.store.nameday: %d \n", config.store.nameday) ;
  }
  else
  {
    dsp.fillRect(_config.left, _config.top, _dimension, _dimension, _bgcolor); // négyzetes forma törlése
    // Serial.printf("widgets.cpp->BitrateWidget _clear() (négyzetes törlés) config.store.nameday: %d \n", config.store.nameday) ;
  }
}

/* Törli mindkét bitratewidget területét és a "nameday" területet is. */
void BitrateWidget::clearAll()
{
  dsp.fillRect(_config.left, _config.top, _dimension * 2, _dimension + 11, _bgcolor);
  // Serial.printf("widgets.cpp->BitrateWidget clearAll() \n") ;
}

/**************************
      PLAYLIST WIDGET
 **************************/
void PlayListWidget::init(ScrollWidget *current)
{
  Widget::init({0, 0, 0, WA_LEFT}, 0, 0);
  _current = current;
#ifndef DSP_LCD
  _plItemHeight = playlistConf.widget.textsize * (CHARHEIGHT - 1) + playlistConf.widget.textsize * 4;
  _plTtemsCount = round((float)dsp.height() / _plItemHeight);
  if (_plTtemsCount % 2 == 0)
  {
    _plTtemsCount++;
  }
  _plCurrentPos = _plTtemsCount / 2;
  _plYStart = (dsp.height() / 2 - _plItemHeight / 2) - _plItemHeight * (_plTtemsCount - 1) / 2 + playlistConf.widget.textsize * 2;
#else
  _plTtemsCount = PLMITEMS;
  _plCurrentPos = 1;
#endif
}

uint8_t PlayListWidget::_fillPlMenu(int from, uint8_t count)
{
  int ls = from;
  uint8_t c = 0;
  bool finded = false;
  // If TDA config UI is active, provide TDA items as the data source
  if (tdaConfigUI_isActive()) {
    Serial.printf("[WIDGET] TDA UI active\n");
    // Exit-confirm (saving) state
    if (tdaConfigUI_isExitConfirm()) {
      _printPLitem(0, "ZAPIS");
      _printPLitem(1, tdaConfigUI_getExitSave() ? "TAK" : "NIE");
      return 2;
    }
    size_t total = tdaConfig_getItemCount();
    Serial.printf("[WIDGET] TDA items=%u\n", (unsigned)total);
    if ((int)total == 0) return 0;
    // Edit mode: show header on top and the large value in the central row
    if (tdaConfigUI_isEditMode()) {
      _printPLitem(0, "KONFIGURACJA");
      const char *v = tdaConfig_getItemValueStr(tdaConfig_getSelectedIndex());
      _printPLitem(_plCurrentPos, v);
      return _plTtemsCount; // redraw whole area
    }
    // List mode: header on top, then list items below it. Center selection vertically.
    int sel = tdaConfig_getSelectedIndex();
    int start = sel - (_plCurrentPos - 1); // reserve row 0 for header
    if (start < 0) start = 0;
    _printPLitem(0, "KONFIGURACJA");
    // iterate and print up to 'count-1' items (one row consumed by header)
    for (int i = 0; i < (int)count - 1; i++) {
      int idx = start + i;
      if ((size_t)idx >= total) break;
      const char *label = tdaConfig_getItemLabel(idx);
      _printPLitem(1 + i, label);
      c++;
    }
    // include header in count
    return c + 1;
  }
  // If TDA config UI is active, provide configuration items as the data source
  if (tdaConfigUI_isActive()) {
    // Top static header required by spec
    _printPLitem(0, "KONFIGURACJA");
    // Save-confirm screen
    if (tdaConfigUI_isExitConfirm()) {
      const char *opt = tdaConfigUI_getExitSave() ? "TAK" : "NIE";
      _printPLitem(1, opt);
      return 2;
    }
    // Edit screen: clear list, show selected label at top and big value in center
    if (tdaConfigUI_isEditMode()) {
      int sel = tdaConfig_getSelectedIndex();
      const char *lbl = tdaConfig_getItemBaseLabel(sel);
      const char *val = tdaConfig_getItemValueStr(sel);
      // ensure header already printed at pos 0
      // print central value at highlighted position
      _printPLitem(_plCurrentPos, val);
      // also ensure the label is visible at top (reuse pos 0)
      _printPLitem(0, lbl);
      return 1;
    }
    // List mode: print items starting from position 1 (below header)
    size_t total = tdaConfig_getItemCount();
    if ((int)total == 0) return 1;
    int sel = tdaConfig_getSelectedIndex();
    int start = sel - (_plCurrentPos - 1); // leave room for header
    if (start < 0) start = 0;
    for (int i = 0; i < count; i++) {
      int idx = start + i;
      if ((size_t)idx >= total) break;
      const char *label = tdaConfig_getItemLabel(idx);
      // shift printed position by +1 to account for header
      _printPLitem(c + 1, label);
      c++;
    }
    return c + 1; // include header
  }
  // If AUX3 mode, show FM stations
  if (config.getMode() == PM_AUX3) {
    uint16_t total = config.getFmStationCount();
    Serial.printf("[FM] _fillPlMenu: total FM stations: %d\n", total);
    if (total == 0) return 0;
    int start = from;
    if (start < 0) start = 0;
    for (int i = 0; i < count; i++) {
      int idx = start + i;
      if (idx >= (int)total) break;
      float freq = config.getFmStationFreq(idx);
      char freqStr[16];
      sprintf(freqStr, "%.1f MHz", freq);
      Serial.printf("[FM] Printing item %d: %s\n", c, freqStr);
      _printPLitem(c, freqStr);
      c++;
    }
    return c;
  }
  // Default: normal playlist reading from SD
  if (config.playlistLength() == 0)
  {
    return 0;
  }
  File playlist = config.SDPLFS()->open(REAL_PLAYL, "r");
  File index = config.SDPLFS()->open(REAL_INDEX, "r");
  while (true)
  {
    if (ls < 1)
    {
      ls++;
      _printPLitem(c, "");
      c++;
      continue;
    }
    if (!finded)
    {
      index.seek((ls - 1) * 4, SeekSet);
      uint32_t pos;
      index.readBytes((char *)&pos, 4);
      finded = true;
      index.close();
      playlist.seek(pos, SeekSet);
    }
    bool pla = true;
    while (pla)
    {
      pla = playlist.available();
      String stationName = playlist.readStringUntil('\n');
      stationName = stationName.substring(0, stationName.indexOf('\t'));
      if (config.store.numplaylist && stationName.length() > 0)
      {
        stationName = String(from + c) + " " + stationName;
      }
      _printPLitem(c, stationName.c_str());
      c++;
      if (c >= count)
      {
        break;
      }
    }
    break;
  }
  playlist.close();
  return c;
}
#ifndef DSP_LCD
void PlayListWidget::drawPlaylist(uint16_t currentItem)
{
  uint8_t lastPos = _fillPlMenu(currentItem - _plCurrentPos, _plTtemsCount);
  if (lastPos < _plTtemsCount)
  {
    dsp.fillRect(0, lastPos * _plItemHeight + _plYStart, dsp.width(), dsp.height() / 2, config.theme.background);
  }
}

void PlayListWidget::_printPLitem(uint8_t pos, const char *item)
{
  dsp.setTextSize(playlistConf.widget.textsize);
  if (pos == _plCurrentPos)
  {
    _current->setText(item);
  }
  else
  {
    uint8_t plColor = (abs(pos - _plCurrentPos) - 1) > 4 ? 4 : abs(pos - _plCurrentPos) - 1;
    dsp.setTextColor(config.theme.playlist[plColor], config.theme.background);
    dsp.setCursor(TFT_FRAMEWDT, _plYStart + pos * _plItemHeight);
    dsp.fillRect(0, _plYStart + pos * _plItemHeight - 1, dsp.width(), _plItemHeight - 2, config.theme.background);
    dsp.print(utf8To(item, true));
  }
}
#else
void PlayListWidget::_printPLitem(uint8_t pos, const char *item)
{
  if (pos == _plCurrentPos)
  {
    _current->setText(item);
  }
  else
  {
    dsp.setCursor(1, pos);
    char tmp[dsp.width()] = {0};
    strlcpy(tmp, utf8To(item, true), dsp.width());
    dsp.print(tmp);
  }
}

void PlayListWidget::drawPlaylist(uint16_t currentItem)
{
  dsp.clear();
  _fillPlMenu(currentItem - _plCurrentPos, _plTtemsCount);
  dsp.setCursor(0, 1);
  dsp.write(uint8_t(126));
}
#endif // DSP_LCD

#endif // #if DSP_MODEL!=DSP_DUMMY
