/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### RawChainPlayer.cpp

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
#include "RawChainPlayer.h"
#include <stddef.h>
#include <string.h>
#include <Arduino.h>

RawChainPlayer::RawChainPlayer()
{
	chained_status = PlayingNone;
	status = AudioSourceStopped;
	chained_mode = PlayModeNormal;
	header_size = 0;
}

RawChainPlayer::~RawChainPlayer()
{
}

bool RawChainPlayer::begin(const char* filename, uint32_t fs, uint8_t bps, bool mono, uint32_t hdrsize)
{
	// If the main_file is already opened, it means this function was already called
	if (audio_file.isOpened())
		return false;

	// Call the base class begin function to allocate the working buffer
	if (!RawPlayer::begin(fs, bps, mono, hdrsize))
		return false;

	// Open the file as raw
	if (!audio_file.openRaw(filename, sample_size, hdrsize, true))
		return false;

	// Remember the header size
	header_size = hdrsize;

	// Do fill buffers
	if (refill())
	{
		setChainedStatus(PlayingMain);
		return true;
	}

	audio_file.close();
	return false;
}

bool RawChainPlayer::doChain(PlayMode mode)
{
	// Allocate buffer for chained track. The format must be the same of the main track.
	if (!chained_buffer.allocate(buffer.getBufferSamples() * 2, sample_size))
		return false;

	chained_mode = mode;

	// Fill one buffer and start playing
	chained_buffer.reset();
	if (!refillBuffer(&chained_file, chained_buffer.getPlayingBuffer(),
					 chained_buffer.getBufferSamples()))
		return false;

	setChainedStatus(PlayingChained);

	// Fill the other buffer
	if (!refillBuffer(&chained_file, chained_buffer.getUpdatingBuffer(),
					 chained_buffer.getBufferSamples()))
	{
		setChainedStatus(PlayingMain);
		return false;
	}

	if (playing())
	{
		// Refill the main track buffer in case we need to suddenly stop playing the chained track
		audio_file.rewind();
		if (!refill())
			return false;

		if (mode == PlayModeBlocking)
			while (chained_status == PlayingChained);
	}

	return true;
}

bool RawChainPlayer::chain(const char* filename, PlayMode mode)
{
	// This function can be called only after calling begin. main_file determines
	// the chained track format
	if (!audio_file.isOpened())
		return false;

	// If already playing a chained track, go back to PlayingMain
	if (chained_status == PlayingChained)
		setChainedStatus(PlayingMain);

	// Open the chained file as raw. It has to have the same format as main_file
	if (!chained_file.openRaw(filename, sample_size, header_size, mode == PlayModeLoop))
		return false;

	if (!doChain(mode))
	{
		chained_file.close();
		return false;
	}

	return true;
}

bool RawChainPlayer::chainRandom(const char* filename, const char* ext, uint32_t min, uint32_t max, PlayMode mode)
{
	// This function can be called only after calling begin. main_file determines
	// the chained track format
	if (!audio_file.isOpened())
		return false;

	// If already playing a chained track, go back to PlayingMain
	if (chained_status == PlayingChained)
		setChainedStatus(PlayingMain);

	// Open a random chained file as raw. It has to have the same format as main_file
	if (!chained_file.openRandomRaw(filename, ext, min, max, sample_size, header_size, mode == PlayModeLoop))
		return false;

	if (!doChain(mode))
	{
		chained_file.close();
		return false;
	}

	return true;
}

bool RawChainPlayer::play()
{
	// This function can be called only after calling begin()
	if (!audio_file.isOpened())
		return false;

	if (status != AudioSourceStopped)
		return false;

	update_requested = false;

	if (!Audio.addSource(this))
		return false;

	status = AudioSourcePlaying;

	if (chained_status == PlayingChained && chained_mode == PlayModeBlocking)
		while (chained_status == PlayingChained);

	return true;
}

bool RawChainPlayer::stop()
{
	if (!audio_file.isOpened())
		return false;

	status = AudioSourceStopped;

	Audio.removeSource(this);
	audio_file.close();

	if (chained_file.isOpened())
		chained_file.close();

	return true;
}

void RawChainPlayer::setChainedStatus(ChainPlayStatus status)
{
	ChainPlayStatus new_status;
	playerBuffer* new_active_buffer = NULL;

	switch (status)
	{
		default:
		case PlayingNone:
			return;

		case PlayingMain:
			new_status = status;
			new_active_buffer = &buffer;
			break;

		case PlayingChained:
			new_status = status;
			new_active_buffer = &chained_buffer;
			break;

		case PlayingTransition:
			new_status = status;
			break;
	}

	__disable_irq();
	chained_status = new_status;
	if (new_active_buffer)
		active_buffer = new_active_buffer;
	__enable_irq();
}

UpdateResult RawChainPlayer::update()
{
	uint32_t samples_read;
	UpdateResult result = UpdateError;

	switch (chained_status)
	{
		case PlayingMain:
			result = RawPlayer::update();
			break;

		case PlayingTransition:
			// Do nothing
			result = SourceUpdated;
			break;

		case PlayingChained:
			if (!chained_update_requested)
				return SourceUpdated;

			chained_update_requested = false;

			if (chained_file.eofReached())
			{
				setChainedStatus(PlayingTransition);
				result = SourceUpdated;
				break;
			}

			result = RawPlayer::update(&chained_file, &chained_buffer);

			if (result == SourceUpdated)
			{
				samplesBuffer* updating_buffer = chained_buffer.getUpdatingBuffer();

				// Check if the buffer was filled completely
				if (updating_buffer->samples == chained_buffer.getBufferSamples())
					break;

				// Otherwise fill the remaining with audio from the main track
				uint8_t* ptr = updating_buffer->buffer + updating_buffer->samples * sample_size;
				uint32_t samples = chained_buffer.getBufferSamples() - updating_buffer->samples;

				audio_file.rewind();
				samples_read = audio_file.fillBuffer(ptr, samples);

				if (samples_read == samples)
				{
					updating_buffer->samples += samples;
					result = SourceUpdated;

					// Refill main track buffer
					if (!refill())
					{
						result = UpdateError;
						break;
					}

					// Check the condition on which there is a very, very loaded system, and the
					// playing_buffer ran out of samples while (from the mixingEnded function point
					// of view) the updating_buffer is still to be updated.
					samplesBuffer* playing_buffer = chained_buffer.getPlayingBuffer();
					if (!playing_buffer->samples && !playing_buffer->updated)
					{
						// Do the buffer switching here
						chained_buffer.switchBuffers();

						// Request another update
						chained_update_requested = true;
						Audio.triggerUpdate();
					}
				} else {
					result = UpdateError;
				}
			}
			break;

		case PlayingNone:
			break;
	}

	return result;
}

bool RawChainPlayer::restart()
{
	bool was_playing = false;

	if (!audio_file.isOpened())
		return false;

	if (status == AudioSourceStopped)
		return false;

	was_playing = playing();

	if (chained_status == PlayingMain)
	{
		status = AudioSourcePaused;
		audio_file.rewind();
		if (!refill())
		{
			stop();
			return false;
		}
	}

	setChainedStatus(PlayingMain);

	if (was_playing)
		status = AudioSourcePlaying;

	return true;
}

void RawChainPlayer::mixingEnded(uint32_t samples)
{
	samplesBuffer* playing_buffer = chained_buffer.getPlayingBuffer();

	switch (chained_status)
	{
		case PlayingMain:
			RawPlayer::mixingEnded(samples);
			break;

		case PlayingChained:
			playing_buffer->samples -= samples;
			if (!playing_buffer->samples)
			{
				playing_buffer->updated = false;

				// Don't switch buffers if updating_buffer is not updated,
				// since it may be still updating right now.
				if (chained_buffer.getUpdatingBuffer()->updated)
					chained_buffer.switchBuffers();

				chained_update_requested = true;
				Audio.triggerUpdate();
			}
			break;

		case PlayingTransition:
			playing_buffer->samples -= samples;
			if (!playing_buffer->samples)
			{
				// Time to change to main track
				setChainedStatus(PlayingMain);
			}
			break;

		case PlayingNone:
			break;
	}
}

uint32_t RawChainPlayer::getSamplesLeft()
{
	return active_buffer->getPlayingBuffer()->samples;
}

void* RawChainPlayer::getNextSamplePtr()
{
	void* ret = active_buffer->getPlayingBuffer()->readptr;
	active_buffer->getPlayingBuffer()->readptr += sample_size;
	return ret;
}

uint32_t RawChainPlayer::mixingStarts(uint32_t samples)
{
	changeVolume(active_buffer->getPlayingBuffer()->readptr, samples);
	return active_buffer->getPlayingBuffer()->samples;
}

uint32_t RawChainPlayer::getChainedDuration()
{
	if (chained_file.isOpened())
		return chained_file.getDuration();

	return 0;
}

char* RawChainPlayer::getChainedFileName()
{
	return chained_file.getFileName();
}
