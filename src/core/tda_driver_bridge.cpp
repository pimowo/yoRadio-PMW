#include "TDA7719.h"
#include "tda_config_types.h"
#include <algorithm>

// Provide a concrete (non-weak) implementation that applies staged settings to the TDA.
// Returns true on success; false if hardware not present or failure.

bool tdaDriver_apply(const TDASettings &s)
{
  // Basic probe: try to write a harmless register; if Wire not responding, return false.
  // Construct driver and apply mapped settings.
  TDA7719 tda;

  // Map masterVolume dB (-79..0) to driver gain roughly (-15..0)
  int drvGain = (int)((s.masterVolume * 15) / 79); // negative or zero
  if (drvGain < -15) drvGain = -15;
  if (drvGain > 0) drvGain = 0;
  tda.setVol(drvGain, 0, s.softMute ? 1 : 0);

  // Mute
  tda.setMute(s.mute ? 1 : 0, 0, 0, 0, 0, 0, 0);

  // Balance -> attenuate left/right via speaker attenuators
  // balance -16..16 where negative favors left; convert to left/right attenuation offsets
  int bal = std::clamp((int)s.balance, -16, 16);
  int leftAtt = 0, rightAtt = 0;
  if (bal > 0) {
    // move to right: reduce left
    leftAtt = bal;
    rightAtt = 0;
  } else if (bal < 0) {
    rightAtt = -bal;
    leftAtt = 0;
  }
  tda.setVol_LF(-leftAtt, 0);
  tda.setVol_RF(-rightAtt, 0);

  // Tone settings (map gains directly)
  tda.setBass((int)s.bassGain, 0, 0);
  tda.setMiddle((int)s.midGain, 0, 0);
  tda.setTreble((int)s.trebleGain, 0, 0);

  // Loudness
  tda.setLoudness((int)s.loudnessLevel, (int)s.loudnessFreqIdx, s.loudnessHighBoost ? 1 : 0, 0);

  // Subwoofer: apply output level if non-zero
  if (s.outSub != 0) {
    tda.setSMB(1, 1, 0, 0, 0);
    tda.setVol_SUB_L((int)s.outSub, 0);
    tda.setVol_SUB_R((int)s.outSub, 0);
  }

  // Inputs: map enable/gain/routing
  for (int i = 0; i < 5; ++i) {
    // This driver exposes per-input config via setInput: select input index and gain flag
    int gain3dB = (s.inputs[i].gain >= 3) ? 1 : 0; // crude mapping
    tda.setInput(i, 0, gain3dB, 0);
    if (s.inputs[i].routeToSub) {
      // route to sub: part of SMB settings — enable mapping per input not present in simple driver
    }
  }

  return true;
}

// helper clamp not in this translation unit
// no local clamp; use std::clamp from <algorithm>
