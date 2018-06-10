/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### PropConfig.h

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

#ifndef __PROPCONFIG_H__
#define __PROPCONFIG_H__

#include <Arduino.h>

#define CONFIG_MAX_LINE_LEN		255
#define CONFIG_MAX_SECTION_LEN	32

#define CONFIG_USE_PRELOAD
#define CONFIG_PRELOAD_SIZE		1024

typedef enum
{
	TypeUnsigned8,
	TypeSigned8,
	TypeUnsigned16,
	TypeSigned16,
	TypeUnsigned32,
	TypeSigned32,
	TypeFloat,
	TypeString,
	TypeBool,
} DataType;

typedef enum
{
	lineAny,
	lineEmpty,
	lineSection,
	lineComment,
	lineGarbage,
	lineKey,
	lineEmptyKey
} LineType;

typedef enum
{
	findError = -3,
	noSection = -2,
	noKey = -1,
	keyFound = 0,
	sectionFound = 1

} FindResult;

typedef bool (configFileMapCallback)(uint32_t, uint32_t, char*, void*);

class PropConfig
{
	
public:
	PropConfig();
	~PropConfig();
	bool begin(const char* path, bool writable = true);

	bool readValue(const char* section, const char* key, int8_t* value);
	bool readValue(const char* section, const char* key, uint8_t* value);
	bool readValue(const char* section, const char* key, int16_t* value);
	bool readValue(const char* section, const char* key, uint16_t* value);
	bool readValue(const char* section, const char* key, int32_t* value);
	bool readValue(const char* section, const char* key, uint32_t* value);
	bool readValue(const char* section, const char* key, char* value, uint32_t* len);
	bool readValue(const char* section, const char* key, float* value);
	bool readValue(const char* section, const char* key, bool* value);

	bool readArray(const char* section, const char* key, int8_t* values, uint8_t* count);
	bool readArray(const char* section, const char* key, uint8_t* values, uint8_t* count);
	bool readArray(const char* section, const char* key, int16_t* values, uint8_t* count);
	bool readArray(const char* section, const char* key, uint16_t* values, uint8_t* count);
	bool readArray(const char* section, const char* key, int32_t* values, uint8_t* count);
	bool readArray(const char* section, const char* key, uint32_t* values, uint8_t* count);
	bool readArray(const char* section, const char* key, float* values, uint8_t* count);
	
	bool writeValue(const char* section, const char* key, int8_t value);
	bool writeValue(const char* section, const char* key, uint8_t value);
	bool writeValue(const char* section, const char* key, int16_t value);
	bool writeValue(const char* section, const char* key, uint16_t value);
	bool writeValue(const char* section, const char* key, int value);
	bool writeValue(const char* section, const char* key, int32_t value);
	bool writeValue(const char* section, const char* key, uint32_t value);
	bool writeValue(const char* section, const char* key, const char* value);
	bool writeValue(const char* section, const char* key, float value);
	bool writeValue(const char* section, const char* key, bool value);

	bool writeArray(const char* section, const char* key, int8_t* values, uint8_t count);
	bool writeArray(const char* section, const char* key, uint8_t* values, uint8_t count);
	bool writeArray(const char* section, const char* key, int16_t* values, uint8_t count);
	bool writeArray(const char* section, const char* key, uint16_t* values, uint8_t count);
	bool writeArray(const char* section, const char* key, int32_t* values, uint8_t count);
	bool writeArray(const char* section, const char* key, uint32_t* values, uint8_t count);
	bool writeArray(const char* section, const char* key, float* values, uint8_t count);

	bool readValue(uint32_t token, int8_t* value);
	bool readValue(uint32_t token, uint8_t* value);
	bool readValue(uint32_t token, int16_t* value);
	bool readValue(uint32_t token, uint16_t* value);
	bool readValue(uint32_t token, int32_t* value);
	bool readValue(uint32_t token, uint32_t* value);
	bool readValue(uint32_t token, char* value, uint32_t* len);
	bool readValue(uint32_t token, float* value);
	bool readValue(uint32_t token, bool* value);
	bool readValue(uint32_t token, void* value, DataType value_type, uint32_t* len = NULL);

	bool readArray(uint32_t token, int8_t* values, uint8_t* count);
	bool readArray(uint32_t token, uint8_t* values, uint8_t* count);
	bool readArray(uint32_t token, int16_t* values, uint8_t* count);
	bool readArray(uint32_t token, uint16_t* values, uint8_t* count);
	bool readArray(uint32_t token, int32_t* values, uint8_t* count);
	bool readArray(uint32_t token, uint32_t* values, uint8_t* count);
	bool readArray(uint32_t token, float* values, uint8_t* count);

	bool startSectionScan(const char* section);
	bool startSectionScan(uint32_t file_offset);
	bool getNextKey(uint32_t* token, char** key, uint32_t* key_len);
	void endSectionScan();
	bool sectionExists(const char* section);
	bool mapSections(configFileMapCallback* func, void* param = NULL);

	uint32_t getFileRWPointer();
	bool setFileRWPointer(uint32_t pos);

protected:

	FindResult findSection(const char* section);
	FindResult findKey(const char* section, const char* key, char** key_data);
	char* getKeyData(const char* section, const char* key);
	bool createBackupFile();
	bool replaceFileWithBackup();
	bool readLine(char* line);
	bool readValue(const char* section, const char* key, void* value, DataType value_type, uint32_t* len = NULL);
	bool readArray(uint32_t token, void* values, uint8_t* count, DataType value_type);
	bool readArray(const char* section, const char* key, void* values, uint8_t* count, DataType value_type);
	bool writeValues(const char* section, const char* key, void* values, uint32_t count, DataType type);
	bool writeLineToBackup(char* line);
	bool writeKeyToBackup(const char* key, void* values, uint32_t count, DataType type);
	bool copyToBackup(int32_t from, int32_t to);
	LineType parseLine(char* line, char** section, char** key, char** data, LineType looking_for);
	void removeWhiteSpace(char* str);

	FIL _file;
	FIL *_backup_file;
	char _line_buffer_rd[CONFIG_MAX_LINE_LEN];
	char _line_buffer_wr[CONFIG_MAX_LINE_LEN];
	bool _writable;
	char* _file_name;
	char last_section[CONFIG_MAX_SECTION_LEN];
	uint32_t last_section_offs;
	bool section_locked;

#ifdef CONFIG_USE_PRELOAD
	bool preload();
	uint8_t preload_buffer[CONFIG_PRELOAD_SIZE];
	uint32_t preload_size;
	uint32_t preload_offset;
#endif

};

#endif /* __PROPCONFIG_H__ */
