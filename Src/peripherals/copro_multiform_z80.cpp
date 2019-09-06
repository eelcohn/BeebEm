/* Technomatic Multiform Z80 second processor module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "copro_multiform_z80.h"
#include "beebmem.h"				// Included for RomPath variable
#include "main.h"					// Included for WriteLog()

//bool Enable_PEDLZ80	= FALSE;
bool CoProTechnomaticZ80Enable	= FALSE;



/* Constructor / Deconstructor */

copro_multiform_z80::copro_multiform_z80(void) {
	this->DEBUG					= FALSE;
	this->RAM_SIZE				= 0x00010000;	// 64 KBytes of ram memory
	this->ROM_SIZE				= 0x00002000;	// 8 KBytes of rom memory
	this->RAM_ADDR				= 0x00000000;	// RAM is at $0000
	this->ROM_ADDR				= 0x00000000;	// ROM is at $0000
	this->ramMemory = (unsigned char *)malloc(this->RAM_SIZE);
	this->romMemory = (unsigned char *)malloc(this->ROM_SIZE);
	strcpy (this->biosFile, (char *)"BeebFile/OSMZ80v137.rom");

	this->cpu						= new z80(Z80);
	this->cpu->clockspeed			= 4000000;		// Running at 4MHz
	this->cpu->DEBUG				= FALSE;

	this->host_register1			= new ls374();
	this->host_register1->DEBUG		= FALSE;

	this->host_register2			= new ls374();
	this->host_register2->DEBUG		= FALSE;

	this->parasite_register1		= new ls374();
	this->parasite_register1->DEBUG	= TRUE;

	this->parasite_register2		= new ls374();
	this->parasite_register2->DEBUG	= TRUE;
}

copro_multiform_z80::~copro_multiform_z80(void) {
	free(this->romMemory);
	free(this->ramMemory);
	delete this->parasite_register2;
	delete this->parasite_register1;
	delete this->host_register2;
	delete this->host_register1;
	delete this->cpu;
}

/* Reset all hardware components of the second processor */

void copro_multiform_z80::Reset(void) {
	this->InitMemory();				// Load firmware ROM and clear RAM
	this->host_register1->Reset();
	this->host_register2->Reset();
	this->parasite_register1->Reset();
	this->parasite_register2->Reset();
	this->cpu->Reset();
}

/* Emulate all hardware components of the second processor */

void copro_multiform_z80::Exec(int Cycles) {
	// Emulate the Technomatic Z80 hardware

	// Then emulate the chips

	// And finally emulate the cpu
	this->cpu->Exec(Cycles);
	if (this->DEBUG == TRUE)
		WriteLog("copro_multiform_z80::Exec\n");
}

void copro_multiform_z80::InitMemory(void) {
	FILE *fp;
	char path[256];

	// load ROM into memory
	strcpy(path, RomPath);
	strcat(path, this->biosFile);

	fp = fopen(path, "rb");

	if( fp != NULL ) {
		fread(this->romMemory, this->ROM_SIZE, 1, fp);	// 32 KBytes of rom memory
		fclose(fp);
		if (this->DEBUG == TRUE)
			WriteLog("copro_multiform_z80::InitMemory - Firmware %s loaded\n", path);
	} else
		if (this->DEBUG == TRUE)
			WriteLog("copro_multiform_z80::InitMemory - Error: ROM file %s not found!\n", path);

	memset(this->ramMemory, 0, this->RAM_SIZE);	// Clear RAM
}
