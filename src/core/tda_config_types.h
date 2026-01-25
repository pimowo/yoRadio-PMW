#pragma once
#ifndef TDA_CONFIG_TYPES_H
#define TDA_CONFIG_TYPES_H

#include <Arduino.h>

struct InputCfg {
  bool enabled;
  int8_t gain;
  bool routeToSub;
};

struct TDASettings {
  // Master volume stored in dB: -79 .. 0
  int16_t masterVolume;
  bool mute;
  // balance and fader
  int8_t balance; // -15..15
  int8_t fader;   // -15..15 (front<->rear)

  // Tone controls
  int8_t bassGain; // -15..15 dB
  uint8_t bassCenterIdx; // 0..3 mapping to frequencies
  uint8_t bassQIdx; // 0..3 mapping to Q values
  bool bassDC;

  int8_t midGain; // -15..15
  uint8_t midCenterIdx; // 0..3
  uint8_t midQIdx; // 0..3

  int8_t trebleGain; // -15..15
  uint8_t trebleCenterIdx; // 0..3

  // Loudness
  int8_t loudnessLevel; // 0..15 dB
  uint8_t loudnessFreqIdx; // 0..3 (Flat/400/800/2400)
  bool loudnessHighBoost;

  // Outputs levels (dB): 0 .. -79
  int16_t outL;
  int16_t outR;
  int16_t outSub;

  // Subwoofer filter selection
  uint8_t subFilterIdx; // 0..3 (Flat/80/120/160)

  // Soft
  uint8_t softStepTimeIdx; // 0..1 (5ms/10ms)
  bool softMute;

  // Inputs
  InputCfg inputs[5];
  bool inputInvert[5];

  // Diagnostics
  bool dcDetect;
  bool levelMeterOut;
  // UI feedback: beep on navigation/confirm inside configuration UI
  bool winIn;

  uint8_t schema_version;
  uint32_t crc32;
};

#endif
