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

#include <SD.h>
#include <ff.h>

/* for debugging file open/close leaks
   uint8_t nfilecount=0;
*/

File::File()
{
	_file = NULL;
	_dir = NULL;
	sync = false;
}

File::File(DIR* dir, const char* name)
{
	_dir = dir;
	_file = NULL;
	strcpy(_full_name, name);
	sync = false;
}

File::File(FIL* fil, const char* name, bool dosync)
{
	_dir = NULL;
	_file = fil;
	strcpy(_full_name, name);
	sync = dosync;
}

char* File::getFileName()
{
	char* ptr = _full_name;
	char* str = ptr;
	uint32_t offs = 0;
	uint32_t idx = 0;

	if (strcmp(ptr, "/") == 0)
		return ptr;

	do
	{
		idx++;

		if (*ptr == '/')
			offs = idx;

	} while (*ptr++);

	return str+offs;
}

// returns a pointer to the file name
char* File::name()
{
	return getFileName();
}

char* File::fullName()
{
	return _full_name;
}

// a directory is a special type of file
bool File::isDirectory()
{
	return (_dir != NULL);
}


size_t File::write(uint8_t val)
{
	return write(&val, 1);
}

size_t File::write(const uint8_t *buf, size_t size)
{
	unsigned int written;

	if (_file && f_write(_file, buf, size, &written) == FR_OK)
	{
		if (sync && f_sync(_file) != FR_OK)
			return 0;

		return written;
	}

	setWriteError();
	return 0;
}

int File::peek()
{
	if (! _file)
		return 0;

	FSIZE_t ptr = f_tell(_file);

	int c = read();
	if (c != -1)
		f_lseek(_file, ptr);

	return c;
}

int File::read()
{
	int i;

	if (read(&i, 1))
		return i;

	return -1;
}

int File::read(void *buf, uint16_t nbyte)
{
	unsigned int read;

	if (_file && f_read(_file, buf, nbyte, &read) == FR_OK)
		return read;

    return -1;
}

int File::available()
{
	if (! _file)
		return 0;

	return (f_size(_file) - f_tell(_file));
}

void File::flush()
{
	if (_file)
		f_sync(_file);
}

bool File::seek(uint32_t pos)
{
	if (_file && f_lseek(_file, pos) == FR_OK)
		return true;

	return false;
}

uint32_t File::position()
{
	if (! _file)
		return -1;

	return f_tell(_file);
}

uint32_t File::size()
{
	if (! _file)
		return 0;

	return f_size(_file);
}

void File::close()
{
	// This class has to manage both dirs and files so
	// take care of freeing the resources for both types
	if (_file)
	{
		f_close(_file);
		free(_file);
		_file = NULL;
		return;
	}

	if (_dir)
	{
		f_closedir(_dir);
		free(_dir);
		_dir = NULL;
		return;
	}
}

File::operator bool()
{
	if (_file == NULL && _dir == NULL)
		return false;

	return true;
}

// allows you to recurse into a directory
File File::openNextFile(uint8_t mode)
{
	FILINFO fi;

	if (_dir == NULL)
		return File();

	if (f_readdir(_dir, &fi) == FR_OK)
	{
		// End?
		if (*fi.fname == 0)
			return File();

		return SD.open(_full_name, fi.fname, mode);
	}

	return File();
}

void File::rewindDirectory(void)
{
	if (_dir != NULL)
		f_readdir(_dir, NULL);
}

