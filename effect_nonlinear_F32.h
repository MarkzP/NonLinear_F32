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
#define INT_NUMTAPS   125


class AudioEffectNonLinear_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:effect_NonLinear 
  public:
    AudioEffectNonLinear_F32(void): AudioStream_F32(1, inputQueueArray)
    {
      _sample_rate_Hz = AUDIO_SAMPLE_RATE_EXACT;
    }

    AudioEffectNonLinear_F32(const AudioSettings_F32 &settings): AudioStream_F32(1, inputQueueArray)
    {
      _sample_rate_Hz = settings.sample_rate_Hz;
    }    
    
    bool begin(bool multirate = true);
    
    void gain(float gain = 1.0);
    void tone(float tone = 1.0f);
    void bottom(float bottom = 1.0f);
    void level(float level = 1.0f);
    
    virtual void update(void);

  private:
    static constexpr float _oneThird = 1.0f / 3.0f;
    static constexpr float _twoThirds = 2.0f / 3.0f;
    static constexpr float _twoPi = 6.2831853f;
	
	inline float omega(float f);
  
    audio_block_f32_t *inputQueueArray[1];
    float _sample_rate_Hz;
    bool _active = false;
    float _gla = 0.0f;
    float _gain = 0.0f;
    float _sm_gain = 0.0f;
    float _level = 0.0f;
    float _sm_level = 0.0f;
    float _lpa1 = 0.0f;
    float _lpf1 = 0.0f;
    float _lpa2 = 0.0f;
    float _lpf2 = 0.0f;
    float _hpa_pre1 = 0.0f;
    float _hppre1 = 0.0f;
    float _hpa_pre2 = 0.0f;
    float _hppre2 = 0.0f;	
    float _hpa_post = 0.0f;
    float _hppost = 0.0f;

    arm_fir_interpolate_instance_f32 _interpolator;
    arm_fir_decimate_instance_f32 _decimator;
    float _interpolated[INT_BUFFER_SIZE];
    float _int_state[(INT_NUMTAPS / INTERPOLATION) + RAW_BUFFER_SIZE - 1];
    float _decim_state[INT_NUMTAPS + INT_BUFFER_SIZE - 1];
    bool _multirate = false;
};

#endif
