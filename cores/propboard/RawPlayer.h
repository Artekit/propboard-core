/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### RawPlayer.h

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

#ifndef __RAWPLAYER_H__
#define __RAWPLAYER_H__

#include <stm32f4xx.h>
#include <ff.h>
#include "AudioSource.h"
#include "AudioFileHelper.h"

enum PlayMode
{
	PlayModeNormal = 0,
	PlayModeLoop,
	PlayModeBlocking
};

typedef struct
{
	volatile bool updated;
	uint8_t* buffer;
	uint8_t* readptr;
	uint32_t samples;
} samplesBuffer;

class playerBuffer
{
public:
	playerBuffer()
	{
		buffer_alloc = NULL;
		alloc_size = 0;
		playing_buffer = updating_buffer = NULL;
		buffers_samples = 0;
	}

	bool allocate(uint32_t samples, uint32_t sample_size)
	{
		uint32_t size = sample_size * samples;

		// Do not reallocate if we already have a buffer with the requested size
		if (buffer_alloc && size == alloc_size)
		{
			// We may have changed sample_size (stereo/mono), so recalculate buffer_samples
			buffers_samples = samples / 2;
			return true;
		}

		alloc_size = size;

		// Allocate and check
		buffer_alloc = (uint8_t*) malloc(alloc_size + 4);
		if (!buffer_alloc)
			return false;

		// Check alignment
		uint8_t* buffer_ptr = buffer_alloc;
		if ((uint32_t) buffer_ptr & 0x03)
			buffer_ptr += (4 - ((uint32_t) buffer_alloc & 0x03));

		// Both buffers can hold the same quantity of samples
		buffers_samples = samples / 2;

		// Assign double buffer pointers
		buffers[0].buffer = buffer_ptr;
		buffers[1].buffer = buffer_ptr + alloc_size / 2;
		return true;
	}

	void deallocate()
	{
		if (buffer_alloc)
			free(buffer_alloc);

		buffer_alloc = NULL;
	}

	void reset()
	{
		buffers[0].samples = buffers[1].samples = 0;
		buffers[0].updated = buffers[1].updated = false;
		buffers[0].readptr = buffers[0].buffer;
		buffers[1].readptr = buffers[1].buffer;
		playing_buffer = &buffers[0];
		updating_buffer = &buffers[1];
	}

	inline samplesBuffer* getPlayingBuffer() { return playing_buffer; }
	inline samplesBuffer* getUpdatingBuffer() { return updating_buffer; }
	inline uint32_t getBufferSamples() { return buffers_samples; }

	inline void switchBuffers()
	{
		__disable_irq();
		samplesBuffer* tmp = playing_buffer;
		playing_buffer = updating_buffer;
		updating_buffer = tmp;
		__enable_irq();
	}

private:
	uint8_t* buffer_alloc;
	uint32_t alloc_size;
	samplesBuffer buffers[2];
	samplesBuffer* playing_buffer;
	samplesBuffer* updating_buffer;
	uint32_t buffers_samples;
};

class RawPlayer : public AudioSource
{
public:
	RawPlayer();
	~RawPlayer();

	bool begin(uint32_t fs, uint8_t bps, bool mono, uint32_t hdrsize);
	void end();
	bool play(const char* filename, PlayMode mode = PlayModeNormal);
	bool playRandom(const char* filename, const char* ext, uint32_t min, uint32_t max, PlayMode mode = PlayModeNormal);
	bool replay();
	bool stop();
	char* getFileName();
	UpdateResult update();
	uint32_t getSamplesLeft();
	uint32_t duration() { return audio_file.getDuration(); }
	inline void* getNextSamplePtr()
	{
		// Don't need to check on 'end of buffer' condition. This function is called from the Audio
		// mixing function and it will call it as many times as we declared with getSamplesLeft().
		// 'End of buffer' will be checked in the mixingEnded() function.
		void* ret = buffer.getPlayingBuffer()->readptr;
		buffer.getPlayingBuffer()->readptr += sample_size;
		return ret;
	}

protected:

	bool doPlay(PlayMode mode);
	uint32_t mixingStarts(uint32_t samples);
	void mixingEnded(uint32_t samples);
	bool refill();
	bool refill(AudioFileHelper* file, playerBuffer* buffer);
	bool refillBuffer(AudioFileHelper* file, samplesBuffer* buffer, uint32_t samples);
	UpdateResult update(AudioFileHelper* file, playerBuffer* buffer);

	AudioFileHelper audio_file;
	PlayMode play_mode;
	uint32_t header_size;
	bool update_requested;

	playerBuffer buffer;
};

#endif /* __RAWPLAYER_H__ */
