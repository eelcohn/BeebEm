/* 74LS374 module for the BeebEm emulator
  Written by Eelco Huininga 2016-2017
*/

#include "../main.h"				// Included for WriteLog()
#include "ls374.h"

/* Constructor / Deconstructor */

ls374::ls374(void) {
}

ls374::~ls374(void) {
}

void ls374::Reset(void) {
	this->reg	= 0x00;

//	this->DEBUG		= false;
}

void ls374::Exec(int Cycles) {
	if (this->DEBUG)
		WriteLog("ls374::Exec\n");
}

unsigned char ls374::ReadRegister(void) {
	if (this->DEBUG)
		WriteLog("ls374::ReadRegister value=%02X\n", this->reg);

	return (this->reg);
}

void ls374::WriteRegister(unsigned char Value) {
	if (this->DEBUG)
		WriteLog("ls374::WriteRegister value=%02X\n", Value);

	this->reg = Value;
}
