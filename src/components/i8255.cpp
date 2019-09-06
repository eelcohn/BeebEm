/* Intel 8255 PPI module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "../main.h"				// Included for WriteLog()
#include "i8255.h"

/* Constructor / Deconstructor */

i8255::i8255(void) {
}

i8255::~i8255(void) {
}

void i8255::Reset(void) {
	this->port_a	= 0x00;
	this->port_b	= 0x00;
	this->port_c	= 0x00;
	this->control	= 0x00;

	this->DEBUG		= FALSE;
}

void i8255::Exec(int Cycles) {
	// The i8255 chip actually doesn't have a clock input, so the Exec() function shouldn't do anything
	if (this->DEBUG)
		WriteLog("i8255::Exec\n");
}

unsigned char i8255::ReadRegister(unsigned char Register) {
	unsigned char	Value;

	switch (Register) {
		case 0x00 :	// Port A
			Value = this->pa;
			break;

		case 0x01 :	// Port B
			Value = this->pb;
			break;

		case 0x02 :	// Port C
			Value = this->pc;
			break;
	}

	if (this->DEBUG)
		WriteLog("i8255::ReadRegister register=0x%02X, value=%02X\n", Register, Value);

	return (Value);
}

void i8255::WriteRegister(unsigned char Register, unsigned char Value) {
	if (this->DEBUG)
		WriteLog("i8255::WriteRegister register=0x%02X, value=%02X\n", Register, Value);

	switch (Register) {
		case 0x00 :	// Port A
			this->port_a = Value;
			this->pa = Value;
			break;

		case 0x01 :	// Port B
			this->port_b = Value;
			this->pb = Value;
			break;

		case 0x02 :	// Port C
			this->port_c = Value;
			this->pc = Value;
			break;

		case 0x03 :	// Control register
			this->control = Value;
			if (Value & 0x80)		// BSR mode (bit set/reset mode)
				if (Value & 0x01)	// Bit set
					this->pc |= (0x01 << ((Value & 0x0E) >> 1));
				else				// Bit reset
					this->pc &= (0x01 << ((Value & 0x0E) >> 1));
  			break;
	}
}

void i8255::setPA(unsigned char value) {
	this->pa = value;
}

void i8255::setPB(unsigned char value) {
	this->pb = value;
}

void i8255::setPC(unsigned char value) {
	this->pc = value;
}
