#include "TDA7719.h"
#include <Wire.h>

TDA7719::TDA7719(){
  // do not call Wire.begin() here if I2C already initialized elsewhere
  Wire.begin();
}

static inline uint8_t to7bit(uint8_t subaddr){ return subaddr; }

void TDA7719::writeWire(uint8_t a, uint8_t b){
  Wire.beginTransmission(TDA7719_address);
  Wire.write(a);
  Wire.write(b);
  Wire.endTransmission();
}

// Helper: clamp int to range
static int clampInt(int v, int lo, int hi){ if (v < lo) return lo; if (v > hi) return hi; return v; }

void TDA7719::setInput(int input, int md, int input_gain, int input_conf){
  input = clampInt(input, 0, 7);
  md = (md ? 0b00001000 : 0);
  input_gain = (input_gain ? 0b00010000 : 0);
  input_conf = (input_conf & 7) << 5;
  uint8_t input_sum = (uint8_t)input | (uint8_t)md | (uint8_t)input_gain | (uint8_t)input_conf;
  writeWire(TDA7719_INPUT, input_sum);
}

void TDA7719::setInput_2(int sell, int md_2, int input_gain_2, int bypass_front, int bypass_rear, int bypass_sub) {
  sell = clampInt(sell, 0, 7);
  md_2 = (md_2 ? 0b00001000 : 0);
  input_gain_2 = (input_gain_2 ? 0b00010000 : 0);
  bypass_front = (bypass_front ? 0b00100000 : 0);
  bypass_rear = (bypass_rear ? 0b01000000 : 0);
  bypass_sub = (bypass_sub ? 0b10000000 : 0);
  uint8_t v = (uint8_t)sell | (uint8_t)md_2 | (uint8_t)input_gain_2 | (uint8_t)bypass_front | (uint8_t)bypass_rear | (uint8_t)bypass_sub;
  writeWire(TDA7719_INPUT_2, v);
}

void TDA7719::setMix_source(int mix_sell, int mix_att){
  mix_sell = clampInt(mix_sell, 0, 7);
  // mix_att is -31..0 mapped to bit pattern; for simplicity map 0..31 -> pattern by offset
  int ma = clampInt(mix_att, -31, 0);
  // convert to 3-bit groups as in original table: we pack into high bits; here we approximate
  uint8_t att = (uint8_t)((-ma & 0x1F) << 3);
  uint8_t v = (uint8_t)mix_sell | att;
  writeWire(TDA7719_MIX_SOUR, v);
}

void TDA7719::setMix_cont(int mix_fl, int mix_fr, int mix_rl, int mix_rr, int rear_speak, int ref_out_sell, int level_metr, int dc) {
  uint8_t v = 0;
  v |= (mix_fl ? 0b00000001 : 0);
  v |= (mix_fr ? 0b00000010 : 0);
  v |= (mix_rl ? 0b00000100 : 0);
  v |= (mix_rr ? 0b00001000 : 0);
  v |= (rear_speak ? 0b00010000 : 0);
  v |= (ref_out_sell ? 0b00100000 : 0);
  v |= (level_metr ? 0b01000000 : 0);
  v |= (dc ? 0b10000000 : 0);
  writeWire(TDA7719_MIX_CONT, v);
}

void TDA7719::setMute(int mute, int pin_mute, int time_mute, int sub_in_conf, int sub_eneble, int fast, int filter) {
  uint8_t v = 0;
  v |= (mute ? 0b00000001 : 0);
  v |= (pin_mute ? 0b00000010 : 0);
  // time_mute: map 0..3 to bits at 2..3
  v |= (uint8_t)((time_mute & 3) << 2);
  v |= (sub_in_conf ? 0b00010000 : 0);
  v |= (sub_eneble ? 0b00100000 : 0);
  v |= (fast ? 0b01000000 : 0);
  v |= (filter ? 0b10000000 : 0);
  writeWire(TDA7719_MUTE, v);
}

void TDA7719::setSoft_1(int soft_loun, int soft_vol, int soft_treb, int soft_mid, int soft_bass, int soft_lf, int soft_fr, int soft_lr) {
  uint8_t v = 0;
  v |= (soft_loun ? 0b00000001 : 0);
  v |= (soft_vol ? 0b00000010 : 0);
  v |= (soft_treb ? 0b00000100 : 0);
  v |= (soft_mid ? 0b00001000 : 0);
  v |= (soft_bass ? 0b00010000 : 0);
  v |= (soft_lf ? 0b00100000 : 0);
  v |= (soft_fr ? 0b01000000 : 0);
  v |= (soft_lr ? 0b10000000 : 0);
  writeWire(TDA7719_SOFT_1, v);
}

void TDA7719::setSoft_2(int soft_rr, int soft_sub_l, int soft_sub_r, int soft_time, int soft_zero, int soft_time_cons){
  uint8_t v = 0;
  v |= (soft_rr ? 0b00000001 : 0);
  v |= (soft_sub_l ? 0b00000010 : 0);
  v |= (soft_sub_r ? 0b00000100 : 0);
  v |= (uint8_t)((soft_time & 1) ? 0b00001000 : 0);
  // soft_zero uses bits at 4..5
  v |= (uint8_t)((soft_zero & 3) << 4);
  v |= (uint8_t)((soft_time_cons & 3) << 6);
  writeWire(TDA7719_SOFT_2, v);
}

void TDA7719::setLoudness(int loud_att, int loud_f, int loud_b, int loud_s){
  int la = clampInt(abs(loud_att), 0, 15);
  uint8_t v = (uint8_t)la;
  v |= (loud_f ? (uint8_t)(loud_f << 4) : 0);
  v |= (loud_b ? 0b01000000 : 0);
  v |= (loud_s ? 0b10000000 : 0);
  writeWire(TDA7719_LOUD, v);
}

// setVol uses a detailed mapping table in original; here we implement a simplified mapping
void TDA7719::setVol(int gain, int out_gain, int soft_step){
  // gain expected -15..+15. We'll map to a central range and set bits accordingly.
  int g = clampInt(gain, -15, 15);
  // convert to 0..31 representation roughly
  uint8_t gval = (uint8_t)(g + 15);
  uint8_t v = (gval & 0x1F);
  v |= (out_gain ? 0b01000000 : 0);
  v |= (soft_step ? 0b10000000 : 0);
  writeWire(TDA7719_VOL, v);
}

void TDA7719::setTreble(int gain_treb, int treb_f, int soft_treb){
  int g = clampInt(gain_treb, -15, 15);
  uint8_t gv = (uint8_t)(g + 15);
  uint8_t v = gv & 0x1F;
  v |= (treb_f ? (uint8_t)(treb_f << 5) : 0);
  v |= (soft_treb ? 0b10000000 : 0);
  writeWire(TDA7719_TRBLE, v);
}

void TDA7719::setMiddle(int gain_mid, int mid_q, int soft_mid){
  int g = clampInt(gain_mid, -15, 15);
  uint8_t gv = (uint8_t)(g + 15);
  uint8_t v = gv & 0x1F;
  v |= (mid_q ? (uint8_t)(mid_q << 5) : 0);
  v |= (soft_mid ? 0b10000000 : 0);
  writeWire(TDA7719_MIDDLE, v);
}

void TDA7719::setBass(int gain_bass, int bass_q, int soft_bass){
  int g = clampInt(gain_bass, -15, 15);
  uint8_t gv = (uint8_t)(g + 15);
  uint8_t v = gv & 0x1F;
  v |= (bass_q ? (uint8_t)(bass_q << 5) : 0);
  v |= (soft_bass ? 0b10000000 : 0);
  writeWire(TDA7719_BASS, v);
}

void TDA7719::setSMB(int sub_f, int sub_out, int mid_f, int bass_f, int bass_dc) {
  uint8_t v = 0;
  v |= (sub_f & 3);
  v |= (sub_out ? 0b00000100 : 0);
  v |= (uint8_t)((mid_f & 3) << 3);
  v |= (uint8_t)((bass_f & 3) << 5);
  v |= (bass_dc ? 0b10000000 : 0);
  writeWire(TDA7719_SUB_M_B, v);
}

void TDA7719::setVol_LF(int lf, int soft_lf){
  uint8_t v = (uint8_t)(abs(lf) + 0x10);
  v |= (soft_lf ? 0b10000000 : 0);
  writeWire(TDA7719_ATT_LF, v);
}
void TDA7719::setVol_RF(int rf, int soft_rf){ uint8_t v = (uint8_t)(abs(rf) + 0x10); v |= (soft_rf ? 0b10000000 : 0); writeWire(TDA7719_ATT_RF, v); }
void TDA7719::setVol_LR(int lr, int soft_lr){ uint8_t v = (uint8_t)(abs(lr) + 0x10); v |= (soft_lr ? 0b10000000 : 0); writeWire(TDA7719_ATT_LR, v); }
void TDA7719::setVol_RR(int rr, int soft_rr){ uint8_t v = (uint8_t)(abs(rr) + 0x10); v |= (soft_rr ? 0b10000000 : 0); writeWire(TDA7719_ATT_RR, v); }

void TDA7719::setVol_SUB_L(int sl, int soft_sl){ uint8_t v = (uint8_t)(abs(sl) + 0x10); v |= (soft_sl ? 0b10000000 : 0); writeWire(TDA7719_ATT_SUB_L, v); }
void TDA7719::setVol_SUB_R(int sr, int soft_sr){ uint8_t v = (uint8_t)(abs(sr) + 0x10); v |= (soft_sr ? 0b10000000 : 0); writeWire(TDA7719_ATT_SUB_R, v); }

void TDA7719::setTest1(int x0, int x1, int x2, int x3){
  uint8_t v = 0;
  v |= (x0 ? 0b00000001 : 0);
  v |= (uint8_t)((x1 & 7) << 1);
  v |= (uint8_t)((x2 & 1) << 3);
  v |= (uint8_t)((x3 & 1) << 6);
  writeWire(TDA7719_TEST_1, v);
}

void TDA7719::setTest2(int y0, int y1, int y2, int y3){
  uint8_t v = 0;
  v |= (y0 ? 0b00000001 : 0);
  v |= (uint8_t)((y1 & 1) << 1);
  v |= (uint8_t)((y2 & 1) << 2);
  v |= (uint8_t)((y3 & 3) << 3);
  writeWire(TDA7719_TEST_2, v);
}
