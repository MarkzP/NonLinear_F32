#include "effect_nonlinear_F32.h"

#include "arm_math.h"


// The interpolation/decimation filter has been designed to attenuate the upper frequencies way below nyquist.
// Tuned by ear to offer the best compromise between performance, sound quality & aliasing rejection
// https://www.arc.id.au/FilterDesign.html lpf 17.5kHz@220.5kHz, 82dB, 125 taps
float FIR_coeffs[INT_NUMTAPS] =
{
-0.000005, 
-0.000015, 
-0.000027, 
-0.000035, 
-0.000031, 
-0.000010, 
0.000030, 
0.000084, 
0.000138, 
0.000168, 
0.000152, 
0.000076, 
-0.000061, 
-0.000236, 
-0.000405, 
-0.000506, 
-0.000484, 
-0.000300, 
0.000040, 
0.000479, 
0.000910, 
0.001198, 
0.001211, 
0.000868, 
0.000173, 
-0.000760, 
-0.001715, 
-0.002416, 
-0.002594, 
-0.002072, 
-0.000835, 
0.000925, 
0.002821, 
0.004342, 
0.004973, 
0.004347, 
0.002375, 
-0.000669, 
-0.004151, 
-0.007191, 
-0.008855, 
-0.008405, 
-0.005544, 
-0.000581, 
0.005543, 
0.011383, 
0.015285, 
0.015784, 
0.012025, 
0.004102, 
-0.006778, 
-0.018375, 
-0.027755, 
-0.031829, 
-0.028006, 
-0.014819, 
0.007631, 
0.037505, 
0.071402, 
0.104874, 
0.133169, 
0.152083, 
0.158730, 
0.152083, 
0.133169, 
0.104874, 
0.071402, 
0.037505, 
0.007631, 
-0.014819, 
-0.028006, 
-0.031829, 
-0.027755, 
-0.018375, 
-0.006778, 
0.004102, 
0.012025, 
0.015784, 
0.015285, 
0.011383, 
0.005543, 
-0.000581, 
-0.005544, 
-0.008405, 
-0.008855, 
-0.007191, 
-0.004151, 
-0.000669, 
0.002375, 
0.004347, 
0.004973, 
0.004342, 
0.002821, 
0.000925, 
-0.000835, 
-0.002072, 
-0.002594, 
-0.002416, 
-0.001715, 
-0.000760, 
0.000173, 
0.000868, 
0.001211, 
0.001198, 
0.000910, 
0.000479, 
0.000040, 
-0.000300, 
-0.000484, 
-0.000506, 
-0.000405, 
-0.000236, 
-0.000061, 
0.000076, 
0.000152, 
0.000168, 
0.000138, 
0.000084, 
0.000030, 
-0.000010, 
-0.000031, 
-0.000035, 
-0.000027, 
-0.000015, 
-0.000005	
};


float AudioEffectNonLinear_F32::omega(float f)
{
	float fs = _sample_rate_Hz * (_multirate ? (float)INTERPOLATION : 1.0f);
	return 1.0f - expf(-_twoPi * f / fs);
}


bool AudioEffectNonLinear_F32::begin(bool multirate)
{
  if (multirate)
  {
    if (!_multirate)
    {
      arm_status int_init = arm_fir_interpolate_init_f32(&_interpolator, INTERPOLATION, INT_NUMTAPS, FIR_coeffs, _int_state, RAW_BUFFER_SIZE);
      arm_status dec_init = arm_fir_decimate_init_f32(&_decimator, INT_NUMTAPS, INTERPOLATION, FIR_coeffs, _decim_state, INT_BUFFER_SIZE);
      _multirate = int_init == ARM_MATH_SUCCESS && dec_init == ARM_MATH_SUCCESS;
    }
  }
  else _multirate = false;
  
  _gla = 0.002f * (_multirate ? 1.0f : (float)INTERPOLATION);

  gain();
  tone();
  bottom();
  level();

  return _multirate;
}

void AudioEffectNonLinear_F32::gain(float gain)
{
  gain = fabs(gain);
  _active = gain > 1.0f;
  _gain = gain * (_multirate ? (float)INTERPOLATION : 1.0f);
}

void AudioEffectNonLinear_F32::bottom(float bottom)
{
  bottom = bottom < 0.0f ? 0.0f : bottom > 1.0f ? 1.0f : bottom;
  bottom = 1.0f - bottom;
  	
  float fpre = (bottom * 350.0f) + 50.0f;
  float fpost = 50.0f;	

  _hpa_pre1 = omega(fpre);
  _hpa_pre2 = omega(fpre);
  _hpa_post = omega(fpost);
}

void AudioEffectNonLinear_F32::tone(float tone)
{
  tone = tone < 0.0f ? 0.0f : tone > 1.0f ? 1.0f : tone;
  
  float f1 = (tone * 8000.0f) + 800.0f;
  float f2 = 10000.0f;

  _lpa1 = omega(f1);
  _lpa2 = omega(f2);
}

void AudioEffectNonLinear_F32::level(float level)
{
  _level = level;
}

void AudioEffectNonLinear_F32::update(void)
{
  float sample; //, sabs, sign, knee;

  audio_block_f32_t *block = AudioStream_F32::receiveWritable_f32(0);
  if (!block) return;

  int len = block->length;
  float *p = block->data;
  
  if (_multirate)
  {
    // Interpolate
    arm_fir_interpolate_f32(&_interpolator, block->data, _interpolated, block->length);
    len *= INTERPOLATION;
    p = _interpolated;
  }
  
  for (int i = 0; i < len; i++)
  {
    sample = *p;

    // 2nd order high pass (to block dc & remove some low end)
    if (_active)
	{
		sample -= (_hppre1 += (sample - _hppre1) * _hpa_pre1);
		sample -= (_hppre2 += (sample - _hppre2) * _hpa_pre2);
	}
    
    // Apply smoothed gain
    sample *= (_sm_gain += (_gain - _sm_gain) * (_gain > _sm_gain ? _gla : 1.0f));
    
    if (_active)
    {
      // Hard clipper
      sample = sample < -1.0f ? -1.0f : sample > 1.0f ? 1.0f : sample;
      
      // 2nd order low pass filter (to control harmonics)
      sample = (_lpf1 += (sample - _lpf1) * _lpa1);
      sample = (_lpf2 += (sample - _lpf2) * _lpa2);    

      // 1st order high pass (to restore dc balance)
      sample -= (_hppost += (sample - _hppost) * _hpa_post);
    }
    
    // Apply smoothed level
    sample *= (_sm_level += (_level - _sm_level) * (_level > _sm_level ? _gla : 1.0f));
   
    *p = sample;
    p++;
  }

  // Decimate
  if (_multirate) arm_fir_decimate_f32(&_decimator, _interpolated, block->data, len);
  
  AudioStream_F32::transmit(block);
  AudioStream_F32::release(block);
}