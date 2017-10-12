/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### RawPlayer.cpp

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

#include "PropAudio.h"
#include "RawPlayer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

RawPlayer::RawPlayer()
{
	play_mode = PlayModeNormal;
	header_size = 0;
	set_volume = 1;
}
	
RawPlayer::~RawPlayer()
{
}

bool RawPlayer::begin(uint32_t fs, uint8_t bps, bool mono, uint32_t hdrsize)
{
	header_size = hdrsize;
	return AudioSource::begin(fs, bps, mono);
}

bool RawPlayer::update()
{
	uint8_t* readptr;
	uint32_t samples_read = 0;
	uint32_t samples_to_read = 0;

	// Update called with samples still in the buffer
	if (samples_left)
	{
		// Samples in the buffer are enough for another round
		if (samples_left >= MIN_OUTPUT_SAMPLES)
			return true;

		// Check if we have reached EOF
		if (audio_file.eofReached())
			// We may have reached the EOF, but we still have samples
			// to play. Just return true.
			return true;

		// We are dealing with a Loop mode playback and the samples in
		// the buffer are not enough to guarantee a constant output,
		// so fill the buffer partially.
		readptr = getReadPtr() + (samples_left * sample_size);
		samples_to_read = ((buffer + buffer_size) - readptr) / sample_size;

	} else {
		// Check if we have reached EOF
		if (audio_file.eofReached())
			// EOF and no samples left.
			// Return false so Audio can remove this AudioSource from the list.
			return false;

		// Otherwise read the entire buffer
		readptr = buffer;
		samples_to_read = buffer_size / sample_size;
		setReadPtr(readptr);
	}
	
	samples_read = audio_file.fillBuffer(readptr, samples_to_read);

	onNewData(readptr, samples_read);

	if (!samples_read)
		return false;

	samples_left += samples_read;
	return true;
}

bool RawPlayer::doPlay(PlayMode mode)
{
	play_mode = mode;

	setReadPtr(buffer);

	samples_left = audio_file.fillBuffer(buffer, buffer_size / sample_size);

	if (!samples_left)
	{
		audio_file.close();
		return false;
	}

	changeVolume();

	if (!Audio.addSource(this))
	{
		stop();
		return false;
	}

	status = AudioSourcePlaying;

	if (play_mode == PlayModeBlocking)
		while (playing());

	return true;
}

bool RawPlayer::play(const char* filename, PlayMode mode)
{
	if (status != AudioSourceStopped)
		stop();

	if (!audio_file.openRaw(filename, sample_size, header_size, mode == PlayModeLoop))
		return false;

	return doPlay(mode);
}

bool RawPlayer::playRandom(const char* filename, const char* ext, uint32_t min, uint32_t max, PlayMode mode)
{
	if (status != AudioSourceStopped)
		stop();

	if (!audio_file.openRandomRaw(filename, ext, min, max, sample_size, header_size, mode == PlayModeLoop))
		return false;

	return doPlay(mode);
}

bool RawPlayer::replay()
{
	bool was_playing = false;

	if (status == AudioSourceStopped)
		return false;

	if (status == AudioSourcePlaying)
	{
		pause();
		was_playing = true;
	}

	if (!audio_file.rewind())
		return false;

	samples_left = audio_file.fillBuffer(buffer, buffer_size / sample_size);
	if (!samples_left)
		return false;

	setReadPtr(buffer);

	changeVolume();

	if (was_playing)
		status = AudioSourcePlaying;

	return true;
}

bool RawPlayer::stop()
{
	status = AudioSourceStopped;
	Audio.removeSource(this);

	if (audio_file.isOpened())
		audio_file.close();

	return true;
}

void RawPlayer::setVolume(float value)
{
	if (value >= 0)
		set_volume = value;
}

void RawPlayer::changeVolume(bool gradually)
{
	float prev_volume = volume;
	float value;
	float step;
	uint32_t samples;

	if (set_volume != volume)
		volume = set_volume;

	if (volume != prev_volume || volume != 1)
	{
		int16_t* ptr = (int16_t*) getReadPtr();
		uint32_t count = getSamplesLeft();

		if (count > MIN_OUTPUT_SAMPLES)
			count = MIN_OUTPUT_SAMPLES;

		if (count)
		{
			float diff = volume - prev_volume;

			if (gradually && diff)
			{
				// Change volume gradually
				step = diff / (float) count;
				value = volume;

				while (count)
				{
					*ptr = (int16_t)((float) *ptr * value);
					ptr++;

					if (isStereo())
					{
						*ptr = (int16_t)((float) *ptr * value);
						ptr++;
					}

					value -= step;
					count--;
				}
			} else {
				if (isStereo())
					count *= 2;

				if (volume == 0)
				{
					memset(ptr, 0, count * sizeof(int16_t));
				} else if (set_volume != 1)
				{
					while (count)
					{
						*ptr = (int16_t)((float) *ptr * volume);
						ptr++;
						count--;
					}
				}
			}
		}
	}
}

uint8_t* RawPlayer::mixingStarts()
{
	// About to mix this track. Change volume if required
	changeVolume();
	return getReadPtr();
}
