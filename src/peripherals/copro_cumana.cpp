/* Cumana 68000 second processor module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "copro_cumana.h"
#include "../beebmem.h"				// Included for RomPath variable
#include "../main.h"					// Included for WriteLog()

//bool Enable_Cumana68k	= false;
bool mc68kTube_Cumana	= false;



/* Constructor / Deconstructor */

copro_cumana::copro_cumana(void) {
	this->DEBUG				= false;
	this->RAM_SIZE			= 0x00020000;	// 128 KBytes of ram memory???
	this->ROM_SIZE			= 0x00004000;	// 16 KBytes of rom memory
	this->RAM_ADDR			= 0x00080000;	// RAM is at $00080000
	this->ROM_ADDR			= 0x00008000;	// ROM is at $00008000
	this->ramMemory			= (unsigned char *)malloc(this->RAM_SIZE);

	this->sasipia			= new mc6821();
	this->rtcpia			= new mc6821();
	this->fdc				= new wd177x(WD2797);
	this->rtc				= new mc146818();
	this->cpu				= new mc68k(MC68008L, NOFPU, NOMMU);
	this->cpu->Architecture = CUMANA;

	this->sasipia->DEBUG	= false;
	this->rtcpia->DEBUG		= false;
	this->fdc->DEBUG		= false;
	this->rtc->DEBUG		= false;
	this->cpu->DEBUG		= false;
}

copro_cumana::~copro_cumana(void) {
	free(this->ramMemory);
	delete this->cpu;
	delete this->rtc;
	delete this->fdc;
	delete this->rtcpia;
	delete this->sasipia;
}

/* Reset all hardware components of the second processor */

void copro_cumana::Reset(void) {
	this->InitMemory();				// Load firmware ROM and clear RAM
	this->sasipia->Reset();
	this->sasipia->cb1 = true;		// CB1 of IC3 is tied to +5V
	this->rtcpia->Reset();
	this->rtcpia->cb2 = true;		// Pull up resistor R12 makes CB2 high when it's configured as an input
	this->fdc->Reset();
	this->rtc->Reset();
	this->cpu->Reset();
	this->cpu->cpu.stop = true;
}

/* Emulate all hardware components of the second processor */

void copro_cumana::Exec(int Cycles) {
	// Emulate the Cumana hardware (lots of it since it's hardware is complex...)
	if (this->cpu->cpu.stop != this->rtcpia->cb2) {	// CB2 has changed, so the user has issued the *OS9 command
		this->cpu->cpu.stop = this->rtcpia->cb2;
		this->cpu->cpu.ssp	= 0x00081000; //this->cpu->readLong(0x00008200);
		this->cpu->cpu.pc	= 0x00008662; //this->cpu->readLong(0x00008204);
	}

	this->rtcpia->ca1 = this->rtc->irq;
	this->rtcpia->cb1 = this->fdc->intrq;
	if ((this->rtcpia->pb & 0x08) == 0x08)		// FDC Double density enable
		this->fdc->dden = true;
	else
		this->fdc->dden = false;

	if ((this->rtcpia->pb & 0x10) == 0x10)		// FDC Enable pre-compensation
		this->fdc->enp = true;
	else
		this->fdc->enp = false;

	if ((this->rtcpia->pb & 0x20) == 0x20)		// RTC Address strobe
		this->rtc->as = true;
	else
		this->rtc->as = false;

	if ((this->rtcpia->pb & 0x40) == 0x40)		// RTC Data strobe
		this->rtc->ds = true;
	else
		this->rtc->ds = false;

	if ((this->rtcpia->pa & 0x80) == 0x80) {
		this->rtc->rw = true;					// RTC R/W = Read
		this->rtcpia->setPA(this->rtc->ad);
	} else {
		this->rtc->rw = false;					// RTC R/W = Write
		this->rtc->setAD(this->rtcpia->pa);
	}

	if (this->fdc->drq)
		this->sasipia->setPB((this->sasipia->pb & ~0x40) | 0x40);	// Set SASI PIA PB6
	else
		this->sasipia->setPB(this->sasipia->pb & ~0x40);			// Clear SASI PIA PB6

	// Then emulate the chips
	this->sasipia->Exec(Cycles);
	this->rtcpia->Exec(Cycles);
	this->fdc->Exec(Cycles);
	this->rtc->Exec(Cycles);

	// And finally emulate the cpu
	this->cpu->Exec(Cycles);
}

void copro_cumana::InitMemory(void) {
	memset(this->ramMemory, 0, this->RAM_SIZE);	// Clear RAM
}