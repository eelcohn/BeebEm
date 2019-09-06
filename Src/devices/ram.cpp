/* Generic RAM module for the BeebEm emulator
  Written by Eelco Huininga 2016-2017
*/

#include "../main.h"				// Included for WriteLog()
#include "ram.h"

/* Constructor / Deconstructor */

rammemory::rammemory(unsigned long size) {
	this->size = size;

	this->memory = (unsigned char *)malloc(size);
	if (this->memory == NULL)
		WriteLog("ram::ram - Could not allocate %i bytes as RAM memory.\n", size);
}

rammemory::~rammemory(void) {
	free(this->memory);
}

void rammemory::Reset(void) {
	memset(this->memory, 0, this->size);	// Clear RAM
	this->DEBUG = false;
}
