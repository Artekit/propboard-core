/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### AudioFileHelper.h

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

#ifndef __AUDIOFILEHELPER_H__
#define __AUDIOFILEHELPER_H__

#include <stm32f4xx.h>
#include "AudioSource.h"
#include <ff.h>

typedef struct _wav_chunk
{
	uint8_t		riff[4];
	uint32_t	file_size;
	uint8_t		wave[4];
} WAV_CHUNK;

typedef struct _wav_format
{
	uint16_t	fmt_type;
	uint16_t	channels;
	uint32_t	sample_rate;
	uint32_t	byte_rate;
	uint16_t	block_align;
	uint16_t	bits_per_sample;
} WAV_FORMAT;

typedef struct _wav_header
{
	WAV_CHUNK chunk;
	WAV_FORMAT format;
} WAV_HEADER;

class AudioFileHelper
{
public:
	AudioFileHelper();
	~AudioFileHelper();

	bool openWav(const char* name, bool infinite);
	bool openRandomWav(const char* name, uint32_t min, uint32_t max, bool infinite);

	bool openRaw(const char* name, uint32_t samplesize, uint32_t hdrsize, bool infinite);
	bool openRandomRaw(const char* name, const char* ext, uint32_t min, uint32_t max, uint32_t samplesize, uint32_t hdrsize, bool infinite);

	void close();
	bool rewind();
	uint32_t fillBuffer(uint8_t* buffer, uint32_t samples);
	uint32_t getDuration(uint32_t fs = 0);
	uint32_t getFileSize();
	uint32_t getDataSize();

	inline bool eofReached() { return eof; }
	inline bool isOpened() { return opened; }
	inline WAV_HEADER* getWavHeader() { return &wav_header; }
	inline uint8_t getSampleSize() { return sample_size; }

private:
	bool generateRandomFileName(const char* name, const char* ext, uint32_t min, uint32_t max);
	bool parseWavHeader();

	FIL file;
	bool opened;
	bool infinite_mode;
	bool is_raw;
	volatile bool eof;
	uint32_t last_random;
	uint32_t data_end;
	uint8_t sample_size;
	uint32_t header_size;
	WAV_HEADER wav_header;
	char file_name[_MAX_LFN];
};

#endif /* __AUDIOFILEHELPER_H__ */
