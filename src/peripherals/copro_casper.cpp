/* Casper 68000 second processor module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "copro_casper.h"
#include "../devices/rom.h"
#include "../devices/ram.h"
#include "../beebmem.h"				// Included for RomPath variable
#include "../main.h"					// Included for WriteLog()

//bool Enable_Casper68k	= false;
bool mc68kTube_Casper	= false;



/* Constructor / Deconstructor */

copro_casper::copro_casper(void) {
	this->DEBUG		= false;
	this->RAM_SIZE	= 0x00020000;	// 128 KBytes of ram memory
	this->ROM_SIZE	= 0x00004000;	// 16 KBytes of rom memory
	this->RAM_ADDR	= 0x00020000;	// RAM is at $00020000
	this->ROM_ADDR	= 0x00000000;	// ROM is at $00000000
//	this->ramMemory	= (unsigned char *)malloc(this->RAM_SIZE);
//	this->romMemory = (unsigned char *)malloc(this->ROM_SIZE);
	strcpy (this->biosFile, (char *)"BeebFile/Casper68k.rom");

	this->ram					= new rammemory(this->RAM_SIZE);
	this->rom					= new rommemory(this->biosFile);

	this->host_via				= new r6522;
	this->parasite_via			= new r6522;
	this->cpu					= new mc68k(MC68000, NOFPU, NOMMU);
	this->cpu->Architecture		= CASPER;

	this->host_via->DEBUG		= false;
	this->parasite_via->DEBUG	= false;
	this->cpu->DEBUG			= false;
}

copro_casper::~copro_casper(void) {
//	free(this->romMemory);
//	free(this->ramMemory);
	delete this->cpu;
	delete this->parasite_via;
	delete this->host_via;
}

/* Reset all hardware components of the second processor */

void copro_casper::Reset(void) {
//	this->InitMemory();				// Load firmware ROM and clear RAM
	this->host_via->Reset();
	this->parasite_via->Reset();
	this->cpu->Reset();
//	this->parasite_via->Debug = TRUE;
}

/* Emulate all hardware components of the second processor */

void copro_casper::Exec(int Cycles) {
	// First emulate the Casper hardware
	this->host_via->setPA(this->parasite_via->pb);
	this->parasite_via->setPA(this->host_via->pb);
	this->host_via->setCA1(this->parasite_via->cb2);
	this->host_via->setCB1(this->parasite_via->ca2);
	this->parasite_via->setCA1(this->host_via->cb2);
	this->parasite_via->setCB1(this->host_via->ca2);

	// Then emulate the chips
	this->host_via->Exec(Cycles);
	this->parasite_via->Exec(Cycles);

	// And finally emulate the cpu
	this->cpu->Exec(Cycles);
}
/*
void copro_casper::InitMemory(void) {
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
			WriteLog("copro_casper::InitMemory - Firmware %s loaded\n", path);
	} else
		if (this->DEBUG == true)
			WriteLog("copro_casper::InitMemory - Error: ROM file %s not found!\n", path);

	memset(this->ramMemory, 0, this->RAM_SIZE);	// Clear RAM
}
*/