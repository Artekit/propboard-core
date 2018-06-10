/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### AudioSource.h

#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

***************************************************************************/

#ifndef __AUDIOSOURCE_H__
#define __AUDIOSOURCE_H__

#include <stm32f4xx.h>
#include <stddef.h>

#define MAX_BITS_PER_SAMPLE			24
#define MAX_BYTES_PER_SAMPLE		(MAX_BITS_PER_SAMPLE / 8)
#define MAX_SAMPLE_RATE_HZ			96000
#define MAX_SOURCE_CHANNELS			2
#define MAX_UPDATE_BUFFER_SIZE		4096
#define INVALID_VOLUME_VALUE		0xFF
#define VOLUME_CHANGE_SAMPLES		512

enum AudioSourceStatus
{
	AudioSourceStopped = 0,
	AudioSourcePlaying,
	AudioSourcePaused
};

enum UpdateResult
{
	SourceIdling,
	SourceUpdated,
	SourceRemove,
	UpdateError
};

class AudioSource;
class PropAudio;

class AudioSource
{
	friend class PropAudio;

public:
	AudioSource();
	virtual ~AudioSource();

	bool begin(uint32_t fs, uint8_t bps, bool mono);
	void end();

	virtual bool play();
	virtual bool stop();
	virtual bool pause();
	virtual bool resume();
	virtual UpdateResult update();
	virtual uint32_t getSamplesLeft();
	virtual inline void* getNextSamplePtr()			{ return NULL; }
	virtual inline bool isStereo() 					{ return stereo; }
	virtual inline uint32_t sampleRate() 			{ return sample_rate; }
	virtual inline uint8_t bitsPerSample() 			{ return bits_per_sample; }
	virtual inline AudioSourceStatus getStatus() 	{ return status; }
	virtual inline bool playing() 					{ return status == AudioSourcePlaying; }
	virtual inline uint32_t getSampleSize() 		{ return sample_size; }
	virtual inline float getVolume() 				{ return current_volume; }
	virtual void setVolume(float value);

protected:
	
	void changeVolume(uint8_t* samples_ptr, uint32_t samples);
	
	virtual uint32_t mixingStarts(uint32_t samples) = 0;
	virtual void mixingEnded(uint32_t samples) = 0;

	inline void setNextToMix(AudioSource* next) { next_to_mix = next; }
	inline AudioSource* getNextToMix() { return next_to_mix; }
	inline void setNextInList(AudioSource* next) { next_in_list = next; }
	inline AudioSource* getNextInList() { return next_in_list; }

	bool stereo;
	uint8_t bits_per_sample;
	uint32_t sample_rate;
	uint8_t sample_size;
	float current_volume;
	float target_volume;
	float target_volume_step;
	uint32_t target_volume_samples;
	volatile AudioSourceStatus status;

private:
	AudioSource* next_to_mix;
	AudioSource* next_in_list;
};

#endif /* __AUDIOSOURCE_H__ */
