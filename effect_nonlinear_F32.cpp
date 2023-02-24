#include "effect_nonlinear_F32.h"

#include "arm_math.h"

// The interpolation/decimation filter has been designed to attenuate the upper frequencies way below nyquist.
// Tuned by ear to offer the best compromise between performance, sound quality & aliasing rejection
// https://www.arc.id.au/FilterDesign.html lpf 10.25kHz@220.5kHz, 80dB, 45 taps
float FIR_coeffs[INT_NUMTAPS] = {
0.000005, 
-0.000018, 
-0.000111, 
-0.000328, 
-0.000718, 
-0.001305, 
-0.002059, 
-0.002866, 
-0.003507, 
-0.003656, 
-0.002897, 
-0.000775, 
0.003138, 
0.009152, 
0.017377, 
0.027655, 
0.039523, 
0.052228, 
0.064793, 
0.076123, 
0.085146, 
0.090962, 
0.092971, 
0.090962, 
0.085146, 
0.076123, 
0.064793, 
0.052228, 
0.039523, 
0.027655, 
0.017377, 
0.009152, 
0.003138, 
-0.000775, 
-0.002897, 
-0.003656, 
-0.003507, 
-0.002866, 
-0.002059, 
-0.001305, 
-0.000718, 
-0.000328, 
-0.000111, 
-0.000018, 
0.000005
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

void AudioEffectNonLinear_F32::gain(float gain, DriveTypes driveType)
{
	float g1 = gain < 0.0f ? 0.0f : gain > 1.0f ? 1.0f : gain;
	float g2 = g1 * g1;
	float comp = 1.0f - (g1 * 0.8f) - (g2 * 0.1f);
	const float maxGain = 150.0f;
	
	switch (driveType)
	{
		case Drive_Abs:
			gain = (g2 * maxGain * 0.6f) + 0.6f;
			break;
		case Drive_Sigmoid:
			gain = (g1 * maxGain * 0.02f) + (g2 * maxGain * 1.73f) + 1.75f;
			comp *= 0.63661977f;
			break;
		case Drive_Tanh:
			gain = (g2 * maxGain * 1.25f) + 1.1f;
			break;
		case Drive_Cubic:
			gain = (g2 * maxGain * 0.75f) + 0.7f;
			comp *= 1.5f;
			break;
		case Drive_Hard:
			gain = (g2 * maxGain) + 1.0f;
			break;
		default: break;    
	}

	// Compensate interpolation losses
	if (_multirate) gain *= (float)INTERPOLATION;

	__disable_irq();
	_driveType = driveType;
	_gain = gain;
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
	float level = _level * _comp;

	audio_block_f32_t *block = AudioStream_F32::receiveWritable_f32(0);
	if (!block) return;
	
	if (_driveType == Drive_Linear)
	{
		AudioStream_F32::transmit(block);
		AudioStream_F32::release(block);
		return;
	}

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

	switch (_driveType)
	{
		case Drive_Abs:
			for (int i = 0; i < len; i++)
			{
				sample = *p;
				sample = sample < -1.0f ? -1.0f : sample > 1.0f ? 1.0f : sample;
				sample *= 2.0f - fabsf(sample);
				*p = sample;
				p++;
			}
			break;
		case Drive_Sigmoid:
			for (int i = 0; i < len; i++)
			{
				*p = atanf(*p);   
				p++;
			}
			break;
		case Drive_Tanh:
			for (int i = 0; i < len; i++)
			{
				*p = tanhf(*p);
				p++;
			}
			break;
		case Drive_Cubic:
			for (int i = 0; i < len; i++)
			{
				sample = *p;
				sample = sample < -1.0f ? -1.0f : sample > 1.0f ? 1.0f : sample;
				sample -= sample * sample * sample * 0.33333333f;
				*p = sample;
				p++;
			}
			break;
		case Drive_Hard:
			for (int i = 0; i < len; i++)
			{
				sample = *p;
				sample = sample < -1.0f ? -1.0f : sample > 1.0f ? 1.0f : sample;
				*p = sample;
				p++;
			}
			break;
		default: break;
	}
	
	// Decimate
	if (_multirate) arm_fir_decimate_f32(&_decimator, _interpolated, block->data, len);

	// Apply level
	arm_scale_f32(block->data, level, block->data, block->length);

	AudioStream_F32::transmit(block);
	AudioStream_F32::release(block);
}