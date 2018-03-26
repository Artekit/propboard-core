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

enum ChainPlayStatus
{
	PlayingNone = 0,
	PlayingMain,
	PlayingChained,
	PlayingTransition,
};

class RawChainPlayer : public AudioSource
{
public:
	RawChainPlayer();
	~RawChainPlayer();

	bool begin(const char* filename, uint32_t fs, uint8_t bps, bool mono, uint32_t hdrsize);
	bool chain(const char* filename, PlayMode mode = PlayModeNormal);
	bool chainRandom(const char* filename, const char* ext, uint32_t min, uint32_t max, PlayMode mode = PlayModeNormal);
	bool play();
	bool stop();
	UpdateResult update(uint32_t min_samples);
	bool restart();
	void skip(uint32_t samples);

	uint32_t getChainedDuration();
	inline ChainPlayStatus getChainStatus() { return chain_status; }
	inline bool playingChained() { return (chain_status == PlayingChained || chain_status == PlayingTransition); }
	uint8_t* getReadPtr();

protected:
	bool doChain(PlayMode mode);
	UpdateResult update(AudioSource* src, AudioFileHelper* wav, uint32_t min_samples);
	uint32_t getSamplesLeft();
	void skipMainTrack(uint32_t samples);

	AudioFileHelper main_file;
	AudioFileHelper chained_file;
	uint32_t header_size;
	AudioSource chained;
	PlayMode chained_mode;
	volatile ChainPlayStatus chain_status;
};

#endif /* __RAWCHAINPLAYER_H__ */
