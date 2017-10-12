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

#ifndef __SD_H__
#define __SD_H__

#include <Arduino.h>
#include <ff.h>

/* Took from SdFat.h */
// use the gnu style oflag in open()
/** open() oflag for reading */
uint8_t const O_READ = 0X01;
/** open() oflag - same as O_READ */
uint8_t const O_RDONLY = O_READ;
/** open() oflag for write */
uint8_t const O_WRITE = 0X02;
/** open() oflag - same as O_WRITE */
uint8_t const O_WRONLY = O_WRITE;
/** open() oflag for reading and writing */
uint8_t const O_RDWR = (O_READ | O_WRITE);
/** open() oflag mask for access modes */
uint8_t const O_ACCMODE = (O_READ | O_WRITE);
/** The file offset shall be set to the end of the file prior to each write. */
uint8_t const O_APPEND = 0X04;
/** synchronous writes - call sync() after each write */
uint8_t const O_SYNC = 0X08;
/** create the file if nonexistent */
uint8_t const O_CREAT = 0X10;
/** If O_CREAT and O_EXCL are set, open() shall fail if the file exists */
uint8_t const O_EXCL = 0X20;
/** truncate the file to zero length */
uint8_t const O_TRUNC = 0X40;

#define FILE_READ FA_READ
#define FILE_WRITE (FA_READ | FA_WRITE | FA_OPEN_ALWAYS)

namespace SDLib {

class SDClass;

class File : public Stream
{
	friend class SDClass;

private:
	File(DIR* dir, const char* name);
	File(FIL* fil, const char* name, bool dosync);

	char* getFileName();

	char _full_name[_MAX_LFN + 1];
	FIL		*_file; // underlying file pointer
	DIR		*_dir;	// this library uses class File to manage directories so
					// we dont't have much options that to include a DIR object
					// to handle them.
	bool sync;

public:
	File();							// 'empty' constructor
	virtual size_t write(uint8_t);
	virtual size_t write(const uint8_t *buf, size_t size);
	virtual int read();
	virtual int peek();
	virtual int available();
	virtual void flush();
	int read(void *buf, uint16_t nbyte);
	bool seek(uint32_t pos);
	uint32_t position();
	uint32_t size();
	void close();
	operator bool();
	char* name();
	char* fullName();

	bool isDirectory();
	File openNextFile(uint8_t mode = FILE_READ);
	void rewindDirectory();

	using Print::write;
};

class SDClass
{

public:

	SDClass(FATFS* fs);

	// This needs to be called to set up the connection to the SD card
	// before other methods are used.
	bool begin(uint8_t csPin = 0);
	bool begin(uint32_t clock, uint8_t csPin);

	// Open the specified file/directory with the supplied mode (e.g. read or
	// write, etc). Returns a File object for interacting with the file.
	// Note that currently only one file can be open at a time.
	File open(const char *filename, uint8_t mode = FILE_READ);
	File open(const String &filename, uint8_t mode = FILE_READ) { return open( filename.c_str(), mode ); }

	// Methods to determine if the requested file path exists.
	bool exists(const char *filepath);
	bool exists(const String &filepath) { return exists(filepath.c_str()); }

	// Create the requested directory heirarchy--if intermediate directories
	// do not exist they will be created.
	bool mkdir(const char *filepath);
	bool mkdir(const String &filepath) { return mkdir(filepath.c_str()); }

	// Delete the file.
	bool remove(const char *filepath);
	bool remove(const String &filepath) { return remove(filepath.c_str()); }

	bool rmdir(const char *filepath);
	bool rmdir(const String &filepath) { return rmdir(filepath.c_str()); }

private:

	FATFS* _fs;
	char _path_buffer[_MAX_LFN + 1];	// used for walking directories
	File openDir(const char* path);
	File open(const char *base_path, const char *filename, uint8_t mode = FILE_READ);

	friend class File;
};

extern SDClass SD;

};

// We enclose File and SD classes in namespace SDLib to avoid conflicts
// with others legacy libraries that redefines File class.

// This ensure compatibility with sketches that uses only SD library
using namespace SDLib;

// This allows sketches to use SDLib::File with other libraries (in the
// sketch you must use SDFile instead of File to disambiguate)
typedef SDLib::File    SDFile;
typedef SDLib::SDClass SDFileSystemClass;
#define SDFileSystem   SDLib::SD

#endif
