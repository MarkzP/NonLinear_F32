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
#define INT_NUMTAPS   45

class AudioEffectNonLinear_F32 : public AudioStream_F32
{
//GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:effect_NonLinear	
	public:
		typedef enum
		{
			Drive_Linear,
			Drive_Abs,
			Drive_Sigmoid,
			Drive_Tanh,
			Drive_Cubic,
			Drive_Hard,
		} DriveTypes;
		
		AudioEffectNonLinear_F32(void): AudioStream_F32(1, inputQueueArray) { }
		AudioEffectNonLinear_F32(const AudioSettings_F32 &settings): AudioStream_F32(1, inputQueueArray) { }

		bool begin(bool multirate = true);
		
		// Gain: 0.0 (almost linear) to 1.0 (overdrive)
		void gain(float gain = 1.0f, DriveTypes drive = Drive_Sigmoid);
		
		void level(float level = 1.0f);
		
		virtual void update(void);

	private:
		audio_block_f32_t *inputQueueArray[1];
		DriveTypes _driveType = Drive_Sigmoid;
		float _gain = 1.0f;
		float _comp = 1.0f;
		float _level = 1.0f;
	
		arm_fir_interpolate_instance_f32 _interpolator;
		arm_fir_decimate_instance_f32 _decimator;
		float _interpolated[INT_BUFFER_SIZE];
		float _int_state[(INT_NUMTAPS / INTERPOLATION) + RAW_BUFFER_SIZE - 1];
		float _decim_state[INT_NUMTAPS + INT_BUFFER_SIZE - 1];
		bool _multirate = false;
};

#endif
