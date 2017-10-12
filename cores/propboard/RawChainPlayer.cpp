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

	if (samples_left < MIN_OUTPUT_SAMPLES)
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
bool RawChainPlayer::update(AudioSource* src, AudioFileHelper* wav)
{
	uint8_t* readptr;
	uint32_t samples_read = 0;
	uint32_t samples_to_read = 0;

	// Update called with samples still in the buffer
	if (src->getSamplesLeft())
	{
		// Samples in the buffer are enough for another round
		if (src->getSamplesLeft() >= MIN_OUTPUT_SAMPLES)
			return true;

		// Check if we have reached EOF
		if (wav->eofReached())
			// We may have reached the EOF, but we still have samples
			// to play. Just return true.
			return true;

		// We are dealing with a Loop mode playback and the samples in
		// the buffer are not enough to guarantee a constant output,
		// so fill the buffer partially.
		readptr = src->getReadPtr() + (src->getSamplesLeft() * sample_size);
		samples_to_read = (src->getBufferPtr() + src->getBufferSize() - readptr) / sample_size;

		if (!samples_to_read)
		{
			// Copy the remaining data to the beginning of the buffer
			memcpy(src->getBufferPtr(), src->getReadPtr(), src->getSamplesLeft() * sample_size);

			// Adjust the read (from file) pointer
			readptr = src->getBufferPtr() + src->getSamplesLeft() * sample_size;

			// Update samples to read
			samples_to_read = (src->getBufferPtr() + src->getBufferSize() - readptr) / sample_size;

			// Set the reading pointer (of samples) to the beginning of the buffer
			src->setReadPtr(src->getBufferPtr());
		}
	} else {
		// Check if we have reached EOF
		if (wav->eofReached())
			// EOF and no samples left.
			return false;

		// Otherwise read the entire buffer
		src->setReadPtr(src->getBufferPtr());
		readptr = src->getBufferPtr();
		samples_to_read = src->getBufferSize() / sample_size;
	}

	samples_read = wav->fillBuffer(readptr, samples_to_read);

	if (!samples_read)
		return false;

	src->setSamplesLeft(src->getSamplesLeft() + samples_read);
	return true;
}

bool RawChainPlayer::update()
{
	uint32_t offset;

	if (chain_status == PlayingChained)
	{
		if (!update(&chained, &chained_file) || !chained.getSamplesLeft())
		{
			chained_file.close();
			chain_status = PlayingMain;
		} else if (chained.getSamplesLeft() < MIN_OUTPUT_SAMPLES)
		{
			// The chained track will stop after this update. If we were using a WavPlayer object
			// we may hear a 'click' while it loads the next WAV file. Here we will ensure
			// a smooth and click-less transition by copying part of the main track at
			// the end of the chained track.
			uint8_t* offset = chained.getReadPtr() + (chained.getSamplesLeft() * sample_size);

			// Be sure the main track has the samples to complete the chained track
			if (samples_left < MIN_OUTPUT_SAMPLES - chained.getSamplesLeft())
			{
				setReadPtr(buffer);
				main_file.rewind();
				samples_left = main_file.fillBuffer(buffer, buffer_size);
			}

			// Copy
			memcpy(offset, buffer, (MIN_OUTPUT_SAMPLES - chained.getSamplesLeft()) * sample_size);

			// Adjust main read pointer
			AudioSource::skip(MIN_OUTPUT_SAMPLES - chained.getSamplesLeft());

			chained.setSamplesLeft(MIN_OUTPUT_SAMPLES);
			chained_file.close();
			chain_status = PlayingTransition;
		}

		return true;
	}

	if (chain_status == PlayingTransition)
	{
		if (chained.getSamplesLeft())
			return true;

		chain_status = PlayingMain;
	}

	if (chain_status == PlayingMain)
	{
		if (!update(this, &main_file) || !samples_left)
			return false;
	}

	return true;
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

uint8_t* RawChainPlayer::mixingStarts()
{
	// PropAudio is ready to mix us so select
	// the pointer according to the track we're playing
	switch (chain_status)
	{
		case PlayingMain:
			return getReadPtr();

		case PlayingChained:
		case PlayingTransition:
			return chained.getReadPtr();
	}

	return NULL;
}

void RawChainPlayer::mixingEnded(uint32_t samples)
{
	// Check which is the current playing track
	// and skip samples accordingly
	switch (chain_status)
	{
		case PlayingMain:
			AudioSource::skip(samples);
			break;

		case PlayingChained:
		case PlayingTransition:
			chained.skip(samples);
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
	}

	return 0;
}

uint32_t RawChainPlayer::getChainedDuration()
{
	if (chained_file.isOpened())
		return chained_file.getDuration();

	return 0;
}
