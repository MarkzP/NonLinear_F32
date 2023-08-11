#include "effect_nonlinear_F32.h"

#include "arm_math.h"

// The interpolation/decimation filter has been designed to attenuate the upper frequencies way below nyquist.
// Tuned by ear to offer the best compromise between performance, sound quality & aliasing rejection
// https://www.arc.id.au/FilterDesign.html lpf 13.0kHz@220.5kHz, 86dB, 65 taps
float FIR_coeffs[INT_NUMTAPS] =
{
-0.000009, 
-0.000032, 
-0.000072, 
-0.000123, 
-0.000167, 
-0.000172, 
-0.000095, 
0.000108, 
0.000468, 
0.000981, 
0.001587, 
0.002162, 
0.002521, 
0.002434, 
0.001675, 
0.000079, 
-0.002387, 
-0.005565, 
-0.009059, 
-0.012241, 
-0.014290, 
-0.014294, 
-0.011384, 
-0.004897, 
0.005469, 
0.019537, 
0.036598, 
0.055436, 
0.074442, 
0.091806, 
0.105752, 
0.114786, 
0.117914, 
0.114786, 
0.105752, 
0.091806, 
0.074442, 
0.055436, 
0.036598, 
0.019537, 
0.005469, 
-0.004897, 
-0.011384, 
-0.014294, 
-0.014290, 
-0.012241, 
-0.009059, 
-0.005565, 
-0.002387, 
0.000079, 
0.001675, 
0.002434, 
0.002521, 
0.002162, 
0.001587, 
0.000981, 
0.000468, 
0.000108, 
-0.000095, 
-0.000172, 
-0.000167, 
-0.000123, 
-0.000072, 
-0.000032, 
-0.000009  
};

bool AudioEffectNonLinear_F32::begin(bool multirate)
{
  if (multirate)
  {
    arm_status int_init = arm_fir_interpolate_init_f32(&_interpolator, INTERPOLATION, INT_NUMTAPS, FIR_coeffs, _int_state, RAW_BUFFER_SIZE);
    arm_status dec_init = arm_fir_decimate_init_f32(&_decimator, INT_NUMTAPS, INTERPOLATION, FIR_coeffs, _decim_state, INT_BUFFER_SIZE);
    _multirate = int_init == ARM_MATH_SUCCESS && dec_init == ARM_MATH_SUCCESS;
    return _multirate;
  }
  
  _multirate = false;
  return true;
}

void AudioEffectNonLinear_F32::gain(float gain)
{
  _gain = gain < 0.0f ? 0.0f : gain;
}

void AudioEffectNonLinear_F32::curve(float curve)
{
  _curve = curve < 0.0f ? 0.0f : curve;
}

void AudioEffectNonLinear_F32::clip(float clip)
{
  _clip = clip < 0.0f ? 0.0f : clip > 1.0f ? 1.0f : clip;
}

void AudioEffectNonLinear_F32::tone(float tone)
{
  tone = tone < 0.0f ? 0.0f : tone > 1.0f ? 1.0f : tone;
  _lpa = (tone * tone * 0.99f) + 0.01f;
}

void AudioEffectNonLinear_F32::level(float level)
{
  _level = level < 0.0f ? 0.0f : level;
}

void AudioEffectNonLinear_F32::update(void)
{
  float sample;

  audio_block_f32_t *block = AudioStream_F32::receiveWritable_f32(0);
  if (!block) return;

  int len = block->length;
  float *p = block->data;
  
  if (_multirate)
  {
    // Compensate for interpolation losses
    arm_scale_f32(block->data, (float)INTERPOLATION, block->data, block->length);
    // Interpolate
    arm_fir_interpolate_f32(&_interpolator, block->data, _interpolated, block->length);
    len *= INTERPOLATION;
    p = _interpolated;
  }
  
  for (int i = 0; i < len; i++)
  {
    sample = *p;

    // dc block
    sample -= (_dcpre += (sample - _dcpre) * _dca);
    
    // Apply gain
    sample *= _gain;
    
    // Apply curve
    sample = (_curve + 1.0f) * sample / (1.0f + _curve * fabsf(sample));
    
    // Hard clip
    sample = sample < -_clip ? -_clip : sample > _clip ? _clip : sample;
    
    // Apply cubic
    if (sample > 0.0f) sample -= sample * sample * sample * (1.0f / 3.0f);
    
    // 1st order filter to remove harsh harmonics
    sample = (_lpf += (sample - _lpf) * _lpa);
    
    // dc block
    sample -= (_dcpost += (sample - _dcpost) * _dca);
    
    // Apply level
    sample *= _level;
   
    *p = sample;
    p++;
  }

  // Decimate
  if (_multirate) arm_fir_decimate_f32(&_decimator, _interpolated, block->data, len);

  AudioStream_F32::transmit(block);
  AudioStream_F32::release(block);
}