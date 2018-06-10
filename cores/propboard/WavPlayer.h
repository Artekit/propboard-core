/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### WavPlayer.h

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

#ifndef __WAVPLAYER_H__
#define __WAVPLAYER_H__

#include <stm32f4xx.h>
#include "AudioSource.h"
#include <ff.h>
#include "AudioFileHelper.h"
#include "RawPlayer.h"

class WavPlayer : public RawPlayer
{
public:
	WavPlayer();
	~WavPlayer();

	bool play(const char* filename, PlayMode mode = PlayModeNormal);
	bool playRandom(const char* filename, uint32_t min, uint32_t max, PlayMode mode = PlayModeNormal);

protected:

	bool beginAndPlay(PlayMode mode);
};

#endif /* __WAVPLAYER_H__ */
