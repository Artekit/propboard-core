/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### WavChainPlayer.cpp

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
#include "WavChainPlayer.h"
#include <stddef.h>
#include <string.h>

WavChainPlayer::WavChainPlayer()
{
	chain_status = PlayingNone;
	status = AudioSourceStopped;
	chained_mode = PlayModeNormal;
}

WavChainPlayer::~WavChainPlayer()
{
}

bool WavChainPlayer::begin(const char* filename)
{
	if (main_file.isOpened())
		return false;

	if (!main_file.openWav(filename, true))
		return false;

	if (!AudioSource::begin(main_file.getWavHeader()->format.sample_rate,
							main_file.getWavHeader()->format.bits_per_sample,
							main_file.getWavHeader()->format.channels == 1))
	{
		main_file.close();
		return false;
	}

	samples_left = main_file.fillBuffer(buffer, buffer_size / sample_size);

	if (samples_left < MIN_OUTPUT_SAMPLES)
	{
		main_file.close();
		return false;
	}

	chain_status = PlayingMain;
	return true;
}

bool WavChainPlayer::doChain(PlayMode mode)
{
	// Chained track must match main track
	if (memcmp(&main_file.getWavHeader()->format,
			   &chained_file.getWavHeader()->format,
			   sizeof(WAV_FORMAT) != 0))
		return false;

	chained_mode = mode;

	if (!chained.begin(chained_file.getWavHeader()->format.sample_rate,
					   chained_file.getWavHeader()->format.bits_per_sample,
					   chained_file.getWavHeader()->format.channels == 1))
		return false;

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

bool WavChainPlayer::chain(const char* filename, PlayMode mode)
{
	if (!main_file.isOpened())
		return false;

	if (chain_status == PlayingChained)
	{
		chain_status = PlayingMain;
		chained_file.close();
	}

	if (!chained_file.openWav(filename, mode == PlayModeLoop))
		return false;

	if (!doChain(mode))
	{
		chained_file.close();
		return false;
	}

	return true;
}

bool WavChainPlayer::chainRandom(const char* filename, uint32_t min, uint32_t max, PlayMode mode)
{
	if (!main_file.isOpened())
		return false;

	if (chain_status == PlayingChained)
	{
		chain_status = PlayingMain;
		chained_file.close();
	}

	if (!chained_file.openRandomWav(filename, min, max, mode == PlayModeLoop))
		return false;

	if (!doChain(mode))
	{
		chained_file.close();
		return false;
	}

	return true;
}
