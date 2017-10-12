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

#define MAX_BITS_PER_SAMPLE		24
#define MAX_BYTES_PER_SAMPLE	(MAX_BITS_PER_SAMPLE / 8)
#define MAX_SAMPLE_RATE_HZ		96000
#define MAX_SOURCE_CHANNELS		2
#define MIN_UPDATE_SAMPLES		512 // Same as MIN_OUTPUT_SAMPLES in Audio.h
#define MIN_UPDATE_BUFFERS		2
#define MAX_BUFFER				(MIN_UPDATE_SAMPLES * MAX_SOURCE_CHANNELS * MAX_BYTES_PER_SAMPLE * MIN_UPDATE_BUFFERS)
#define AUDIOSOURCE_ALLOC		1
#define INVALID_VOLUME_VALUE	0xFF

enum AudioSourceStatus
{
	AudioSourceStopped = 0,
	AudioSourcePlaying,
	AudioSourcePaused
};

class AudioSource;
class PropAudio;

typedef bool (UpdateCallback)(AudioSource*, void*);
typedef void (newDataCallback)(AudioSource*, uint8_t*, uint32_t);

class AudioSource
{
	friend class PropAudio;

public:
	AudioSource();
	virtual ~AudioSource();

	bool begin(uint32_t fs, uint8_t bps, bool mono, UpdateCallback* callback = NULL, void* callback_param = NULL);
	void end();

	virtual bool play();
	virtual bool stop();
	virtual bool pause();
	virtual bool resume();
	virtual bool update();
	virtual void skip(uint32_t samples);
	virtual inline uint8_t* getBufferPtr() { return buffer; }
	virtual inline uint8_t* getReadPtr() { return read_ptr; }
	virtual inline void setReadPtr(uint8_t* ptr) { read_ptr = ptr; }
	virtual inline uint8_t* getRelativeDataPtr(uint8_t* ptr, uint32_t offset) { return ptr + offset; }
	virtual inline uint32_t getSamplesLeft() { return samples_left; }
	virtual inline uint32_t getBufferSize() { return buffer_size; }
	virtual inline bool isStereo() { return stereo; }
	virtual inline uint32_t sampleRate() { return sample_rate; }
	virtual inline uint8_t bitsPerSample() { return bits_per_sample; }
	virtual inline AudioSourceStatus getStatus() { return status; }
	virtual inline bool playing() { return status == AudioSourcePlaying; }
	virtual inline uint32_t getSampleSize() { return sample_size; }
	virtual inline void setSamplesLeft(uint32_t samples) { samples_left = samples; }
	virtual void setVolume(float value) { volume = value; }
	virtual float getVolume() { return volume; }
	void setNewDataCallback(newDataCallback* callback);

protected:
	
	bool allocateBuffer();
	bool deallocateBuffer();
	void onNewData(uint8_t* dataptr, uint32_t samples);
	
	virtual uint8_t* mixingStarts();
	virtual void mixingEnded(uint32_t samples);

	inline void setNextToMix(AudioSource* next) { next_to_mix = next; }
	inline AudioSource* getNextToMix() { return next_to_mix; }
	inline void setNextInList(AudioSource* next) { next_in_list = next; }
	inline AudioSource* getNextInList() { return next_in_list; }

	bool stereo;
	uint8_t bits_per_sample;
	uint32_t sample_rate;
	uint32_t samples_left;
	uint8_t sample_size;
	uint32_t buffer_size;
	float volume;

#if AUDIOSOURCE_ALLOC
	uint8_t* buffer;
	uint8_t* buff_alloc;
#else
	uint8_t buffer[MAX_BUFFER_SIZE];
#endif

	uint8_t* read_ptr;

	volatile AudioSourceStatus status;

private:
	newDataCallback* new_data_callback;
	UpdateCallback* update_callback;
	void* update_callback_param;
	AudioSource* next_to_mix;
	AudioSource* next_in_list;
};

#endif /* __AUDIOSOURCE_H__ */
