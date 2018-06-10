/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### RawChainPlayer.h

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

#ifndef __RAWCHAINPLAYER_H__
#define __RAWCHAINPLAYER_H__

#include "WavPlayer.h"
#include "RawPlayer.h"

enum ChainPlayStatus
{
	PlayingNone = 0,
	PlayingMain,
	PlayingChained,
	PlayingTransition,
};

class RawChained : public RawPlayer
{
	uint32_t fillThisBuffer(uint8_t* buffer, uint32_t samples)
	{
		return audio_file.fillBuffer(buffer, samples);
	}
};

class RawChainPlayer : public RawPlayer
{
public:
	RawChainPlayer();
	~RawChainPlayer();

	bool begin(const char* filename, uint32_t fs, uint8_t bps, bool mono, uint32_t hdrsize);
	bool chain(const char* filename, PlayMode mode = PlayModeNormal);
	bool chainRandom(const char* filename, const char* ext, uint32_t min, uint32_t max, PlayMode mode = PlayModeNormal);
	bool play();
	bool stop();
	UpdateResult update();
	bool restart();

	uint32_t getChainedDuration();
	inline ChainPlayStatus getChainStatus() { return chained_status; }
	inline bool playingChained() { return (chained_status == PlayingChained ||
										   chained_status == PlayingTransition); }
	char* getChainedFileName();
	void* getNextSamplePtr();
	uint32_t getSamplesLeft();

protected:
	bool doChain(PlayMode mode);
	uint32_t mixingStarts(uint32_t samples);
	void mixingEnded(uint32_t samples);
	void setChainedStatus(ChainPlayStatus status);

	playerBuffer* active_buffer;

	playerBuffer chained_buffer;
	AudioFileHelper chained_file;
	PlayMode chained_mode;
	volatile ChainPlayStatus chained_status;
	bool chained_update_requested;
};

#endif /* __RAWCHAINPLAYER_H__ */
