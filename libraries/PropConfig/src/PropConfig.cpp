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
	last_section_offs = 0;
	section_locked = false;

#ifdef CONFIG_USE_PRELOAD
	preload_size = preload_offset = 0;
#endif

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
	return true;
}

/* Finds a section and leaves the cursor at the first line within the section */
FindResult PropConfig::findSection(const char* section)
{
	// If section is the same as the last read section, jump to it
	if (strlen(last_section) == strlen(section) &&
		strncasecmp(last_section, section, strlen(last_section)) == 0)
	{
		if (!setFileRWPointer(last_section_offs))
			return findError;

		return sectionFound;
	}

	// Otherwise scan the entire file. Start where we've left the file pointer.
	uint32_t file_ptr = getFileRWPointer();
	bool roll = false;

	while (true)
	{
		if (!readLine(_line_buffer_rd))
		{
			if (!setFileRWPointer(0))
				break;

			roll = true;
			continue;
		}

		if (roll && getFileRWPointer() >= file_ptr)
			break;

		char *file_sect;
		if (parseLine(_line_buffer_rd, &file_sect, NULL, NULL, lineSection) == lineSection)
		{
			if (strlen(section) == strlen(file_sect))
			{
				if (strncasecmp(file_sect, section, strlen(section)) == 0)
				{
					// Remember the last section name, offset and line
					strncpy(last_section, section, CONFIG_MAX_SECTION_LEN);
					last_section_offs = getFileRWPointer();
					return sectionFound;
				}
			}
		}
	}

	return noSection;
}

bool PropConfig::sectionExists(const char* section)
{
	return findSection(section) == sectionFound;
}

FindResult PropConfig::findKey(const char* section, const char* key, char** key_data)
{
	FindResult res = findSection(section);
	if (res != sectionFound)
		return res;

	while (readLine(_line_buffer_rd))
	{
		char *file_sect, *file_key, *file_data;

		switch (parseLine(_line_buffer_rd, &file_sect, &file_key, &file_data, lineAny))
		{
			case lineAny:
			case lineEmpty:
			case lineComment:
			case lineGarbage:
				continue;

			case lineKey:
			case lineEmptyKey:
				if (strlen(key) == strlen(file_key))
				{
					if (strncasecmp(key, file_key, strlen(key)) == 0)
					{
						if (key_data)
							*key_data = file_data;

						return keyFound;
					}
				}
				continue;

			case lineSection:
				// Found another section while searching for a key, so ...
				return noKey;
		}
	}

	return noKey;
}

char* PropConfig::getKeyData(const char* section, const char* key)
{
	char *data;
	if (findKey(section, key, &data) == keyFound)
		return data;
	
	return NULL;
}

#ifdef CONFIG_USE_PRELOAD
bool PropConfig::preload()
{
	UINT read;

	if (f_read(&_file, &preload_buffer, CONFIG_PRELOAD_SIZE, &read) != FR_OK || read == 0)
		return false;

	preload_size = read;
	preload_offset = 0;
	return true;
}
#endif

bool PropConfig::readLine(char* line)
{
	char c;
	uint32_t idx = 0;

	while (idx < CONFIG_MAX_LINE_LEN)
	{
#ifdef CONFIG_USE_PRELOAD
		if (preload_offset == preload_size)
		{
			if (!preload())
				return false;
		}

		c = preload_buffer[preload_offset++];
#else
		UINT read;
		if (f_read(&_file, &c, 1, &read) != FR_OK || read != 1)
			return false;
#endif
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

void PropConfig::removeWhiteSpace(char* str)
{
	char* dst = str;

	while (*str)
	{
		if (*str != 0x20)
			*(dst++) = *str;

		str++;
	}

	*dst = '\0';
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
	{
		*count = 0;
		return true;
	}

	removeWhiteSpace(data);

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
	written = snprintf(_line_buffer_wr, CONFIG_MAX_LINE_LEN, "%s = ", key);

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
	bool done = false;
	bool in_section = false;

	// Write is not allowed while locked into a section (for reading)
	if (section_locked)
		return false;

	if (!createBackupFile())
		return false;

	bool key_exists = false;
	bool section_exists = false;
	FindResult res = findKey(section, key, NULL);

	if (res == keyFound)
		key_exists = section_exists = true;
	else if (res == sectionFound)
		section_exists = true;
	else if (res == findError)
		return false;

	// Start copying
	if (!setFileRWPointer(0))
		return -1;

	if (!section_exists)
	{
		// Just copy all, and add the section at the end
		while (readLine(_line_buffer_rd))
		{
			if (!writeLineToBackup(_line_buffer_rd))
				return false;
		}

		// End of file reached
		// We have to write the new section and the key
		snprintf(_line_buffer_rd, CONFIG_MAX_LINE_LEN, "[%s]", section);
		if (!writeLineToBackup(_line_buffer_rd) ||
			!writeKeyToBackup(key, values, count, type))
			return false;

	} else {
		// Copy all while searching for the section
		while (readLine(_line_buffer_rd))
		{
			if (done)
			{
				// Write the read line
				if (!writeLineToBackup(_line_buffer_rd))
					return false;

				continue;
			}

			// Make a copy
			strcpy(_line_buffer_wr, _line_buffer_rd);

			// Parse the line
			char *file_sect, *file_key, *file_data;
			LineType line_type;

			line_type = parseLine(_line_buffer_rd, &file_sect, &file_key, &file_data, lineAny);

			// Check if we are inside the section we want to write
			if (line_type == lineSection)
			{
				if (strlen(section) == strlen(file_sect))
				{
					if (strncasecmp(file_sect, section, strlen(section)) == 0)
					{
						in_section = true;

						// If the key doesn't exist, add it here
						if (!key_exists)
						{
							// Write the read line (copy)
							if (!writeLineToBackup(_line_buffer_wr))
								return false;

							// Write the line to be replaced
							if (!writeKeyToBackup(key, values, count, type))
								return false;

							done = true;
							continue;
						}
					}
				} else in_section = false;
			} else if (in_section)
			{
				if (line_type == lineKey || line_type == lineEmptyKey)
				{
					// Found some key inside the section we want
					if (strlen(key) == strlen(file_key))
					{
						if (strncasecmp(file_key, key, strlen(key)) == 0)
						{
							// This is the key/line we want to replace
							if (!writeKeyToBackup(key, values, count, type))
								return false;

							done = true;
							continue;
						}
					}
				}
			}

			// Write the read line (copy)
			if (!writeLineToBackup(_line_buffer_wr))
				return false;
		}
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

			if (!offs)
			{
				*len = 0;
			} else {
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
			}
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

#ifdef CONFIG_USE_PRELOAD
	preload_size = preload_offset = 0;
#endif

	return true;
}

bool PropConfig::startSectionScan(const char* section)
{
	if (section_locked)
		return false;

	if (findSection(section) == sectionFound)
	{
		section_locked = true;
		return true;
	}

	return false;
}

bool PropConfig::startSectionScan(uint32_t file_offset)
{
	if (section_locked)
		return false;

	if (!setFileRWPointer(file_offset))
		return false;

	section_locked = true;
	return true;
}

bool PropConfig::getNextKey(uint32_t* token, char** key, uint32_t* key_len)
{
	if (!section_locked || !token || !key || !key_len)
		return NULL;

	while (readLine(_line_buffer_rd))
	{
		char* section, *file_key, *data;
		switch (parseLine(_line_buffer_rd, &section, &file_key, &data, lineAny))
		{
			case lineKey:
			case lineEmptyKey:
				if (data)
					*token = (uint32_t) data;

				if (key)
					*key = file_key;

				if (key_len)
					*key_len = strlen(file_key);
				return true;

			case lineAny:
			case lineEmpty:
			case lineComment:
			case lineGarbage:
				continue;

			case lineSection:
				// Found another section while searching for a key, so ...
				return false;
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

bool PropConfig::setFileRWPointer(uint32_t pos)
{
	if (f_lseek(&_file, pos) != FR_OK)
		return false;

#ifdef CONFIG_USE_PRELOAD
	if (!preload())
		return false;
#endif

	return true;
}

uint32_t PropConfig::getFileRWPointer()
{
	uint32_t file_ptr = f_tell(&_file);

#ifdef CONFIG_USE_PRELOAD
	if (file_ptr >= CONFIG_PRELOAD_SIZE)
		file_ptr = (file_ptr - preload_size) + preload_offset;
#endif

	return file_ptr;
}

bool PropConfig::mapSections(configFileMapCallback* func, void* param)
{
	uint32_t section_start_offs = 0;
	uint32_t data_start_offs;
	char* section;

	if (!func)
		return false;

	if (!setFileRWPointer(0))
		return false;

	while (true)
	{
		section_start_offs = getFileRWPointer();

		if (!readLine(_line_buffer_rd))
			return false;

		if (parseLine(_line_buffer_rd, &section, NULL, NULL, lineSection) == lineSection)
		{
			data_start_offs = getFileRWPointer();
			if ((func)(section_start_offs, data_start_offs, section, param) == false)
				break;
		}
	}

	return true;
}

LineType PropConfig::parseLine(char* line, char** section, char** key, char** data, LineType looking_for)
{
	// Parse a line. Use looking_for to search for a specific type. For example set it to
	// lineSection when looking for sections and so avoid parsing things we're not
	// interested in.
	char* equal_at = NULL;

	// Check for garbage/whitespace/comment
	while (*line)
	{
		if (*line == '#' || (*line == '/' && *(line+1) == '/'))
			return lineComment;

		if (*line == '=')
			return lineGarbage;

		if (*line == '\r' || *line == '\n')
			return lineEmpty;

		if (*line != 0x20 && *line != 0x09)
			break;

		line++;
	}

	if (!*line)
		return lineGarbage;

	// Check for section
	if (*line == '[')
	{
		if (looking_for != lineSection && looking_for != lineAny)
			return lineGarbage;

		// Section starts
		line++;

		// Scan for garbage and remove whitespace
		while (*line)
		{
			if (*line == '#' || (*line == '/' && *(line+1) == '/') ||
				*line == '\r' || *line == '\n' || *line == ']')
				return lineGarbage;

			if (*line != 0x20 && *line != 0x09)
				break;

			line++;
		}

		if (!*line)
			return lineGarbage;

		// Point section here
		if (section)
			*section = line;

		// Find closure
		while (*line)
		{
			if (*line == '#' || (*line == '/' && *(line+1) == '/') ||
				*line == '\r' || *line == '\n')
				return lineGarbage;

			if (*line == ']')
				break;

			line++;
		}

		if (!*line)
			return lineGarbage;

		// Roll back finding whitespace
		while (*(line-1) == 0x20 || *(line-1) == 0x09)
			line--;

		// Put a null char and return
		*line = 0x00;
		return lineSection;

	} else {
		if (looking_for != lineKey && looking_for != lineAny)
			return lineGarbage;

		// Should be key=data. Point key pointer here
		if (key)
			*key = line;

		// Key starts
		line++;

		// Search for equal
		while (*line)
		{
			if (*line == '#' || (*line == '/' && *(line+1) == '/') ||
				*line == '\r' || *line == '\n' || *line == '[' || *line == ']')
				return lineGarbage;

			if (*line == '=')
				break;

			line++;
		}

		if (!*line)
			return lineGarbage;

		equal_at = line;

		// Roll back finding whitespace
		while (*(line-1) == 0x20 || *(line-1) == 0x09)
			line--;

		// Put a null char
		*line = 0x00;

		line = equal_at + 1;

		// Remove whitespace after equal sign
		while (*line)
		{
			if (*line == '#' || (*line == '/' && *(line+1) == '/'))
				return lineGarbage;

			if (*line != 0x20 && *line != 0x09)
				break;

			line++;
		}

		// Put data pointer here
		if (data)
			*data = line;

		if (!*line)
			return lineEmptyKey;

		// Check for end of line
		while (*line)
			line++;

		// Roll back finding whitespace
		while (*(line-1) == 0x20 || *(line-1) == 0x09)
			line--;

		// Put a null char and return
		*line = 0x00;
		return lineKey;
	}

	return lineGarbage;
}
