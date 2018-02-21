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


#include <Arduino.h>

#define CONFIG_MAX_LINE_LEN		255
#define CONFIG_MAX_KEY_LEN		32

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

	bool sectionExists(const char* section);

private:

	int32_t findSection(const char* section);
	bool findKey(const char* section, const char* key, int32_t* section_line, int32_t* key_line, char** key_data);
	char* verifyKey(const char* key, char* line);
	char* getKeyData(const char* section, const char* key);
	bool lineIsCommentedOut(char* line);
	bool getValidSection(char* line);
	bool lineIsEmpty(char* line);
	bool createBackupFile();
	bool replaceFileWithBackup();
	bool readLine(char* line);
	bool readValue(const char* section, const char* key, void* value, DataType value_type, uint32_t* len = NULL);
	bool readArray(const char* section, const char* key, void* values, uint8_t* count, DataType value_type);
	bool writeValues(const char* section, const char* key, void* values, uint32_t count, DataType type);
	bool writeLineToBackup(char* line);
	bool writeKeyToBackup(const char* key, void* values, uint32_t count, DataType type);
	bool copyToBackup(int32_t from, int32_t to);
	char* skipWhiteSpaceAndTabs(char* str);
	bool isASCII(char c);

	FIL _file;
	FIL *_backup_file;
	char _line_buffer_rd[CONFIG_MAX_LINE_LEN];
	char _line_buffer_wr[CONFIG_MAX_LINE_LEN];
	bool _writable;
	char* _file_name;
};
