/*

 SD - a slightly more friendly wrapper for sdfatlib

 This library aims to expose a subset of SD card functionality
 in the form of a higher level "wrapper" object.

 License: GNU General Public License V3
          (Because sdfatlib is licensed with this.)

 (C) Copyright 2010 SparkFun Electronics

 Adapted to work with FatFS.
 (C) Copyright 2017 Artekit Labs

 */

#include "SD.h"
#include <sdcard.h>

extern FATFS fs;

namespace SDLib {

bool getNextPathComponent(const char *path, uint32_t *p_offset, char *buffer, uint32_t *b_offset, uint32_t len)
{
	uint32_t bufferOffset = *b_offset;
	uint32_t offset = *p_offset;

	// Skip root or other separator
	if (path[offset] == '/')
		offset++;

	// Copy the next next path segment
	while (bufferOffset < len && (path[offset] != '/')	&& (path[offset] != '\0'))
		buffer[bufferOffset++] = path[offset++];

	buffer[bufferOffset] = '\0';

	// Skip trailing separator so we can determine if this
	// is the last component in the path or not.
	if (path[offset] == '/')
		offset++;

	*p_offset = offset;
	*b_offset = bufferOffset;

	return (path[offset] != '\0');
}

SDClass::SDClass(FATFS* fs)
{
	_fs = fs;
}

bool SDClass::begin(uint8_t /* csPin */)
{
	return true;
}

bool SDClass::begin(uint32_t clock, uint8_t csPin)
{
	UNUSED(clock);
	UNUSED(csPin);

	return begin();
}

File SDClass::openDir(const char* path)
{
	DIR* dir = (DIR*) malloc(sizeof(DIR));
	if (dir)
	{
		if (f_opendir(dir, path) == FR_OK)
			return File(dir, path);

		free(dir);
	}

	return File();
}

File SDClass::open(const char *filepath, uint8_t mode)
{
	FILINFO fi;
	uint8_t fatfs_mode = FA_OPEN_EXISTING;

	if (strcmp(filepath, "/") == 0)
		// It's the root directory. f_stat() would say it doesn't
		// exists, so we deal with it here.
		return openDir(filepath);

	// Check if the file exists
	if (f_stat(filepath, &fi) == FR_OK)
	{
		// File exists, check if it's a directory
		if (fi.fattrib & AM_DIR)
			return openDir(filepath);

		if (mode & O_TRUNC)
			fatfs_mode = FA_CREATE_ALWAYS;

		if ((mode & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
			return File();
	} else {
		if (!(mode & O_CREAT))
			return File();

		fatfs_mode = FA_CREATE_NEW;
	}

	if (mode & O_READ)
		fatfs_mode |= FA_READ;

	if (mode & O_WRITE)
		fatfs_mode |= FA_WRITE;

	// It's a file
	FIL* fil = (FIL*) malloc(sizeof(FIL));
	if (fil)
	{
		if (f_open(fil, filepath, fatfs_mode) == FR_OK)
		{
			if ((mode & (O_APPEND | O_WRITE)) == (O_APPEND | O_WRITE))
				f_lseek(fil, f_size(fil));

			return File(fil, filepath, (mode & O_SYNC));
		}

		free(fil);
	}

	return File();
}

File SDClass::open(const char *base_path, const char *filename, uint8_t mode)
{
	if (strcmp(base_path, "/") == 0)
		return open(filename, mode);

	sprintf(_path_buffer, "%s/%s", base_path, filename);
	return open(_path_buffer, mode);
}

bool SDClass::exists(const char *filepath)
{
	FILINFO fi;

	if (f_stat(filepath, &fi) != FR_OK)
		return false;

	return true;
}

bool SDClass::mkdir(const char *filepath)
{
	bool ret = true;
	bool more = false;
	uint32_t offset = 0;
	uint32_t b_offset = 0;
	FRESULT res;

	do
	{
		more = getNextPathComponent(filepath, &offset, _path_buffer, &b_offset, sizeof(_path_buffer));

		res = f_mkdir(_path_buffer);
		if (res != FR_EXIST && res != FR_OK)
		{
			ret = false;
			break;
		}

		_path_buffer[b_offset++] = '/';

	} while (more);

	return ret;
}

bool SDClass::rmdir(const char *filepath)
{
	return remove(filepath);
}

bool SDClass::remove(const char *filepath)
{
	return (f_unlink(filepath) == FR_OK);
}

SDClass SD(&fs);

};
