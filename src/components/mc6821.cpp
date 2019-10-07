/* Motorola MC6821 PIA (Pehiperal Interface Adapter) module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include <stdio.h>					// Included for FILE *
#include <string.h>					// Included for strcpy, strcat

#include "../main.h"				// Included for WriteLog()
#include "mc6821.h"



/* Constructor / Deconstructor */

mc6821::mc6821(void) {
	this->DEBUG = false;
}

mc6821::~mc6821(void) {
}



/* Main code */

void mc6821::Reset(void) {
	this->pa		= 0x00;
	this->pb		= 0x00;
	this->ca1		= false;	// CA1 is input only
	this->ca2		= false;	// CA2 is input or output
	this->cb1		= false;	// CB1 is input only
	this->cb2		= false;	// CB2 is input or output
	this->irqa		= true;		// IRQ lines are active low
	this->irqb		= true;

	this->data_a	= 0;
	this->data_b	= 0;
	this->ddra		= 0;
	this->ddrb		= 0;
	this->ctrl_a	= 0;
	this->ctrl_b	= 0;
}

void mc6821::Exec(int Cycles) {
	// The mc6821 chip has no clock input, so there are no Exec(Cycles) to perform
	if (this->DEBUG)
		WriteLog("mc6821::Exec\n");
}

unsigned char mc6821::ReadRegister(unsigned int Register) {
	unsigned char	Value;

	switch (Register) {
		case 0x00 :		// PRA or DDRA
			if (this->ctrl_a & 0x04) {
				Value = this->data_a;
				this->ctrl_a &= 0x3F;	// Clear IRQA1 and IRQA2 bits in the control register
			}
			else
				Value = this->ddra;
			break;

		case 0x01 :		// CRA
			Value = this->ctrl_a;
			break;

		case 0x02 :		// PRB or DDRB
			if (this->ctrl_b & 0x04) {
				Value = this->data_b;
				this->ctrl_b &= ~0x3F;	// Clear IRQB1 and IRQB2 bits in the control register
			}
			else
				Value = this->ddrb;
			break;

		case 0x03 :		// CRB
			Value = this->ctrl_b;
			break;
	}
	this->updateIRQ();

	if (this->DEBUG)
		WriteLog("mc6821::ReadRegister Register=%01X Value=%02X\n", Register, Value);

	return(Value);
}

void mc6821::WriteRegister(unsigned int Register, unsigned char Value) {
	if (this->DEBUG)
		WriteLog("mc6821::WriteRegister Register=%01X Value=%02X\n", Register, Value);

	switch (Register) {
		case 0x00 :		// PRA or DDRA
			if (this->ctrl_a & 0x04) {
				this->data_a &= ~this->ddra;			// Mask out input bits
				this->data_a |= (Value & this->ddra);	// Only set output bits
				this->pa = this->data_a;				// Set PA port according to the DATA_A register
			} else
				this->ddra = Value;
			break;

		case 0x01 :		// CRA
			this->ctrl_a = (Value & 0x3F);
			if ((Value & 0x30) == 0x30)
				if (Value & 0x08)
					this->ca2 = true;
				else
					this->ca2 = false;
			break;

		case 0x02 :		// PRB or DDRB
			if (this->ctrl_b & 0x04) {
				this->data_b &= ~this->ddrb;			// Mask out input bits
				this->data_b |= (Value & this->ddrb);	// Only set output bits
				this->pa = this->data_a;				// Set PB port according to the DATA_B register
			} else
				this->ddrb = (Value & 0x3F);
			break;

		case 0x03 :		// CRB
			this->ctrl_b = Value;
			if ((Value & 0x30) == 0x30)
				if (Value & 0x08)
					this->cb2 = true;
				else
					this->cb2 = false;
			break;
	}
	this->updateIRQ();
}

void mc6821::setPA(unsigned char Value) {
	this->data_a &= this->ddra;
	this->data_a |= (Value & ~this->ddra);
}

void mc6821::setPB(unsigned char Value) {
}

void mc6821::setCA1(bool value) {
	if (((value != this->ca1) && (this->ctrl_a & 0x01) && (this->ctrl_a & 0x02) && (value == 1)) || ((value != this->ca1) && (this->ctrl_a & 0x01) && !(this->ctrl_a & 0x02) && (value == 0))) {
		if (this->DEBUG)
			WriteLog("mc6821::setCA1 current=%01X new=%01X\n", this->ca1, value);
		this->ctrl_a |= 0x80;
		if ((this->ctrl_a & 0x28) == 0x20)
			this->ca2 = true;	// Read strobe with CA1 restore
	}
	this->ca1 = value;
}

void mc6821::setCA2(bool value) {
	if ((this->ctrl_a & 0x20) != 0x20) {	// We can only set CA2 if it's selected as an input
		if (((value != this->ca2) && (this->ctrl_a & 0x08) && (this->ctrl_a & 0x10) && (value == 1)) || ((value != this->ca2) && (this->ctrl_a & 0x08) && !(this->ctrl_a & 0x10) && (value == 0))) {
			if (this->DEBUG)
				WriteLog("mc6821::setCA2 current=%01X new=%01X\n", this->ca2, value);
			this->ctrl_a |= 0x40;
		}
		this->ca2 = value;
	}
}

void mc6821::setCB1(bool value) {
	if (((value != this->cb1) && (this->ctrl_b & 0x01) && (this->ctrl_b & 0x02) && (value == 1)) || ((value != this->cb1) && (this->ctrl_b & 0x01) && !(this->ctrl_b & 0x02) && (value == 0))) {
		if (this->DEBUG)
			WriteLog("mc6821::setCB1 current=%01X new=%01X\n", this->cb1, value);
		this->ctrl_b |= 0x80;
		if ((this->ctrl_b & 0x28) == 0x20)
			this->cb2 = true;	// Read strobe with CB1 restore
	}
	this->cb1 = value;
}

void mc6821::setCB2(bool value) {
	if ((this->ctrl_a & 0x20) != 0x20) {	// We can only set CA2 if it's selected as an input
		if (((value != this->cb2) && (this->ctrl_b & 0x08) && (this->ctrl_b & 0x10) && (value == 1)) || ((value != this->cb2) && (this->ctrl_b & 0x08) && !(this->ctrl_b & 0x10) && (value == 0))) {
			if (this->DEBUG)
				WriteLog("mc6821::setCB2 current=%01X new=%01X\n", this->cb2, value);
			this->ctrl_b |= 0x40;
		}
		this->cb2 = value;
	}
}

void mc6821::updateIRQ(void) {
	if ((this->ctrl_a & 0x80) || (this->ctrl_b & 0x80))
		this->irqa = true;
	else
		this->irqa = false;

	if ((this->ctrl_a & 0x40) || (this->ctrl_b & 0x40))
		this->irqb = true;
	else
		this->irqb = false;
}
