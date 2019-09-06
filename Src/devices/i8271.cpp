/* Intel 8271 FDC (Floppy Disc Controller) object module for the BeebEm emulator
  Written by Eelco Huininga 2017
*/

#include "../main.h"				// Included for WriteLog()
#include "i8271.h"
#include "../disc8271.h"			// Temporary link to old 8271 code

/* Constructor / Deconstructor */

i8271::i8271(void) {
}

i8271::~i8271(void) {
}

void i8271::Reset(void) {
	this->drq					= false;	// DRQ is active high output
	this->dack					= NULL;		// DACK is input
	this->intr					= false;	// INT is active high output
	this->rst					= NULL;		// RESET is input

	this->DEBUG					= false;
}

void i8271::Exec(int Cycles) {
}

unsigned char i8271::ReadRegister(unsigned char Register) {
	unsigned char	Value;

	Value = Disc8271_read(Register & 0x7);

	if (this->DEBUG)
		WriteLog("i8271::ReadRegister register=%02X, value=%02X\n", Register, Value);

	return(Value);
}

void i8271::WriteRegister(unsigned char Register, unsigned char Value) {

	if (this->DEBUG)
		WriteLog("i8271::WriteRegister register=%02X, value=%02X\n", Register, Value);

	Disc8271_write((Register & 7),Value);
}
