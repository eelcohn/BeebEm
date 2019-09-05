/* Torch Z80 communicator second processor module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "copro_torch_z80.h"
#include "../beebmem.h"				// Included for RomPath variable
#include "../main.h"					// Included for WriteLog()

//bool TorchTube			= false;
bool CoProTorchZ80Active	= false;



/* Constructor / Deconstructor */

copro_torch_z80::copro_torch_z80(void) {
	this->DEBUG				= false;
	this->RAM_SIZE			= 0x00010000;	// 64 KBytes of ram memory
	this->ROM_SIZE			= 0x00002000;	// 8 KBytes of rom memory
	this->RAM_ADDR			= 0x00000000;	// RAM is at $0000
	this->ROM_ADDR			= 0x00000000;	// Fill this in later
	this->ramMemory = (unsigned char *)malloc(this->RAM_SIZE);
	this->romMemory = (unsigned char *)malloc(this->ROM_SIZE);
	strcpy (this->biosFile, (char *)"BeebFile/TorchZ80.rom");

	this->via				= new r6522;
	this->via->DEBUG		= false;

	this->ppi				= new i8255;
	this->ppi->DEBUG		= false;

	this->cpu				= new z80(Z80);
	this->cpu->Architecture	= TORCH_Z80;
	this->cpu->DEBUG		= false;
}

copro_torch_z80::~copro_torch_z80(void) {
	free(this->romMemory);
	free(this->ramMemory);
	delete this->cpu;
	delete this->ppi;
	delete this->via;
}

/* Reset all hardware components of the second processor */

void copro_torch_z80::Reset(void) {
	this->InitMemory();				// Load firmware ROM and clear RAM
	this->via->Reset();
	this->ppi->Reset();
	this->cpu->Reset();
}

/* Emulate all hardware components of the second processor */

void copro_torch_z80::Exec(int Cycles) {
	// Emulate the Torch Z80 hardware
	this->via->setPA(this->ppi->pb);
	this->ppi->setPA(this->via->pb);
	this->via->setPB(this->ppi->pa);
	this->ppi->setPB(this->via->pa);

	// Then emulate the chips
	this->via->Exec(Cycles);
	this->ppi->Exec(Cycles);

	// And finally emulate the cpu
	this->cpu->Exec(Cycles);
}

void copro_torch_z80::InitMemory(void) {
	FILE *fp;
	char path[256];

	// load ROM into memory
	strcpy(path, RomPath);
	strcat(path, this->biosFile);

	fp = fopen(path, "rb");

	if( fp != NULL ) {
		fread(this->romMemory, this->ROM_SIZE, 1, fp);	// 32 KBytes of rom memory
		fclose(fp);
		if (this->DEBUG == true)
			WriteLog("copro_torch_z80::InitMemory - Firmware %s loaded\n", path);
	} else
		if (this->DEBUG == true)
			WriteLog("copro_torch_z80::InitMemory - Error: ROM file %s not found!\n", path);

	memset(this->ramMemory, 0, this->RAM_SIZE);	// Clear RAM
}
