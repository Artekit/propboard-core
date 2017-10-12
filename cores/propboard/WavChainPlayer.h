/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### WavChainPlayer.h

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

#ifndef __WAVCHAINPLAYER_H__
#define __WAVCHAINPLAYER_H__

#include "RawChainPlayer.h"

class WavChainPlayer : public RawChainPlayer
{
public:
	WavChainPlayer();
	~WavChainPlayer();

	bool begin(const char* filename);
	bool chain(const char* filename, PlayMode mode = PlayModeNormal);
	bool chainRandom(const char* filename, uint32_t min, uint32_t max, PlayMode mode = PlayModeNormal);

private:

	bool doChain(PlayMode mode);
};

#endif /* __WAVCHAINPLAYER_H__ */

