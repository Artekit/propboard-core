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
	update_requested = false;
}

RawPlayer::~RawPlayer()
{
}

bool RawPlayer::begin(uint32_t fs, uint8_t bps, bool mono, uint32_t hdrsize)
{
	// Remember the header size
	header_size = hdrsize;

	// sample_size is calculated in here
	AudioSource::begin(fs, bps, mono);

	// TBD: get a proper buffer size
	uint32_t samples = Audio.getOutputSamples() * 10 * (stereo ? 1 : 2);

	return buffer.allocate(samples, sample_size);
}

void RawPlayer::end()
{
	buffer.deallocate();
}

uint32_t RawPlayer::getSamplesLeft()
{
	uint32_t samples = 0;

	__disable_irq();
	samples = buffer.getPlayingBuffer()->samples;
	__enable_irq();

	return samples;
}

UpdateResult RawPlayer::update(AudioFileHelper* file, playerBuffer* buffer)
{
	uint32_t samples_read = 0;
	samplesBuffer* updating_buffer = buffer->getUpdatingBuffer();

	if (updating_buffer->updated)
		return SourceUpdated;

	// Read from file
	samples_read = file->fillBuffer(updating_buffer->buffer, buffer->getBufferSamples());

	if ((!samples_read || samples_read != buffer->getBufferSamples()) && !file->eofReached())
		// Something is wrong
		return UpdateError;

	updating_buffer->samples = samples_read;
	updating_buffer->readptr = updating_buffer->buffer;
	updating_buffer->updated = true;

	return SourceUpdated;
}

UpdateResult RawPlayer::update()
{
	// Check if we have reached EOF
	if (audio_file.eofReached())
		// Return SourceRemove so Audio can remove this AudioSource from the list.
		return SourceRemove;

	if (!update_requested)
		return SourceUpdated;

	update_requested = false;

	UpdateResult result = update(&audio_file, &buffer);

	if (result == SourceUpdated)
	{
		// Check the condition on which there is a very, very loaded system, and the playing_buffer
		// ran out of samples while (from the mixingEnded function point of view) the
		// updating_buffer is still to be updated.
		samplesBuffer* playing_buffer = buffer.getPlayingBuffer();
		if (!playing_buffer->samples && !playing_buffer->updated)
		{
			// Do the buffer switching here
			buffer.switchBuffers();

			// Request another update
			update_requested = true;
			Audio.triggerUpdate();
		}
	}

	return result;
}

bool RawPlayer::refillBuffer(AudioFileHelper* file, samplesBuffer* buffer, uint32_t samples)
{
	uint32_t read_samples = file->fillBuffer(buffer->buffer, samples);

	if (!read_samples)
		return false;

	buffer->samples = read_samples;
	buffer->updated = true;
	return true;
}

bool RawPlayer::refill(AudioFileHelper* file, playerBuffer* buffer)
{
	// Reset buffers
	buffer->reset();

	samplesBuffer* playing_buffer = buffer->getPlayingBuffer();
	samplesBuffer* updating_buffer = buffer->getUpdatingBuffer();

	// Fill both buffers
	if (!refillBuffer(file, playing_buffer, buffer->getBufferSamples()))
		return false;

	if (playing_buffer->samples == buffer->getBufferSamples())
		refillBuffer(file, updating_buffer, buffer->getBufferSamples());

	return true;
}

bool RawPlayer::refill()
{
	if (refill(&audio_file, &buffer))
		return true;

	audio_file.close();
	return false;
}

bool RawPlayer::doPlay(PlayMode mode)
{
	play_mode = mode;

	if (!refill())
		return false;

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

bool RawPlayer::playRandom(const char* filename, const char* ext,
						   uint32_t min, uint32_t max, PlayMode mode)
{
	if (status != AudioSourceStopped)
		stop();

	if (!audio_file.openRandomRaw(filename, ext, min, max, sample_size, header_size,
								  mode == PlayModeLoop))
		return false;

	return doPlay(mode);
}

bool RawPlayer::replay()
{
	if (status == AudioSourceStopped)
		return false;

	bool was_playing = false;
	if (status == AudioSourcePlaying)
	{
		pause();
		was_playing = true;
	}

	if (!audio_file.rewind())
		return false;

	if (!refill())
		return false;

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

char* RawPlayer::getFileName()
{
	return audio_file.getFileName();
}

uint32_t RawPlayer::mixingStarts(uint32_t samples)
{
	if (buffer.getPlayingBuffer()->updated)
	{
		changeVolume(buffer.getPlayingBuffer()->readptr, samples);
		samples = buffer.getPlayingBuffer()->samples;
	}
	else
		samples = 0;

	return samples;
}

void RawPlayer::mixingEnded(uint32_t samples)
{
	samplesBuffer* playing_buffer = buffer.getPlayingBuffer();

	playing_buffer->samples -= samples;
	if (!playing_buffer->samples)
	{
		playing_buffer->updated = false;

		// Don't switch buffers if updating_buffer is not updated,
		// since it may be still updating right now.
		if (buffer.getUpdatingBuffer()->updated)
			buffer.switchBuffers();

		update_requested = true;
		Audio.triggerUpdate();
	}
}
