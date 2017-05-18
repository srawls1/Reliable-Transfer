// Spencer Rawls
// Checksum.h

#pragma once
#include "stdafx.h"

class Checksum
{
	DWORD* crc_table;
public:
	Checksum();
	~Checksum();
	DWORD CRC32(char* buf, size_t len);
};