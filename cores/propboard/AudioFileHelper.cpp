/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### AudioFileHelper.cpp

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

#include "AudioFileHelper.h"
#include "Arduino.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern uint32_t getRandom(uint32_t min, uint32_t max);

AudioFileHelper::AudioFileHelper()
{
	infinite_mode = 0;
	eof = false;
	sample_size = 0;
	opened = false;
	last_random = 0xFFFFFFFF;
	header_size = 0;
	is_raw = false;
	data_end = 0;
}

AudioFileHelper::~AudioFileHelper()
{
	if (opened)
		f_close(&file);
}

bool AudioFileHelper::parseWavHeader()
{
	UINT read;
	uint8_t chunk_name[4];
	uint32_t chunk_size;
	bool fmt_found = false;
	bool data_found = false;

	header_size = 0;
	data_end = 0;

	if (f_read(&file, &wav_header.chunk, sizeof(WAV_CHUNK), &read) != FR_OK || read != sizeof(WAV_CHUNK))
		return false;

	if (memcmp(wav_header.chunk.riff, "RIFF", 4) != 0)
		return false;

	if (memcmp(wav_header.chunk.wave, "WAVE", 4) != 0)
		return false;

	header_size += sizeof(WAV_CHUNK);

	while (!fmt_found || !data_found)
	{
		if (f_read(&file, &chunk_name, sizeof(chunk_name), &read) != FR_OK || read != sizeof(chunk_name))
			return false;

		if (f_read(&file, &chunk_size, sizeof(chunk_size), &read) != FR_OK || read != sizeof(chunk_size))
			return false;

		header_size += 8;

		if (memcmp(chunk_name, "fmt ", 4) == 0)
		{
			if (chunk_size < sizeof(WAV_FORMAT))
				return false;

			if (f_read(&file, &wav_header.format, sizeof(WAV_FORMAT), &read) != FR_OK ||
				read != sizeof(WAV_FORMAT))
				return false;

			header_size += chunk_size;

			// Validate
			if (!wav_header.format.channels || wav_header.format.channels > 2)
				return false;

			// Get sample_size; we'll be using this in the fillBuffer() function
			sample_size = (wav_header.format.bits_per_sample / 8) * wav_header.format.channels;

			fmt_found = true;
		} else if (memcmp(chunk_name, "data", 4) == 0)
		{
			if (header_size + chunk_size - 8 != wav_header.chunk.file_size)
				data_end = header_size + chunk_size;

			data_found = true;
		} else {
			header_size += chunk_size;
		}

		if (header_size != file.fptr)
		{
			if (f_lseek(&file, header_size) != FR_OK)
				return false;
		}
	}

	return (fmt_found && data_found);
}

bool AudioFileHelper::openWav(const char* name, bool infinite)
{
	if (opened)
	{
		f_close(&file);
		opened = false;
	}

	if (f_open(&file, name, FA_READ | FA_OPEN_EXISTING) != FR_OK)
		return false;

	// Parse WAV header
	if (!parseWavHeader())
	{
		close();
		return false;
	}

	is_raw = false;
	opened = true;
	infinite_mode = infinite;
	eof = false;
	strcpy(file_name, name);
	return true;
}

bool AudioFileHelper::openRandomWav(const char* name, uint32_t min, uint32_t max, bool infinite)
{
	generateRandomFileName(name, "wav", min, max);
	return openWav(file_name, infinite);
}

bool AudioFileHelper::openRaw(const char* name, uint32_t samplesize, uint32_t hdrsize, bool infinite)
{
	header_size = hdrsize;
	sample_size = samplesize;

	if (opened)
	{
		f_close(&file);
		opened = false;
	}

	if (f_open(&file, name, FA_READ | FA_OPEN_EXISTING) != FR_OK)
		return false;

	// Skip header
	if (f_lseek(&file, header_size) != FR_OK)
	{
		close();
		return false;
	}

	is_raw = true;
	opened = true;
	infinite_mode = infinite;
	eof = false;
	strcpy(file_name, name);
	return true;
}

bool AudioFileHelper::openRandomRaw(const char* name, const char* ext, uint32_t min, uint32_t max, uint32_t samplesize, uint32_t hdrsize, bool infinite)
{
	generateRandomFileName(name, ext, min, max);
	return openRaw(file_name, samplesize, hdrsize, infinite);
}

void AudioFileHelper::close()
{
	if (opened)
	{
		f_close(&file);
		opened = false;
	}
}

bool AudioFileHelper::rewind()
{
	if (file.fptr != header_size)
	{
		if (f_lseek(&file, header_size) != FR_OK)
			return false;
	}

	eof = false;
	return true;
}

uint32_t AudioFileHelper::getSamplesLeft()
{
	uint32_t bytes;

	if (!opened || eof)
		return 0;

	// data_end holds the location in the file where samples data ends
	// If data_end == 0, then sample data ends at EOF
	bytes = getFileSize() - file.fptr - data_end;
	return (bytes / sample_size);
}

uint32_t AudioFileHelper::fillBuffer(uint8_t* buffer, uint32_t samples)
{
	uint32_t to_read;
	UINT read;
	uint32_t samples_read = 0;

	if (!opened || eof)
		return 0;

	to_read = samples * sample_size;

	if (f_read(&file, buffer, to_read, &read) != FR_OK)
		return 0;

	// data_end holds the location in the file where samples data ends
	// If data_end == 0, then sample data ends at EOF
	if (data_end && read && (file.fptr > data_end))
		read -= (file.fptr - data_end);

	samples_read = read / sample_size;

	if (read != to_read || (data_end && file.fptr >= data_end))
	{
		// EOF
		if (!infinite_mode)
		{
			// Signal EOF
			eof = true;
		} else {
			// EOF, move the file pointer back to the beginning
			if (!rewind())
				return 0;

			to_read -= read;

			if (to_read)
			{
				buffer += read;
				if (f_read(&file, buffer, to_read, &read) != FR_OK)
					return 0;

				samples_read += read / sample_size;
			}
		}
	}

	return samples_read;
}

bool AudioFileHelper::generateRandomFileName(const char* name, const char* ext, uint32_t min, uint32_t max)
{
	uint32_t num;

	if (min == max)
	{
		num = min;
	} else {
		if (min > max)
		{
			// Move along
			uint32_t tmp = min;
			min = max;
			max = tmp;
		}

		while (true)
		{
			num = getRandom(min, max);
			if (num != last_random)
				break;
		}
	}

	last_random = num;

	sprintf(file_name, "%s%lu.%s", name, num, ext);
	return true;
}

uint32_t AudioFileHelper::getDuration(uint32_t fs)
{
	uint32_t size = 0;
	uint32_t rate;

	if (!opened)
		return 0;

	if (is_raw)
	{
		size = getDataSize();
		rate = fs;

		if (!size || !fs)
			return 0;
	} else {
		size = wav_header.chunk.file_size;
		rate = wav_header.format.sample_rate;
	}

	return ((size / sample_size) * 1000) / rate;
}

uint32_t AudioFileHelper::getFileSize()
{
	if (!opened)
		return 0;

	return f_size(&file);
}

uint32_t AudioFileHelper::getDataSize()
{
	uint32_t size;

	if (!opened)
		return 0;

	if (is_raw)
	{
		size = getFileSize();
		if (size)
			return size - header_size - data_end;
		else
			return 0;
	} else {
		return  wav_header.chunk.file_size;
	}
}
