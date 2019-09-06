/* 74LS174 module for the BeebEm emulator
  Written by Eelco Huininga 2016-2017
*/

#include "../main.h"				// Included for WriteLog()
#include "ls174.h"

/* Constructor / Deconstructor */

ls174::ls174(void) {
}

ls174::~ls174(void) {
}

void ls174::Reset(void) {
	this->reg	= 0x00;

//	this->DEBUG		= false;
}

void ls174::Exec(int Cycles) {
	if (this->DEBUG)
		WriteLog("ls174::Exec\n");
}

unsigned char ls174::ReadRegister(void) {
	if (this->DEBUG)
		WriteLog("ls174::ReadRegister value=%02X\n", this->reg);

	return (this->reg);
}

void ls174::WriteRegister(unsigned char Value) {
	if (this->DEBUG)
		WriteLog("ls174::WriteRegister value=%02X\n", Value);

	this->reg = Value & 0x3f;
}
