#pragma once
#ifndef TDA7719_H
#define TDA7719_H

#include <Arduino.h>

// 7-bit I2C address
#define TDA7719_address 0x44

// Sub addresses
#define TDA7719_INPUT        0b00000000
#define TDA7719_INPUT_2      0b00000001
#define TDA7719_MIX_SOUR     0b00000010
#define TDA7719_MIX_CONT     0b00000011
#define TDA7719_MUTE         0b00000100
#define TDA7719_SOFT_1       0b00000101
#define TDA7719_SOFT_2       0b00000110
#define TDA7719_LOUD         0b00000111
#define TDA7719_VOL          0b00001000
#define TDA7719_TRBLE        0b00001001
#define TDA7719_MIDDLE       0b00001010
#define TDA7719_BASS         0b00001011
#define TDA7719_SUB_M_B      0b00001100
#define TDA7719_ATT_LF       0b00001101
#define TDA7719_ATT_RF       0b00001110
#define TDA7719_ATT_LR       0b00001111
#define TDA7719_ATT_RR       0b00010000
#define TDA7719_ATT_SUB_L    0b00010001
#define TDA7719_ATT_SUB_R    0b00010010
#define TDA7719_TEST_1       0b00010011
#define TDA7719_TEST_2       0b00010100

class TDA7719 {
  public:
    TDA7719();
    void setInput(int input, int md, int input_gain, int input_conf);
    void setInput_2(int sell, int md_2, int input_gain_2, int bypass_front, int bypass_rear, int bypass_sub);
    void setMix_source(int mix_sell, int mix_att);
    void setMix_cont(int mix_fl, int mix_fr, int mix_rl, int mix_rr, int rear_speak, int ref_out_sell, int level_metr, int dc);
    void setMute(int mute, int pin_mute, int time_mute, int sub_in_conf, int sub_eneble, int fast, int filter);
    void setSoft_1(int soft_loun, int soft_vol, int soft_treb, int soft_mid, int soft_bass, int soft_lf, int soft_fr, int soft_lr);
    void setSoft_2(int soft_rr, int soft_sub_l, int soft_sub_r, int soft_time, int soft_zero, int soft_time_cons);
    void setLoudness(int loud_att, int loud_f, int loud_b, int loud_s);
    void setVol(int gain, int out_gain, int soft_step);
    void setTreble(int gain_treb, int treb_f, int soft_treb);
    void setMiddle(int gain_mid, int mid_q, int soft_mid);
    void setBass(int gain_bass, int bass_q, int soft_bass);
    void setSMB(int sub_f, int sub_out, int mid_f, int bass_f, int bass_dc);
    void setVol_LF(int lf, int soft_lf);
    void setVol_RF(int rf, int soft_rf);
    void setVol_LR(int lr, int soft_lr);
    void setVol_RR(int rr, int soft_rr);
    void setVol_SUB_L(int sl, int soft_sl);
    void setVol_SUB_R(int sr, int soft_sr);
    void setTest1(int x0, int x1, int x2, int x3);
    void setTest2(int y0, int y1, int y2, int y3);
  private:
    void writeWire(uint8_t a, uint8_t b);
};

#endif // TDA7719_H
