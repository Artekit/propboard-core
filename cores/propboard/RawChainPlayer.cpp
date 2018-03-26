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

RawChainPlayer::RawChainPlayer()
{
	chain_status = PlayingNone;
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
	if (main_file.isOpened())
		return false;

	// Call the base class begin function to allocate the working buffer and calculate sample_size
	if (!AudioSource::begin(fs, bps, mono))
		return false;

	// Open the file as raw
	if (!main_file.openRaw(filename, sample_size, hdrsize, true))
		return false;

	// Remember the header size
	header_size = hdrsize;

	// Do the first buffer fill
	samples_left = main_file.fillBuffer(buffer, buffer_size / sample_size);

	if (samples_left < Audio.getOutputBufferSamples())
	{
		main_file.close();
		return false;
	}

	chain_status = PlayingMain;
	return true;
}

bool RawChainPlayer::doChain(PlayMode mode)
{
	// Allocate buffer for chained track. The format must be the same of the main track.
	if (!chained.begin(sample_rate, bits_per_sample, !stereo))
		return false;

	chained_mode = mode;
	chained.setSamplesLeft(chained_file.fillBuffer(chained.getBufferPtr(), chained.getBufferSize() / sample_size));

	if (!chained.getSamplesLeft())
		return false;

	chain_status = PlayingChained;

	if (playing())
	{
		setReadPtr(buffer);
		main_file.rewind();
		samples_left = main_file.fillBuffer(buffer, buffer_size / sample_size);

		if (mode == PlayModeBlocking)
			while (chain_status == PlayingChained);
	}

	return true;
}

bool RawChainPlayer::chain(const char* filename, PlayMode mode)
{
	// This function can be called only after calling begin. main_file determines
	// the chained track format
	if (!main_file.isOpened())
		return false;

	// If already playing a chained track, pass to PlayingMain
	if (chain_status == PlayingChained)
	{
		chain_status = PlayingMain;
		chained_file.close();
	}

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
	if (!main_file.isOpened())
		return false;

	// If already playing a chained track, pass to PlayingMain
	if (chain_status == PlayingChained)
	{
		chain_status = PlayingMain;
		chained_file.close();
	}

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
	if (!main_file.isOpened())
		return false;

	if (status != AudioSourceStopped)
		return false;

	if (!Audio.addSource(this))
		return false;

	status = AudioSourcePlaying;

	if (chain_status == PlayingChained && chained_mode == PlayModeBlocking)
		while (chain_status == PlayingChained);

	return true;
}

bool RawChainPlayer::stop()
{
	if (!main_file.isOpened())
		return false;

	status = AudioSourceStopped;

	Audio.removeSource(this);
	main_file.close();

	if (chained_file.isOpened())
		chained_file.close();

	return true;
}

// We use the following function to update both main and chained tracks
UpdateResult RawChainPlayer::update(AudioSource* src, AudioFileHelper* wav, uint32_t min_samples)
{
	uint8_t* new_data_ptr;
	uint32_t samples_read = 0;
	uint32_t samples_to_read = 0;

	// Update called with samples still in the buffer
	if (src->getSamplesLeft())
	{
		// Samples in the buffer are enough for another round
		if (src->getSamplesLeft() >= min_samples)
			return SourceUpdated;

		// Check if we have reached EOF
		if (wav->eofReached())
			// We may have reached the EOF, but we still have samples
			// to play. Just return SourceUpdated.
			return SourceUpdated;

		// We are dealing with a Loop mode playback and the samples in the buffer are less than
		// min_samples. Fill the buffer entirely. Calculate how many samples we need to read to
		// achieve that and where to store them in the buffer.
		new_data_ptr = src->getReadPtr() + (src->getSamplesLeft() * sample_size);
		samples_to_read = (src->getBufferPtr() + src->getBufferSize() - new_data_ptr) / sample_size;

		// We may have already copied a chunk of samples from the main track into the chained track
		// (because it was ending) and the readings from the main track buffer may not be aligned
		// anymore, because the read pointer was moved arbitrarily when copying the data into the
		// chained track. Check if the pointer to store the new samples in the buffer results to be
		// at the end of it. If so, we need to move the data to the beginning of the buffer and then
		// fill it entirely with new samples from the file.
		// TBD: think about refactoring this whole function and fit it somehow into the RawPlayer
		// base class.
		if (!samples_to_read)
		{
			// This means that new_data_ptr is at the end of the buffer, so copy the remaining data
			// to the beginning.
			memcpy(src->getBufferPtr(), src->getReadPtr(), src->getSamplesLeft() * sample_size);

			// Adjust the read (from file) pointer
			new_data_ptr = src->getBufferPtr() + src->getSamplesLeft() * sample_size;

			// Update samples to read
			samples_to_read = (src->getBufferPtr() + src->getBufferSize() - new_data_ptr) / sample_size;

			// Set the reading pointer (of samples) to the beginning of the buffer
			src->setReadPtr(src->getBufferPtr());
		}
	} else {
		// Check if we have reached EOF
		if (wav->eofReached())
			// EOF and no samples left.
			return SourceRemove;

		// Otherwise read the entire buffer
		src->setReadPtr(src->getBufferPtr());
		new_data_ptr = src->getBufferPtr();
		samples_to_read = src->getBufferSize() / sample_size;
	}

	samples_read = wav->fillBuffer(new_data_ptr, samples_to_read);

	if (!samples_read)
		return UpdateError;

	src->setSamplesLeft(src->getSamplesLeft() + samples_read);
	return SourceUpdated;
}

UpdateResult RawChainPlayer::update(uint32_t min_samples)
{
	if (chain_status == PlayingChained)
	{
		// Update the chained track
		if (update(&chained, &chained_file, min_samples) != SourceUpdated ||
			!chained.getSamplesLeft())
		{
			// Cannot update, jump to main track.
			chained_file.close();
			chain_status = PlayingMain;
		} else if (chained.getSamplesLeft() < min_samples)
		{
			// The chained track will stop after playing these samples. If we were using a WavPlayer
			// object we may hear a 'click' while it loads the next WAV file. Here we will ensure
			// a smooth and click-less transition by copying part of the main track at the end of
			// the chained track.
			uint8_t* offset = chained.getReadPtr() + (chained.getSamplesLeft() * sample_size);

			// Be sure the main track has the samples to complete the chained track
			if (samples_left < min_samples - chained.getSamplesLeft())
			{
				setReadPtr(buffer);
				main_file.rewind();
				samples_left = main_file.fillBuffer(buffer, buffer_size);
			}

			// Copy
			memcpy(offset, buffer, (min_samples - chained.getSamplesLeft()) * sample_size);

			// Adjust main track read pointer and samples left counter
			skipMainTrack(min_samples - chained.getSamplesLeft());
			chained.setSamplesLeft(min_samples);

			// We are not using the chained file anymore
			chained_file.close();

			// Jump to transition status
			chain_status = PlayingTransition;
		} else return SourceUpdated;
	}

	if (chain_status == PlayingTransition)
	{
		//
		if (chained.getSamplesLeft())
			return SourceUpdated;

		chain_status = PlayingMain;
	}

	if (chain_status == PlayingMain)
	{
		if (update(this, &main_file, min_samples) != SourceUpdated || !samples_left)
			return UpdateError;

		return SourceUpdated;
	}

	return SourceIdling;
}

bool RawChainPlayer::restart()
{
	bool was_playing = false;

	if (!main_file.isOpened())
		return false;

	if (status == AudioSourceStopped)
		return false;

	was_playing = playing();

	if (chain_status == PlayingMain)
	{
		status = AudioSourcePaused;
		setReadPtr(buffer);
		main_file.rewind();
		samples_left = main_file.fillBuffer(buffer, buffer_size / sample_size);

		if (!samples_left)
			return false;
	}

	chain_status = PlayingMain;

	if (was_playing)
		status = AudioSourcePlaying;

	return true;
}

uint8_t* RawChainPlayer::getReadPtr()
{
	switch (chain_status)
	{
		case PlayingMain:
			return read_ptr;

		case PlayingChained:
		case PlayingTransition:
			return chained.getReadPtr();

		case PlayingNone:
			break;
	}

	return NULL;
}

void RawChainPlayer::skipMainTrack(uint32_t samples)
{
	uint8_t* ptr = read_ptr;
	uint32_t offset;

	if (samples > samples_left)
		samples = samples_left;

	offset = samples * sample_size;
	ptr += offset;
	read_ptr = ptr;
	samples_left -= samples;
}

void RawChainPlayer::skip(uint32_t samples)
{
	switch (chain_status)
	{
		case PlayingMain:
			skipMainTrack(samples);
			break;

		case PlayingChained:
		case PlayingTransition:
			chained.skip(samples);
			break;

		case PlayingNone:
			break;
	}
}

uint32_t RawChainPlayer::getSamplesLeft()
{
	switch (chain_status)
	{
		case PlayingMain:
			return samples_left;

		case PlayingChained:
		case PlayingTransition:
			return chained.getSamplesLeft();

		case PlayingNone:
			break;
	}

	return 0;
}

uint32_t RawChainPlayer::getChainedDuration()
{
	if (chained_file.isOpened())
		return chained_file.getDuration();

	return 0;
}
