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
	chained_status = PlayingNone;
	status = AudioSourceStopped;
	chained_mode = PlayModeNormal;
}

WavChainPlayer::~WavChainPlayer()
{
}

bool WavChainPlayer::begin(const char* filename)
{
	if (audio_file.isOpened())
		return false;

	if (!audio_file.openWav(filename, true))
		return false;

	if (!RawPlayer::begin(audio_file.getWavHeader()->format.sample_rate,
						  audio_file.getWavHeader()->format.bits_per_sample,
						  audio_file.getWavHeader()->format.channels == 1,
						  audio_file.getHeaderSize()))
	{
		audio_file.close();
		return false;
	}

	// Do fill buffers
	if (refill())
	{
		setChainedStatus(PlayingMain);
		return true;
	}

	audio_file.close();
	return false;
}

bool WavChainPlayer::doChain(PlayMode mode)
{
	// Chained track must match main track
	if (memcmp(&audio_file.getWavHeader()->format,
			   &chained_file.getWavHeader()->format,
			   sizeof(WAV_FORMAT)) != 0)
		return false;

	return RawChainPlayer::doChain(mode);
}

bool WavChainPlayer::chain(const char* filename, PlayMode mode)
{
	if (!audio_file.isOpened())
		return false;

	if (chained_status == PlayingChained)
		setChainedStatus(PlayingMain);

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
	if (!audio_file.isOpened())
		return false;

	if (chained_status == PlayingChained)
	{
		setChainedStatus(PlayingMain);
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
