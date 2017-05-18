// Spencer Rawls
// Checksum.cpp

#include "stdafx.h"
#include "Checksum.h"

Checksum::Checksum()
{
	crc_table = new DWORD[256];

	for (DWORD i = 0; i < 256; i++)
	{
		DWORD c = i;
		for (int j = 0; j < 8; j++) {
			c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
		}
		crc_table[i] = c;
	}
}

Checksum::~Checksum()
{
	delete[] crc_table;
}

DWORD Checksum::CRC32(char* buf, size_t len)
{
	DWORD c = 0xFFFFFFFF;
	for (size_t i = 0; i < len; i++)
		c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFF;
}