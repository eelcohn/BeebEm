/* CiscOS 68k second processor object module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "copro_ciscos.h"
#include "../beebmem.h"				// Included for RomPath variable
#include "../main.h"					// Included for WriteLog()
#include "../tube.h"					// Included for... well, duhh... access to TUBE ULA

//bool Enable_CiscOS68k	= false;
bool mc68kTube_CiscOS	= false;



/* Constructor / Deconstructor */

copro_ciscos::copro_ciscos(void) {
	this->DEBUG				= false;
	this->BOOTFLAG			= true;			// ==TRUE: ROM is at 00000000, ==FALSE: RAM is at 00000000-0037FFFF, ROM is at 00380000-003FFFFF
	this->RAM_SIZE			= 0x00200000;	// 2 MBytes of ram memory
	this->ROM_SIZE			= 0x00008000;	// 32 KBytes of rom memory
	this->RAM_ADDR			= 0x00000000;	// RAM is at $00000000
	this->ROM_ADDR			= 0xFFFF0000;
	this->TUBE_ULA_ADDR		= 0xFFFE0000;
	this->ramMemory			= (unsigned char *)malloc(this->RAM_SIZE);
	this->romMemory			= (unsigned char *)malloc(this->ROM_SIZE);
	strcpy (this->biosFile, (char *)"BeebFile/CiscOS.rom");

	this->tube_ula			= new ula9c018;
	this->cpu				= new mc68k(MC68008P, NOFPU, NOMMU);
	this->cpu->Architecture = CISCOS;

	this->tube_ula->DEBUG	= false;
	this->cpu->DEBUG		= false;
}

copro_ciscos::~copro_ciscos(void) {
	free(this->romMemory);
	free(this->ramMemory);
	delete this->cpu;
	delete this->tube_ula;
}

/* Reset all hardware components of the second processor */

void copro_ciscos::Reset(void) {
	this->InitMemory();
	this->BOOTFLAG			= true;		// Only ROM is available during boot

	this->tube_ula->Reset();
	this->tube_ula->DEBUG	= false;

	R1Status = 0;						// Old style TUBE ULA emulator, see tube.cpp. Can be removed when the new ula9c018.cpp module is finished
	ResetTube();

	this->cpu->Reset();
	this->cpu->DEBUG		= false;
}

/* Emulate all hardware components of the second processor */

void copro_ciscos::Exec(int Cycles) {
	// Emulate the CiscOS hardware
	if ((TubeintStatus & (1<<R1)) || (TubeintStatus & (1<<R4)))
//	if (!this->tube_ula->parasite_irq)
		this->cpu->interruptLevel = 2;
		
	if ((TubeintStatus == 0) && (this->cpu->pendingException == INTERRUPT2)) {
//	if ((this->tube_ula->parasite_irq) && (this->cpu->pendingException == INTERRUPT2)) {
		this->cpu->interruptLevel = 0;
		this->cpu->currentInterrupt = -1;
//		this->cpu->DEBUG = false;
	}

	if (TubeNMIStatus)
//	if (!this->tube_ula->parasite_int)
		this->cpu->interruptLevel = 5;

	if ((!TubeNMIStatus) && (this->cpu->pendingException == INTERRUPT5)) {
//	if ((this->tube_ula->parasite_int) && (this->cpu->pendingException == INTERRUPT5)) {
		this->cpu->interruptLevel = 0;
		this->cpu->currentInterrupt = -1;
//		this->cpu->DEBUG = false;
	}

	// Then emulate the chips
	this->tube_ula->Exec(Cycles);

	// And finally emulate the cpu
	this->cpu->Exec(Cycles);					
}

void copro_ciscos::InitMemory(void) {
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
			WriteLog("copro_ciscos::InitMemory - Firmware %s loaded\n", path);
	} else
		if (this->DEBUG == true)
			WriteLog("copro_ciscos::InitMemory - Error: ROM file %s not found!\n", path);

	memset(this->ramMemory, 0, this->RAM_SIZE);	// Clear RAM
}
