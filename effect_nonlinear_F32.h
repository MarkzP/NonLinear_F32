/*
 * Non-linear waveshaper for the Teensy OpenAudio F32 library 
 * https://github.com/chipaudette/OpenAudio_ArduinoLibrary
 * crude approximation of the typical twin diode clipping stage used in distortion pedals
 *  
 *  by Marc Paquette
 */

#ifndef effect_nonlinear_f32_h_
#define effect_nonlinear_f32_h_

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"

#define INTERPOLATION   5
#define RAW_BUFFER_SIZE 128
#define INT_BUFFER_SIZE (RAW_BUFFER_SIZE * INTERPOLATION)
#define INT_NUMTAPS   65

class AudioEffectNonLinear_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:effect_NonLinear 
  public:
    AudioEffectNonLinear_F32(void): AudioStream_F32(1, inputQueueArray) { }
    AudioEffectNonLinear_F32(const AudioSettings_F32 &settings): AudioStream_F32(1, inputQueueArray) { }

    bool begin(bool multirate = true);
    
    void gain(float gain = 0.1f);
    void curve(float curve = 0.0f);
    void clip(float clip = 1.0f);
    void tone(float tone = 1.0f);
    void level(float level = 10.0f);
    
    virtual void update(void);

  private:
    audio_block_f32_t *inputQueueArray[1];
    float _gain = 1.0f;
    float _curve = 0.0f;
    float _clip = 1.0f;
    float _level = 1.0f;
    float _lpa = 1.0f;
    float _lpf = 0.0f;
    const float _dca = 0.0005f;
    float _dcpre = 0.0f;
    float _dcpost = 0.0f;
  
    arm_fir_interpolate_instance_f32 _interpolator;
    arm_fir_decimate_instance_f32 _decimator;
    float _interpolated[INT_BUFFER_SIZE];
    float _int_state[(INT_NUMTAPS / INTERPOLATION) + RAW_BUFFER_SIZE - 1];
    float _decim_state[INT_NUMTAPS + INT_BUFFER_SIZE - 1];
    bool _multirate = false;
};

#endif
