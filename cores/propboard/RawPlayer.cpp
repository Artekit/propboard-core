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
}

RawPlayer::~RawPlayer()
{
}

bool RawPlayer::begin(uint32_t fs, uint8_t bps, bool mono, uint32_t hdrsize)
{
	header_size = hdrsize;
	return AudioSource::begin(fs, bps, mono);
}

UpdateResult RawPlayer::update(uint32_t min_samples)
{
	uint8_t* readptr;
	uint32_t samples_read = 0;
	uint32_t samples_to_read = 0;

	// Update called with samples still in the buffer?
	if (samples_left)
	{
		// Samples in the buffer are enough for another round?
		if (samples_left >= min_samples)
			// Yes, return SourceUpdated
			return SourceUpdated;

		// Check if we have reached EOF
		if (audio_file.eofReached())
			// We may have reached the EOF, but we still have samples
			// to play. Just return SourceUpdated.
			return SourceUpdated;

		// We are dealing with a Loop mode playback and the samples in the buffer are not enough to
		// guarantee a constant output,so fill the buffer partially. With I2S-aligned buffers we
		// shouldn't come here often (if ever).
		readptr = getReadPtr() + (samples_left * sample_size);
		samples_to_read = buffer_samples - samples_left;
	} else {
		// Check if we have reached EOF
		if (audio_file.eofReached())
			// EOF and no samples left.
			// Return SourceRemove so Audio can remove this AudioSource from the list.
			return SourceRemove;

		// Otherwise read the entire buffer
		readptr = buffer;
		samples_to_read = buffer_samples;
		setReadPtr(readptr);
	}

	samples_read = audio_file.fillBuffer(readptr, samples_to_read);

	onNewData(readptr, samples_read);

	if (!samples_read)
		return UpdateError;

	samples_left += samples_read;
	return SourceUpdated;
}

bool RawPlayer::doPlay(PlayMode mode)
{
	play_mode = mode;

	setReadPtr(buffer);

	samples_left = audio_file.fillBuffer(buffer, buffer_samples);

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
