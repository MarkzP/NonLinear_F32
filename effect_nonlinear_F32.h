/*
 * Non-linear waveshaper for the Teensy OpenAudio F32 library 
 * https://github.com/chipaudette/OpenAudio_ArduinoLibrary
 * crude approximation of the typical twin diode clipping stage used in distortion pedals
 * curves: 
 *  ~0.0 : identity - completely linear (but useless for distortion!)
 *  ~0.5 : slight "tube" warmth
 *  ~1.0 : soft klipping
 *  ~3.5 : nice overdrive
 *  >5.0 : metal 
 *  
 *  by Marc Paquette
 */

#ifndef effect_nonlinear_f32_h_
#define effect_nonlinear_f32_h_

#include "OpenAudio_ArduinoLibrary.h"
#include "AudioStream_F32.h"

class AudioEffectNonLinear_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:effect_NonLinear	
  public:
    AudioEffectNonLinear_F32(void): AudioStream_F32(1, inputQueueArray) {}
    AudioEffectNonLinear_F32(const AudioSettings_F32 &settings): AudioStream_F32(1, inputQueueArray) {}
    
    void gain(float gain = 1.0f)
    {
      __disable_irq();
      aGain = gain;
      set_mix();
      __enable_irq();
    }    
    
    void curve(float curve = 0.0f)
    {
      if (curve < 0.0f) curve = 0.0f;

      __disable_irq();
      aCurve = curve;
      set_mix();
      __enable_irq();
    }
    
    void offset(float offset = 0.0f)
    {
      aOffset = offset;
    }

    void clip(float clip = 1.0f)
    {
      if (clip < 0.0f) clip = 0.0f;

      __disable_irq();
      anClip = -clip;
      apClip = clip;
      __enable_irq();
    }
    
    void tone(float tone = 1.0f)
    {
      if (tone < 0.0f) tone = 0.0f;
      else if (tone > 1.0f) tone = 1.0f;

      aLp = 0.1f + (0.9f * (exp2f(tone) - 1.0f));
    }
    
    void level(float level = 1.0f)
    {
      if (level < -1.0f) level = -1.0f;
      else if (level > 1.0f) level = 1.0f;
      
      aLevel = level;
    }

    void set_mix()
    {
      aDry = 1.0f / (1.0f + fabsf(aGain) + (aCurve * 1.5f));
      aWet = 1.0f - aDry;      
    }

    virtual void update(void)
    {
      float sample;

      float gain = aGain;
      float curve = aCurve;
      float nclip = anClip;
      float pclip = apClip;
      float offset = aOffset;
      float level = aLevel;
      float wet = aWet;
      float dry = aDry;
      float lp = aLp;
      float last = aLast;

      audio_block_f32_t *block = AudioStream_F32::receiveWritable_f32(0);
      if (!block) return;

      for (int i = 0; i < block->length; i++)
      {
        sample = block->data[i] * gain;
        
        sample = (curve + 1.0f) * sample / (1.0f + fabsf(curve * sample));
        sample += offset;

        if (sample < nclip) sample = nclip;
        if (sample > pclip) sample = pclip;

        sample *= level;
        last += (sample - last) * lp;

        block->data[i] *= dry;
        block->data[i] += last * wet;
      }

      AudioStream_F32::transmit(block);
      AudioStream_F32::release(block);

      aLast = last;
    }

  private:
    audio_block_f32_t *inputQueueArray[1];
    float aGain = 1.0f;
    float aCurve = 0.0f;
    float aOffset = 0.0f;
    float anClip = -1.0f;
    float apClip = 1.0f;
    float aLevel = 1.0f;
    float aLp = 1.0f;
    float aLast = 0.0f;
    float aDry = 1.0f;
    float aWet = 0.0f;
};

#endif
