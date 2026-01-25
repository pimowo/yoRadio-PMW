// tda_driver.cpp
// Strong implementation of tdaDriver_apply that programs TDA7719 registers
// Uses existing TDA7719 helper class (I2C). Non-blocking, no delays.

#include "TDA7719.h"
#include "tda_config_types.h"
#include "Arduino.h"

// Provide strong symbol to override weak no-op in tda_config_ui.cpp
extern "C" bool tdaDriver_apply(const TDASettings &s)
{
  // Single static device instance to avoid repeated Wire.begin()
  static TDA7719 dev;

  // Map master volume and per-output attenuation
  // TDA7719::setVol expects relative gain; we use masterVolume (-79..0)
  // Convert to an internal -15..+15 approximation for setVol()
  int masterGain = s.masterVolume;
  if (masterGain < -79) masterGain = -79;
  if (masterGain > 0) masterGain = 0;
  // Map -79..0 -> -15..0 (approx)
  int vol_mapped = (int)((masterGain + 79) * -15 / 79);
  dev.setVol(vol_mapped, 0, (int)s.softStepTimeIdx);

  // Mute handling
  dev.setMute(s.mute ? 1 : 0, 0, 0, 0, (s.outSub != 0) ? 1 : 0, 0, 0);

  // Tone controls
  dev.setBass((int)s.bassGain, (int)s.bassQIdx, 0);
  dev.setMiddle((int)s.midGain, (int)s.midQIdx, 0);
  dev.setTreble((int)s.trebleGain, (int)s.trebleCenterIdx, 0);

  // Loudness mapping: level and frequency selection
  dev.setLoudness((int)s.loudnessLevel, (int)s.loudnessFreqIdx, s.loudnessHighBoost ? 1 : 0, 0);

  // Balance / fader: translate to per-channel attenuation
  // balance -15..15: negative -> left louder; we map to attenuation on opposite channel
  int bal = s.balance; if (bal < -15) bal = -15; if (bal > 15) bal = 15;
  int fader = s.fader; if (fader < -15) fader = -15; if (fader > 15) fader = 15;
  // compute simple per-channel offsets (-15..15)
  int left_offset = -bal - fader; // conservative mapping
  int right_offset = bal - fader;
  // Apply per-output attenuation (abs + clamp)
  dev.setVol_LF(abs(left_offset), 0);
  dev.setVol_RF(abs(right_offset), 0);
  // Rear channels mapped to same as front for mono setups
  dev.setVol_LR(0, 0);
  dev.setVol_RR(0, 0);

  // Subwoofer configuration
  // outSub is attenuation in dB (-79..0). Map to sub L/R attenuation
  int sub = s.outSub;
  if (sub < -79) sub = -79; if (sub > 0) sub = 0;
  int sub_att = (int)(abs(sub) / 5); // coarse mapping
  dev.setVol_SUB_L(sub_att, 0);
  dev.setVol_SUB_R(sub_att, 0);
  // SMB: choose filter and enable sub output flag
  dev.setSMB((int)s.subFilterIdx, (s.outSub!=0)?1:0, 0, 0, s.bassDC?1:0);

  // Soft settings
  dev.setSoft_1(s.winIn?1:0, 0, 0, 0, 0, 0, 0, 0);
  dev.setSoft_2(0, 0, 0, (int)s.softStepTimeIdx, 0, 0);

  // Inputs: map staged inputs[] to device input selectors
  // For each input: enable, gain, routeToSub, inputInvert
  for (int i = 0; i < 5; i++) {
    const InputCfg &ic = s.inputs[i];
    // TDA helper setInput takes input selector and flags; use gain>0 as flag
    dev.setInput(i, 0, (ic.gain!=0)?1:0, ic.enabled?0:0);
    // second input register allows bypass/sub routing; try to set routeToSub
    dev.setInput_2(i, 0, (ic.gain!=0)?1:0, 0, 0, ic.routeToSub?1:0);
  }

  // Final apply: set per-output attenuation using outL/outR
  // outL/outR are stored as dB negative numbers; map to attenuation index
  int outL = s.outL; if (outL < -79) outL = -79; if (outL > 0) outL = 0;
  int outR = s.outR; if (outR < -79) outR = -79; if (outR > 0) outR = 0;
  dev.setVol_LF(abs(outL), 0);
  dev.setVol_RF(abs(outR), 0);

  // No blocking operations; assume writes succeed. Return true to indicate applied.
  return true;
}
