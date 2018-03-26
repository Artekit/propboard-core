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
#include "AudioUtil.h"

AudioSource::AudioSource()
{
	buff_alloc = buffer = NULL;
	stereo = false;
	bits_per_sample = sample_rate = samples_left = sample_size = buffer_size = 0;
	next_to_mix = next_in_list = NULL;
	update_callback = NULL;
	update_callback_param = buff_alloc = read_ptr = NULL;
	new_data_callback = NULL;
	current_volume = 1.0f;
	float_array = NULL;

	target_volume = current_volume;
	target_volume_samples = 0;
	target_volume_step = 0;

	current_pitch = 1.0f;
	pitch_initialized = false;
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

	if (samples > getSamplesLeft())
		samples = getSamplesLeft();

	offset = samples * sample_size;
	ptr += offset;

	setReadPtr(ptr);
	setSamplesLeft(samples_left - samples);
}

bool AudioSource::allocateBuffer()
{
	// Ensure the buffer is aligned with the I2S buffers
	uint32_t alloc_size = sample_size * Audio.getOutputBufferSamples() * 10;

	if (alloc_size > MAX_UPDATE_BUFFER_SIZE)
	{
		// If alloc_size is greater than MAX_UPDATE_BUFFER_SIZE, allocate a buffer around
		// MAX_UPDATE_BUFFER_SIZE
		alloc_size = (MAX_UPDATE_BUFFER_SIZE / sample_size) / Audio.getOutputBufferSamples();
		alloc_size = (alloc_size + 1) * Audio.getOutputBufferSamples() * sample_size;
	}

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
	buffer_samples = buffer_size / sample_size;
	buffer_end = buffer + buffer_size;
	return true;
}

bool AudioSource::deallocateBuffer()
{
	buffer_size = 0;
	sample_size = 0;

	if (buff_alloc)
	{
		free(buff_alloc);
		read_ptr = buffer = buff_alloc = NULL;
	}

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

UpdateResult AudioSource::update(uint32_t min_samples)
{
	if (update_callback)
		return (update_callback)(this, min_samples, update_callback_param);

	return SourceIdling;
}

void AudioSource::changeVolume()
{
	if (target_volume == current_volume && current_volume == 1)
		return;

	// Only 16-bit audio is supported right now
	if (bitsPerSample() != 16)
		return;

	int16_t* ptr = (int16_t*) getReadPtr();
	uint32_t samples = getSamplesLeft();

	if (samples > Audio.getOutputBufferSamples())
		samples = Audio.getOutputBufferSamples();
	else if (!samples)
		return;

	if (isStereo())
		samples *= 2;

	if (current_volume != target_volume && target_volume_samples)
	{
		while (true)
		{
			if (!samples)
				return;

			if (!target_volume_samples)
			{
				current_volume = target_volume;
				break;
			}

			*ptr = (int16_t)((float) *ptr * current_volume);
			ptr++;
			samples--;

			if (isStereo())
			{
				*ptr = (int16_t)((float) *ptr * current_volume);
				ptr++;
				samples--;
			}

			current_volume -= target_volume_step;
			target_volume_samples--;
		}
	}

	if (current_volume == 0)
	{
		memset(ptr, 0, samples * sizeof(int16_t));
		return;
	}

	if (current_volume != 1)
	{
		while (samples)
		{
			*ptr = (int16_t)((float) *ptr * current_volume);
			ptr++;
			samples--;
		}
	}
}

void AudioSource::changePitch()
{
	if (!pitch_initialized || current_pitch == 1)
		return;

	// Only 16-bit audio is supported right now
	if (bitsPerSample() != 16)
		return;

	uint32_t samples = getSamplesLeft();

	if (samples > Audio.getOutputBufferSamples())
		samples = Audio.getOutputBufferSamples();
	else if (!samples)
		return;

	if (isStereo())
		samples *= 2;

	PCM16ToFloat((int16_t*) getReadPtr(), float_array, samples);
	pitch_shifter.tick(float_array, samples);
	floatToPCM16(float_array, (int16_t*) getReadPtr(), samples);
}

uint8_t* AudioSource::mixingStarts()
{
	// About to mix this track. Change volume and pitch if required
	changeVolume();
	changePitch();

	return getReadPtr();
}

void AudioSource::mixingEnded(uint32_t samples)
{
	skip(samples);
}

bool AudioSource::setPitch(float value)
{
	if (!pitch_initialized)
	{
		pitch_shifter.begin();
		pitch_shifter.setEffectMix(1);
		float_array = (float*) malloc(sizeof(float) * Audio.getOutputBufferSamples() * 2);
		if (!float_array)
			return false;

		pitch_initialized = true;
	}

	pitch_shifter.setShift(value);
	current_pitch = value;
	return true;
}

void AudioSource::setVolume(float value)
{
	if (value == current_volume)
	{
		target_volume = value;
		target_volume_samples = 0;
		target_volume_step = 0;
	} else {
		__disable_irq();
		if (playing())
		{
			target_volume = value;
			target_volume_samples = VOLUME_CHANGE_SAMPLES;
			target_volume_step = (current_volume - value) / VOLUME_CHANGE_SAMPLES;
		} else {
			target_volume = current_volume = value;
		}
		__enable_irq();
	}
}
