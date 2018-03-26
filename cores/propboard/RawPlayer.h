/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### RawPlayer.h

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

#ifndef __RAWPLAYER_H__
#define __RAWPLAYER_H__

#include <stm32f4xx.h>
#include <ff.h>
#include "AudioSource.h"
#include "AudioFileHelper.h"

enum PlayMode
{
	PlayModeNormal = 0,
	PlayModeLoop,
	PlayModeBlocking
};

class RawPlayer : public AudioSource
{
public:
	RawPlayer();
	~RawPlayer();

	bool begin(uint32_t fs, uint8_t bps, bool mono, uint32_t hdrsize);
	bool play(const char* filename, PlayMode mode = PlayModeNormal);
	bool playRandom(const char* filename, const char* ext, uint32_t min, uint32_t max, PlayMode mode = PlayModeNormal);
	bool replay();
	bool stop();
	UpdateResult update(uint32_t min_samples);
	uint32_t duration() { return audio_file.getDuration(); }

protected:

	bool doPlay(PlayMode mode);

	AudioFileHelper audio_file;
	PlayMode play_mode;
	uint32_t header_size;
};

#endif /* __RAWPLAYER_H__ */
