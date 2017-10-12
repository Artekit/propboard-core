/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### AudioSource.cpp

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

#include <PropAudio.h>
#include "AudioSource.h"
#include "Arduino.h"

AudioSource::AudioSource()
{
#if AUDIOSOURCE_ALLOC
	buff_alloc = buffer = NULL;
#endif
	stereo = false;
	bits_per_sample = sample_rate = samples_left = sample_size = buffer_size = 0;
	next_to_mix = next_in_list = NULL;
	update_callback = NULL;
	update_callback_param = buff_alloc = read_ptr = NULL;
	new_data_callback = NULL;
	volume = 1.0f;
}

AudioSource::~AudioSource()
{
	end();
}

bool AudioSource::begin(uint32_t fs, uint8_t bps, bool mono, UpdateCallback* callback, void* callback_param)
{
	if (status != AudioSourceStopped)
		stop();

	sample_rate = fs;
	bits_per_sample = bps;
	stereo = !mono;
	sample_size = (bits_per_sample / 8) * (stereo ? 2 : 1);

	if (!allocateBuffer())
		return false;

	read_ptr = buffer;
	update_callback = callback;
	update_callback_param = callback_param;
	return true;
}

void AudioSource::end()
{
	stop();
	deallocateBuffer();
}

void AudioSource::skip(uint32_t samples)
{
	uint8_t* ptr = getReadPtr();
	uint32_t offset;

	if (samples > samples_left)
		samples = getSamplesLeft();

	offset = samples * sample_size;
	ptr += offset;
	setReadPtr(ptr);

	setSamplesLeft(samples_left - samples);
}

bool AudioSource::allocateBuffer()
{

#if AUDIOSOURCE_ALLOC
	uint32_t alloc_size = sample_size * MIN_UPDATE_SAMPLES * MIN_UPDATE_BUFFERS;

	if (buffer)
	{
		if (alloc_size == buffer_size)
			return true;

		free(buffer);
	}

	buff_alloc = (uint8_t*) malloc(alloc_size + 4);

	if (!buff_alloc)
		return false;

	buffer = buff_alloc;

	if ((uint32_t) buffer & 0x03)
		buffer += (4 - ((uint32_t) buff_alloc & 0x03));

	buffer_size = alloc_size;

#else
	buffer_size = MAX_BUFFER_SIZE;
#endif

	return true;
}

bool AudioSource::deallocateBuffer()
{
	buffer_size = 0;
	sample_size = 0;

#if AUDIOSOURCE_ALLOC
	if (buff_alloc)
	{
		free(buff_alloc);
		read_ptr = buffer = buff_alloc = NULL;
	}
#endif

	return true;
}

bool AudioSource::play()
{
	if (status == AudioSourceStopped)
	{
		// Add ourselves to the list
		if (Audio.addSource(this))
		{
			status = AudioSourcePlaying;
			return true;
		}
	}

	return false;
}

bool AudioSource::stop()
{
	if (status == AudioSourcePaused || status == AudioSourcePlaying)
	{
		status = AudioSourceStopped;
		Audio.removeSource(this);
	}

	return true;
}

bool AudioSource::pause()
{
	if (playing())
	{
		status = AudioSourcePaused;
		return true;
	}

	return false;
}

bool AudioSource::resume()
{
	if (status == AudioSourcePaused)
	{
		status = AudioSourcePlaying;
		return true;
	}

	return false;
}

void AudioSource::onNewData(uint8_t* dataptr, uint32_t samples)
{
	if (new_data_callback)
		(new_data_callback)(this, dataptr, samples);
}

void AudioSource::setNewDataCallback(newDataCallback* callback)
{
	__disable_irq();
	new_data_callback = callback;
	__enable_irq();
}

bool AudioSource::update()
{
	if (update_callback)
		return (update_callback)(this, update_callback_param);

	return false;
}

uint8_t* AudioSource::mixingStarts()
{
	return getReadPtr();
}

void AudioSource::mixingEnded(uint32_t samples)
{
	skip(samples);
}
