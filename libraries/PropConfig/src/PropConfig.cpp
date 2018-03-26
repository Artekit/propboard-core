/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### PropConfig.cpp

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

#include "PropConfig.h"

PropConfig::PropConfig()
{
	_backup_file = NULL;
	_writable = false;
	_file_name = NULL;
	last_section_line = 0;
	last_section_offs = 0;
	section_locked = false;
}

PropConfig::~PropConfig()
{
}

bool PropConfig::createBackupFile()
{
	if (!_writable)
		return false;

	if (_backup_file)
	{
		f_close(_backup_file);
	} else {
		_backup_file = (FIL*) malloc(sizeof(FIL));
		if (!_backup_file)
			return false;
	}

	if (f_open(_backup_file, "backup", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
		return false;
	
	return true;
}

bool PropConfig::replaceFileWithBackup()
{
	// Close the file
	if (f_close(&_file) != FR_OK)
		return false;

	// Close the backup
	if (f_close(_backup_file) != FR_OK)
		return false;

	// Delete the original file
	if (f_unlink(_file_name) != FR_OK)
		return false;

	// Rename backup as the original file name
	if (f_rename("backup", _file_name) != FR_OK)
		return false;

	// Open the file again
	if (f_open(&_file, _file_name, FA_OPEN_EXISTING | FA_READ) != FR_OK)
		return false;

	// Invalidate last read section name, offset and line
	memset(last_section, 0x00, CONFIG_MAX_SECTION_LEN);
	last_section_offs = 0;
	last_section_line = 0;

	return true;
}

/* Finds a section and leaves the cursor at the first line within the section */
int32_t PropConfig::findSection(const char* section)
{
	int32_t line = 0;

	// If section is the same as the last read section, jump to it
	if (strlen(last_section) == strlen(section) &&
		strncasecmp(last_section, section, strlen(last_section)) == 0)
	{
		if (f_lseek(&_file, last_section_offs) != FR_OK)
			return -1;

		return last_section_line;
	}

	// Otherwise scan the entire file
	if (f_lseek(&_file, 0) != FR_OK)
		return -1;

	while (readLine(_line_buffer_rd))
	{
		line++;

		if (lineIsEmpty(_line_buffer_rd) || lineIsCommentedOut(_line_buffer_rd))
			continue;

		if (getValidSection(_line_buffer_rd))
		{
			if (strlen(section) == strlen(_line_buffer_rd))
			{
				if (strncasecmp(_line_buffer_rd, section, strlen(_line_buffer_rd)) == 0)
				{
					// Remember the last section name, offset and line
					strncpy(last_section, section, CONFIG_MAX_SECTION_LEN);
					last_section_offs = _file.fptr;
					last_section_line = line;
					return line;
				}
			}
		}
	}

	return -1;
}

bool PropConfig::sectionExists(const char* section)
{
	return (findSection(section) > 0);
}

bool PropConfig::lineIsEmpty(char* line)
{
	if (*line == '\n' || *line == '\0')
		return true;

	if (*line == '\r' && *(line + 1) == '\n')
		return true;
	
	return false;
}

bool PropConfig::findKey(const char* section, const char* key, int32_t* section_line, int32_t* key_line, char** key_data)
{
	int32_t line = findSection(section);

	if (section_line)
		*section_line = line;

	if (line == -1)
		return false;

	while (readLine(_line_buffer_rd))
	{
		line++;

		if (lineIsEmpty(_line_buffer_rd) || lineIsCommentedOut(_line_buffer_rd))
			continue;

		if (getValidSection(_line_buffer_rd))
			// Found another section while searching for a key, so ...
			break;

		char* data = verifyKey(key, _line_buffer_rd);
		if (data)
		{
			if (key_data)
				*key_data = data;

			if (key_line)
				*key_line = line;

			return true;
		}
	}

	return false;
}

char* PropConfig::getKeyData(const char* section, const char* key)
{
	char *data;
	if (findKey(section, key, NULL, NULL, &data))
		return data;
	
	return NULL;
}

bool PropConfig::readLine(char* line)
{
	char c;
	uint32_t idx = 0;
	UINT read;

	// TODO come back with a better idea instead of this
	while (idx < CONFIG_MAX_LINE_LEN)
	{
		if (f_read(&_file, &c, 1, &read) != FR_OK || read != 1)
			return false;
		
		if (c == '\r' || c == '\0')
			continue;
		
		if (c == '\n')
			break;
		
		line[idx++] = c;
	}

	if (idx == CONFIG_MAX_LINE_LEN)
		// Line is too long, return an error
		return false;
	else
		// Ensure null termination
		line[idx] = 0;

	return true;
}

bool PropConfig::lineIsCommentedOut(char* line)
{
	while (*line && (*line == 0x09 || *line == 0x20))
		// Ignore tabs and spaces
		line++;

	if (*line == '\0')
		return false;

	if (*line == '#' || *line == ';')
		return true;
			
	if (*line == '/' && *(line+1) == '/')
		return true;

	return false;
}

/*
 * This function checks for a valid section, that is:
 * - a string contained between [] characters
 * - cannot be of length = 0
 */
bool PropConfig::getValidSection(char* line)
{
	uint32_t idx = 0;
	uint32_t cpyidx = 0;
	bool opener_found = false;
	
	while (idx < CONFIG_MAX_LINE_LEN && line[idx])
	{
		if (line[idx] == '[')
		{
			if (opener_found)
				// Double '[' character not allowed
				return false;
			
			opener_found = true;
		} else if (line[idx] == ']')
		{
			// Closure without opener not allowed
			if (!opener_found)
				return false;
			
			line[cpyidx] = '\0';
			return true;
		} else if (opener_found)
		{
			line[cpyidx++] = line[idx];
			if (cpyidx == CONFIG_MAX_SECTION_LEN - 1)
				return false;
		}

		idx++;
	}
	
	return false;
}

char* PropConfig::skipWhiteSpaceAndTabs(char* str)
{
	while (*str)
	{
		if (*str == 0x20 || *str == 0x09)
			str++;
		else
			break;
	}

	return str;
}

/* 
Checks if the line contains the wanted key and returns a pointer
to the data string within the line.
*/
char* PropConfig::verifyKey(const char* key, char* line, char** key_name, uint32_t* key_name_len)
{
	uint32_t count = 0;
	char* ptr;
	char* data_ptr;
	char* key_ptr;
	
	key_ptr = skipWhiteSpaceAndTabs(line);
	if (!key_ptr)
		return NULL;

	ptr = key_ptr;

	// Find  '='
	while (*ptr)
	{
		if (*ptr == '=')
			break;
		ptr++;
	}

	if (!*ptr || ptr == key_ptr)
		return NULL;

	count = (uint32_t) ptr - (uint32_t) key_ptr;
	data_ptr = ptr + 1;
	ptr -= 1;

	// Scan backwards and remove whitespace from the key length count
	while (ptr != key_ptr)
	{
		if (*ptr == 0x20 || *ptr == 0x09)
			count--;
		else break;

		ptr--;
	}

	// If key is NULL we just want the pointer to the data
	if (key)
	{
		if (count != strlen(key))
			// Not our key
			return NULL;

		if (strncasecmp(key_ptr, key, count) != 0)
			// Not our key
			return NULL;
	}

	if (key_name)
		*key_name = key_ptr;

	if (key_name_len)
		*key_name_len = count;

	// Get the data
	data_ptr = skipWhiteSpaceAndTabs(data_ptr);
	return data_ptr;
}

bool PropConfig::readArray(uint32_t token, void* values, uint8_t* count, DataType value_type)
{
	uint8_t idx = 0;
	char* data = (char*) token;
	uint8_t value_size;
	char* end;

	if (!values || !count || *count == 0)
		return false;

	if (value_type == TypeSigned8 || value_type == TypeUnsigned8)
		value_size = 1;
	else if (value_type == TypeSigned16 || value_type == TypeUnsigned16)
		value_size = 2;
	else if (value_type == TypeSigned32 || value_type == TypeUnsigned32 || value_type == TypeFloat)
		value_size = 4;
	else
		return false;

	memset(values, 0, value_size * *count);

	if (strlen(data) == 0)
		return true;

	do
	{
		if (value_type == TypeSigned8)
		{
			int8_t* ptr = ((int8_t*) values) + idx;
			*ptr = (int8_t) strtol(data, &end, 10);
		} else if (value_type == TypeUnsigned8)
		{
			uint8_t* ptr = ((uint8_t*) values) + idx;
			*ptr = (uint8_t) strtoul(data, &end, 10);
		} else if (value_type == TypeSigned16)
		{
			int16_t* ptr = ((int16_t*) values) + idx;
			*ptr = (int16_t) strtol(data, &end, 10);
		} else if (value_type == TypeUnsigned16)
		{
			uint16_t* ptr = ((uint16_t*) values) + idx;
			*ptr = (uint16_t) strtoul(data, &end, 10);
		} else if (value_type == TypeSigned32)
		{
			int32_t* ptr = ((int32_t*) values) + idx;
			*ptr = (int32_t) strtol(data, &end, 10);
		} else if (value_type == TypeUnsigned32)
		{
			uint32_t* ptr = ((uint32_t*) values) + idx;
			*ptr = (uint32_t) strtoul(data, &end, 10);
		} else if (value_type == TypeFloat)
		{
			float *ptr = ((float*) values) + idx;
			*ptr = strtof(data, &end);
		}

		idx++;

		if (!end || *end == '\0')
			break;

		data = ++end;

	} while (idx < *count && *data);

	*count = idx;
	return true;
}

bool PropConfig::readArray(const char* section, const char* key, void* values, uint8_t* count, DataType value_type)
{
	char* data = getKeyData(section, key);

	if (!data)
		return false;

	return readArray((uint32_t) data, values, count, value_type);
}

bool PropConfig::writeKeyToBackup(const char* key, void* values, uint32_t count, DataType type)
{
	// Uses _line_buffer_rd as a helper array
	int written = 0;
	written = snprintf(_line_buffer_wr, CONFIG_MAX_LINE_LEN, "%s=", key);

	while(count)
	{
		count--;

		switch(type)
		{
			case TypeUnsigned32:
			case TypeUnsigned8:
				written += sprintf(_line_buffer_rd, "%u", *((unsigned int*) values));
				break;

			case TypeSigned32:
			case TypeSigned8:
				written += sprintf(_line_buffer_rd, "%i", *((int*) values));
				break;

			case TypeUnsigned16:
				written += sprintf(_line_buffer_rd, "%uh", *((uint16_t*) values));
				break;

			case TypeSigned16:
				written += sprintf(_line_buffer_rd, "%ih", *((short*) values));
				break;

			case TypeFloat:
				written += sprintf(_line_buffer_rd, "%.02f", *((float*) values));
				break;

			case TypeString:
				written += snprintf(_line_buffer_rd, CONFIG_MAX_LINE_LEN, "%s", (char*) values);
				break;

			case TypeBool:
				written += sprintf(_line_buffer_rd, "%i", *((bool*) values) ? 1 : 0);
				break;
		}

		if (written < CONFIG_MAX_LINE_LEN)
		{
			strcat(_line_buffer_wr, _line_buffer_rd);
			if (count)
				strcat(_line_buffer_wr, ",");
		}

		if (!writeLineToBackup(_line_buffer_wr))
			return false;
	}

	return true;
}

bool PropConfig::writeValues(const char* section, const char* key, void* values, uint32_t count, DataType type)
{
	int32_t section_line;
	int32_t key_line;
	int32_t line = 1;
	bool done = false;

	// Write is not allowed while locked into a section (for reading)
	if (section_locked)
		return false;

	if (!createBackupFile())
		return false;

	// Check if the section and key exist
	findKey(section, key, &section_line, &key_line, NULL);

	// Start copying
	f_lseek(&_file, 0);
	while (readLine(_line_buffer_rd))
	{
		if (!done)
		{
			// If the section exists
			if (section_line != -1)
			{
				// If the key exists
				if (line == key_line)
				{
					// The line we have just read is the line we need to update
					if (!writeKeyToBackup(key, values, count, type))
						return false;

					done = true;
					continue;
				} else if (line == section_line && key_line == -1)
				{
					// The line we have just read is the start of the section
					// we need to add the key

					// Write the section
					if (!writeLineToBackup(_line_buffer_rd))
						return false;

					// Add the key
					if (!writeKeyToBackup(key, values, count, type))
						return false;

					done = true;
					continue;
				}
			}
		}

		writeLineToBackup(_line_buffer_rd);
		line++;
	}

	if (!done && section_line == -1)
	{
		// End of file reached
		// We have to write the new section and key
		snprintf(_line_buffer_rd, CONFIG_MAX_LINE_LEN, "[%s]", section);
		if (!writeLineToBackup(_line_buffer_rd))
			return false;

		if (!writeKeyToBackup(key, values, count, type))
			return false;
	}

	return replaceFileWithBackup();
}

bool PropConfig::readValue(uint32_t token, void* value, DataType value_type, uint32_t* len)
{
	char* data = (char*) token;

	switch (value_type)
	{
		case TypeUnsigned8:
			*((uint8_t*) value) = (uint8_t) strtoul(data, NULL, 10);
			break;

		case TypeSigned8:
			*((int8_t*) value) = (int8_t) strtol(data, NULL, 10);
			break;

		case TypeUnsigned16:
			*((uint16_t*) value) = (uint16_t) strtoul(data, NULL, 10);
			break;

		case TypeSigned16:
			*((int16_t*) value) = (int16_t) strtol(data, NULL, 10);
			break;

		case TypeUnsigned32:
			*((uint32_t*) value) = strtoul(data, NULL, 10);
			break;

		case TypeSigned32:
			*((int32_t*) value) = (int32_t) strtol(data, NULL, 10);
			break;

		case TypeFloat:
			*((float*) value) = atof(data);
			break;

		case TypeString:
		{
			if (len == NULL || *len == 0)
				return false;

			// Scan backwards and remove whitespace
			uint32_t offs = strlen((char*) data);
			char* str = data + offs;
			while (offs)
			{
				str--;
				if (*str == 0x20 || *str == 0x09)
					offs--;
				else break;
			}

			if (offs > *len)
				offs = *len;

			strncpy((char*) value, data, offs);
			*(((char*) value) + offs) = '\0';
			*len = offs;
			break;
		}

		case TypeBool:
			if (strcasecmp(data, "false") == 0 ||
				strcasecmp(data, "0") == 0 ||
				strcasecmp(data, "disabled") == 0 ||
				strcasecmp(data, "no") == 0)
			{
				*((bool*) value) = false;
			}

			if (strcasecmp(data, "true") == 0 ||
				strcasecmp(data, "1") == 0 ||
				strcasecmp(data, "enabled") == 0 ||
				strcasecmp(data, "yes") == 0)
			{
				*((bool*) value) = true;
			}
			break;

		default:
			return false;
	}

	return true;
}

bool PropConfig::readValue(const char* section, const char* key, void* value, DataType value_type, uint32_t* len)
{
	if (section_locked || !section || !key || !value)
		return false;

	char* data = getKeyData(section, key);
	if (!data)
		return false;

	return readValue((uint32_t) data, value, value_type, len);
}

bool PropConfig::writeLineToBackup(char* line)
{
	UINT written;
	uint32_t len = strlen(line);

	if (f_write(_backup_file, line, len, &written) != FR_OK || written != len)
		return false;

	if (f_write(_backup_file, "\r\n", 2, &written) != FR_OK || written != 2)
		return false;

	return true;
}

bool PropConfig::begin(const char* path, bool writable)
{
	f_close(&_file);

	// Open file
	if (f_open(&_file, path, FA_OPEN_EXISTING | FA_READ) != FR_OK)
		return false;

	_writable = writable;

	if (_writable && !_file_name)
	{
		_file_name = (char*) malloc(strlen(path));
		if (!_file_name)
		{
			f_close(&_file);
			return false;
		}

		strcpy(_file_name, path);
	}

	return true;
}

bool PropConfig::startSectionScan(const char* section)
{
	if (section_locked)
		return false;

	int32_t line = findSection(section);
	if (line > 0)
	{
		section_locked = true;
		return true;
	}

	return false;
}

bool PropConfig::getNextKey(uint32_t* token, char** key, uint32_t* key_len)
{
	if (!section_locked || !token || !key || !key_len)
		return NULL;

	while (readLine(_line_buffer_rd))
	{
		if (lineIsEmpty(_line_buffer_rd) || lineIsCommentedOut(_line_buffer_rd))
			continue;

		if (getValidSection(_line_buffer_rd))
			// Found another section while searching for a key, so ...
			return false;

		char* data = verifyKey(NULL, _line_buffer_rd, key, key_len);
		if (data)
		{
			*token = (uint32_t) data;
			return true;
		}
	}

	// End of file or error
	return false;
}

void PropConfig::endSectionScan()
{
	section_locked = false;
}

bool PropConfig::readValue(const char* section, const char* key, int8_t* value)
{
	return readValue(section, key, value, TypeSigned8);
}

bool PropConfig::readValue(const char* section, const char* key, uint8_t* value)
{
	return readValue(section, key, value, TypeUnsigned8);
}

bool PropConfig::readValue(const char* section, const char* key, int16_t* value)
{
	return readValue(section, key, value, TypeSigned16);
}

bool PropConfig::readValue(const char* section, const char* key, uint16_t* value)
{
	return readValue(section, key, value, TypeUnsigned16);
}

bool PropConfig::readValue(const char* section, const char* key, int32_t* value)
{
	return readValue(section, key, value, TypeSigned32);
}

bool PropConfig::readValue(const char* section, const char* key, uint32_t* value)
{
	return readValue(section, key, value, TypeUnsigned32);
}

bool PropConfig::readValue(const char* section, const char* key, char* value, uint32_t* len)
{
	return readValue(section, key, value, TypeString, len);
}

bool PropConfig::readValue(const char* section, const char* key, float* value)
{
	return readValue(section, key, value, TypeFloat);
}

bool PropConfig::readValue(const char* section, const char* key, bool* value)
{
	return readValue(section, key, value, TypeBool);
}

bool PropConfig::readArray(const char* section, const char* key, int8_t* values, uint8_t* count)
{
	return readArray(section, key, values, count, TypeSigned8);
}

bool PropConfig::readArray(const char* section, const char* key, uint8_t* values, uint8_t* count)
{
	return readArray(section, key, values, count, TypeUnsigned8);
}

bool PropConfig::readArray(const char* section, const char* key, int16_t* values, uint8_t* count)
{
	return readArray(section, key, values, count, TypeSigned16);
}

bool PropConfig::readArray(const char* section, const char* key, uint16_t* values, uint8_t* count)
{
	return readArray(section, key, values, count, TypeUnsigned16);
}

bool PropConfig::readArray(const char* section, const char* key, int32_t* values, uint8_t* count)
{
	return readArray(section, key, values, count, TypeSigned32);
}

bool PropConfig::readArray(const char* section, const char* key, uint32_t* values, uint8_t* count)
{
	return readArray(section, key, values, count, TypeUnsigned32);
}

bool PropConfig::readArray(const char* section, const char* key, float* values, uint8_t* count)
{
	return readArray(section, key, values, count, TypeFloat);
}

bool PropConfig::writeValue(const char* section, const char* key, int8_t value)
{
	return writeValues(section, key, &value, 1, TypeSigned8);
}

bool PropConfig::writeValue(const char* section, const char* key, uint8_t value)
{
	return writeValues(section, key, &value, 1, TypeUnsigned8);
}

bool PropConfig::writeValue(const char* section, const char* key, int16_t value)
{
	return writeValues(section, key, &value, 1, TypeSigned16);
}

bool PropConfig::writeValue(const char* section, const char* key, uint16_t value)
{
	return writeValues(section, key, &value, 1, TypeUnsigned16);
}

bool PropConfig::writeValue(const char* section, const char* key, int value)
{
	return writeValue(section, key, (int32_t) value);
}

bool PropConfig::writeValue(const char* section, const char* key, int32_t value)
{
	return writeValues(section, key, &value, 1, TypeSigned32);
}

bool PropConfig::writeValue(const char* section, const char* key, uint32_t value)
{
	return writeValues(section, key, &value, 1, TypeUnsigned32);
}

bool PropConfig::writeValue(const char* section, const char* key, const char* value)
{
	return writeValues(section, key, &value, 1, TypeString);
}

bool PropConfig::writeValue(const char* section, const char* key, float value)
{
	return writeValues(section, key, &value, 1, TypeFloat);
}

bool PropConfig::writeValue(const char* section, const char* key, bool value)
{
	return writeValues(section, key, &value, 1, TypeBool);
}

bool PropConfig::writeArray(const char* section, const char* key, int8_t* values, uint8_t count)
{
	return writeValues(section, key, values, count, TypeSigned8);
}

bool PropConfig::writeArray(const char* section, const char* key, uint8_t* values, uint8_t count)
{
	return writeValues(section, key, values, count, TypeUnsigned8);
}

bool PropConfig::writeArray(const char* section, const char* key, int16_t* values, uint8_t count)
{
	return writeValues(section, key, values, count, TypeSigned16);
}

bool PropConfig::writeArray(const char* section, const char* key, uint16_t* values, uint8_t count)
{
	return writeValues(section, key, values, count, TypeUnsigned16);
}

bool PropConfig::writeArray(const char* section, const char* key, int32_t* values, uint8_t count)
{
	return writeValues(section, key, values, count, TypeSigned32);
}

bool PropConfig:: writeArray(const char* section, const char* key, uint32_t* values, uint8_t count)
{
	return writeValues(section, key, values, count, TypeUnsigned32);
}

bool PropConfig::writeArray(const char* section, const char* key, float* values, uint8_t count)
{
	return writeValues(section, key, values, count, TypeFloat);
}

bool PropConfig::readValue(uint32_t token, int8_t* value)
{
	return readValue(token, value, TypeSigned8);
}

bool PropConfig::readValue(uint32_t token, uint8_t* value)
{
	return readValue(token, value, TypeUnsigned8);
}

bool PropConfig::readValue(uint32_t token, int16_t* value)
{
	return readValue(token, value, TypeSigned16);
}

bool PropConfig::readValue(uint32_t token, uint16_t* value)
{
	return readValue(token, value, TypeUnsigned16);
}

bool PropConfig::readValue(uint32_t token, int32_t* value)
{
	return readValue(token, value, TypeSigned32);
}

bool PropConfig::readValue(uint32_t token, uint32_t* value)
{
	return readValue(token, value, TypeUnsigned32);
}

bool PropConfig::readValue(uint32_t token, char* value, uint32_t* len)
{
	return readValue(token, value, TypeString, len);
}

bool PropConfig::readValue(uint32_t token, float* value)
{
	return readValue(token, value, TypeFloat);
}

bool PropConfig::readValue(uint32_t token, bool* value)
{
	return readValue(token, value, TypeBool);
}

bool PropConfig::readArray(uint32_t token, int8_t* values, uint8_t* count)
{
	return readArray(token, values, count, TypeSigned8);
}

bool PropConfig::readArray(uint32_t token, uint8_t* values, uint8_t* count)
{
	return readArray(token, values, count, TypeUnsigned8);
}

bool PropConfig::readArray(uint32_t token, int16_t* values, uint8_t* count)
{
	return readArray(token, values, count, TypeSigned16);
}

bool PropConfig::readArray(uint32_t token, uint16_t* values, uint8_t* count)
{
	return readArray(token, values, count, TypeUnsigned16);
}

bool PropConfig::readArray(uint32_t token, int32_t* values, uint8_t* count)
{
	return readArray(token, values, count, TypeSigned32);
}

bool PropConfig::readArray(uint32_t token, uint32_t* values, uint8_t* count)
{
	return readArray(token, values, count, TypeUnsigned32);
}

bool PropConfig::readArray(uint32_t token, float* values, uint8_t* count)
{
	return readArray(token, values, count, TypeFloat);
}
