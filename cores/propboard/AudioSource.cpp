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
	stereo = false;
	bits_per_sample = sample_rate = sample_size = 0;
	next_to_mix = next_in_list = NULL;
	current_volume = 1.0f;
	target_volume = current_volume;
	target_volume_samples = 0;
	target_volume_step = 0;
}

AudioSource::~AudioSource()
{
	end();
}

bool AudioSource::begin(uint32_t fs, uint8_t bps, bool mono)
{
	if (status != AudioSourceStopped)
		stop();

	sample_rate = fs;
	bits_per_sample = bps;
	stereo = !mono;
	sample_size = (bits_per_sample / 8) * (stereo ? 2 : 1);
	return true;
}

void AudioSource::end()
{
	stop();
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

void AudioSource::changeVolume(uint8_t* samples_ptr, uint32_t samples)
{
	// Only 16-bit audio is supported right now
	if ((target_volume == current_volume && current_volume == 1) || bitsPerSample() != 16)
		return;

	int16_t* ptr = (int16_t*) samples_ptr;

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

			// Linear? that's bad.
			// TODO: use logarithmic volume
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

UpdateResult AudioSource::update()
{
	return UpdateError;
}

uint32_t AudioSource::getSamplesLeft()
{
	return 0;
}
