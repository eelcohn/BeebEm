/* Generic ROM module for the BeebEm emulator
  Written by Eelco Huininga 2016-2017
*/

#include "../main.h"				// Included for WriteLog()
#include "rom.h"

/* Constructor / Deconstructor */

rommemory::rommemory(const char *bios_file) {
	FILE			*fp;
	unsigned int	filesize;

	fp = fopen(bios_file, "rb");

	if( fp != NULL ) {
		fseek(fp, 0, SEEK_END);
		filesize = ftell(fp);
		this->memory = (unsigned char *)malloc(filesize);
		if (this->memory == NULL)
			WriteLog("rom::rom - Could not allocate %i bytes as ROM memory for %s.\n", filesize, bios_file);
		else {
			fseek(fp, 0, SEEK_SET);
			fread(this->memory, filesize, 1, fp);
			fclose(fp);
			if (this->DEBUG == true)
				WriteLog("rom::rom - Firmware %s loaded\n", bios_file);
		}
	} else
		if (this->DEBUG == true)
			WriteLog("rom::rom - Error: ROM file %s not found!\n", bios_file);
}

rommemory::~rommemory(void) {
	free(this->memory);
}

void rommemory::Reset(void) {
	this->DEBUG = false;
}
