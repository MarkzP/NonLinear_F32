#include "effect_nonlinear_F32.h"

#include "arm_math.h"

// The interpolation/decimation filter has been designed to attenuate the upper frequencies way below nyquist.
// Tuned by ear to offer the best compromise between performance, sound quality & aliasing rejection
// https://www.arc.id.au/FilterDesign.html lpf 12.1kHz@220.5kHz, 80dB, 55 taps
float FIR_coeffs[INT_NUMTAPS] =
{
0.000004, 
0.000036, 
0.000117, 
0.000260, 
0.000461, 
0.000684, 
0.000854, 
0.000855, 
0.000545, 
-0.000214, 
-0.001515, 
-0.003351, 
-0.005570, 
-0.007847, 
-0.009672, 
-0.010396, 
-0.009299, 
-0.005704, 
0.000895, 
0.010708, 
0.023551, 
0.038802, 
0.055421, 
0.072046, 
0.087150, 
0.099236, 
0.107049, 
0.109751, 
0.107049, 
0.099236, 
0.087150, 
0.072046, 
0.055421, 
0.038802, 
0.023551, 
0.010708, 
0.000895, 
-0.005704, 
-0.009299, 
-0.010396, 
-0.009672, 
-0.007847, 
-0.005570, 
-0.003351, 
-0.001515, 
-0.000214, 
0.000545, 
0.000855, 
0.000854, 
0.000684, 
0.000461, 
0.000260, 
0.000117, 
0.000036, 
0.000004
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

void AudioEffectNonLinear_F32::gain(float gain, float curve)
{
  gain = gain < 0.0f ? 0.0f : gain;
  curve = curve < 0.0f ? 0.0f : curve > 1.0f ? 1.0f : curve;

	float comp = 1.0f + (curve * 0.5f);
	gain /= comp;
	curve /= 3.0f;
  
	// Compensate interpolation losses
	if (_multirate) gain *= (float)INTERPOLATION;

	__disable_irq();
	_gain = gain;
  _curve = curve;
  _comp = comp;
	__enable_irq();   
}

void AudioEffectNonLinear_F32::level(float level)
{
	_level = level;
}

void AudioEffectNonLinear_F32::update(void)
{
	float sample;
	float gain = _gain;
  float curve = _curve;
	float level = _level * _comp;

	audio_block_f32_t *block = AudioStream_F32::receiveWritable_f32(0);
	if (!block) return;
	
	int len = block->length;
	float *p = block->data;
	
	// Apply gain
	arm_scale_f32(block->data, gain, block->data, block->length);
	
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
    sample = sample < -1.0f ? -1.0f : sample > 1.0f ? 1.0f : sample;
    sample -= sample * sample * sample * curve;
    *p = sample;
    p++;
  }

	// Decimate
	if (_multirate) arm_fir_decimate_f32(&_decimator, _interpolated, block->data, len);

	// Apply level
	arm_scale_f32(block->data, level, block->data, block->length);

	AudioStream_F32::transmit(block);
	AudioStream_F32::release(block);
}