/* Rockwell 6522 VIA module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "../main.h"				// Included for WriteLog()
#include "r6522.h"

/* Constructor / Deconstructor */

r6522::r6522(void) {
}

r6522::~r6522(void) {
}

void r6522::Reset(void) {
	this->pa		= 0x00;
	this->pb		= 0x00;
	this->ca1		= TRUE;
	this->ca2		= TRUE;
	this->cb1		= TRUE;
	this->cb2		= TRUE;
	this->res		= NULL;	// Input
	this->irq		= TRUE;

	this->ora		= 0xFF;
	this->ira		= 0xFF;
	this->orb		= 0xFF;
	this->irb		= 0xFF;
	this->ddra		= 0x00;	// All inputs
	this->ddrb		= 0x00;	// All inputs
	this->t1c		= 0xFFFF;
	this->t1l		= 0xFFFF;
	this->t2ll		= 0xFF;
	this->t2c		= 0xFFFF;
	this->sr		= 0x00;
	this->acr		= 0x00;
	this->pcr		= 0x00;
	this->ier		= 0x80;
	this->ifr		= 0x00;

	this->DEBUG		= FALSE;
}

void r6522::Exec(int Cycles) {
	if (this->DEBUG)
		WriteLog("r6522::Exec");

	this->updateInterrupt();		// Update the IFR register
	if ((this->pcr & 0x0C) == 0x08)	// CA2 is output and in pulse mode
		this->ca2 = TRUE;			// No need to check current value of CA2, just set it to true
	if ((this->pcr & 0xE0) == 0xA0)	// CB2 is output and in pulse mode
		this->cb2 = TRUE;			// No need to check current value of CB2, just set it to true

	if (this->t1c != 0) {
		this->t1c--;
		if ((this->t1c == 0) && (this->ier & 0x40))	// Timer 1 time-out
			this->ifr |= 0x40;				// Set 'time-out of timer 1' interrupt flag
	} else {
		if (this->DEBUG)
			WriteLog(" timer1 timed out");
		if (this->acr & 0x40) {				// Timer 1 is in free-run mode
			this->t1c = this->t1l;			// ...so reload counter
			if (this->acr & 0x80)
				this->pb ^= 0x80;			// Toggle PB7 if T1 is in free-run mode
		} else
			if (this->acr & 0x80)
				this->pb |= 0x80;			// Set PB7 if T1 is in one-shot mode
	}

	if ((this->acr & 0x20) != 0x20)			// Timer 2 is in timed interrupt mode
		if (this->t2c != 0) {
			this->t2c--;
			if ((this->t2c == 0) && (this->ier & 0x20))	// Timer 2 time-out
				this->ifr |= 0x20;			// Set 'time-out of timer 2' interrupt flag
		} else {
			if (this->DEBUG)
				WriteLog(" timer2 timed out");
			this->t2c = this->t2ll;			// Reload counter
		}

	if (this->DEBUG)
		WriteLog("\n");
}

unsigned char r6522::ReadRegister(unsigned char Register) {
	unsigned char	Value;

	switch (Register) {
		case 0x00 :	// Input register B
			Value = this->irb;
			this->ifr &= ~0x10;
			if ((this->pcr & 0xC0) == 0x80)	// CB2 is output and in handshake or pulse mode
				this->cb2 = FALSE;
			break;

		case 0x01 :	// Input register A
			Value = this->ira;
			this->ifr &= ~0x02;
			if ((this->pcr & 0x0C) == 0x08)	// CA2 is output and in handshake or pulse mode
				this->ca2 = FALSE;
			break;

		case 0x02 :	// Data direction register B
			Value = this->ddrb;
			break;

		case 0x03 :	// Data direction register A
			Value = this->ddra;
			break;

		case 0x04 :	// Timer 1 Low-Order Latches
			Value = this->t1l & 0x00FF;
			this->ifr &= ~0x40;				// Clear Timer 1 bit in IFR
			break;

		case 0x05 :	// Timer 1 High-Order Counter
			Value = (this->t1c & 0xFF00) >> 8;
			break;

		case 0x06 :	// Timer 1 Low-Order Latches
			Value = this->t1l & 0x00FF;
			this->ifr &= ~0x40;				// Clear Timer 1 bit in IFR
			break;

		case 0x07 :	// Timer 1 High-Order Latches
			Value = (this->t1l & 0xFF00) >> 8;
			break;

		case 0x08 :	// Timer 2 Low-Order Latches
			this->ifr &= ~0x20;				// Clear Timer 2 bit in IFR
			Value = this->t2ll;
			break;

		case 0x09 :	// Timer 1 High-Order Counter
			Value = (this->t1c & 0xFF00) >> 8;
			break;

		case 0x0A :	// Shift Register
			Value = this->sr;
			break;

		case 0x0B :	// Auxiliary Control Register
			Value = this->acr;
			break;

		case 0x0C :	// Pehiperal Control Register
			Value = this->pcr;
			break;

		case 0x0D :	// Interrupt Flag Register
			Value = this->ifr;
			break;

		case 0x0E :	// Interrupt Enable Register
			Value = this->ier | 0x80;
			break;

		case 0x0F :	// Input Register A (No Handshake)
			Value = this->ira;
			break;
	}

	if (this->DEBUG)
		WriteLog("r6522::ReadRegister 0x%02X, value=%02X ca1=%01X ca2=%01X cb1=%01X cb2=%01X pcr=%02X\n", Register, Value, this->ca1, this->ca2, this->cb1, this->cb2, this->pcr);

	return (Value);
}

void r6522::WriteRegister(unsigned char Register, unsigned char Value) {
	if (this->DEBUG)
		WriteLog("r6522::WriteRegister 0x%02X, value=%02X ca1=%01X ca2=%01X cb1=%01X cb2=%01X pcr=%02X\n", Register, Value, this->ca1, this->ca2, this->cb1, this->cb2, this->pcr);

	switch (Register) {
		case 0x00 :	// Output register B
			this->orb = Value;
			this->pb &= ~this->ddrb;		// Clear any output bits
			this->pb |= Value;				// And set them accordingly to the ORB register
			this->ifr &= ~0x10;				// Clear the CB1 flag in the IFR register
			if ((this->pcr & 0xC0) == 0x80)	// Is CB2 output and in handshake or pulse mode?
				this->cb2 = FALSE;			// If so, then generate "Data Taken" on CB2
			break;

		case 0x01 :	// Output register A
			this->ora = Value;
			this->pa &= ~this->ddra;		// Clear any output bits
			this->pa |= Value;				// And set them accordingly to the ORA register
			this->ifr &= ~0x02;				// Clear the CA1 flag in the IFR register
			if ((this->pcr & 0x0C) == 0x08)	// Is CA2 output and in handshake or pulse mode?
				this->ca2 = FALSE;			// If so, then generate "Data Taken" on CA2
			break;

		case 0x02 :	// Data direction register B
			this->ddrb = Value;
			break;

		case 0x03 :	// Data direction register A
			this->ddra = Value;
			break;

		case 0x04 :	// Timer 1 Low-Order Counter
			this->t1c &= 0xFF00;
			this->t1c |= Value;
			break;

		case 0x05 :	// Timer 1 High-Order Counter
			this->t1c &= 0x00FF;
			this->t1c |= (Value << 8);
			this->ifr &= ~0x40;				// Clear Timer 1 bit in IFR
			if (this->acr & 0x80)			// If PB7 toggling enabled, then lower PB7 now
				this->pb &= 0x7F;
			break;

		case 0x06 :	// Timer 1 Low-Order Latches
			this->t1l &= 0xFF00;
			this->t1l |= Value;
			break;

		case 0x07 :	// Timer 1 High-Order Latches
			this->t1l &= 0x00FF;
			this->t1l |= (Value << 8);
			this->ifr &= ~0x40;				// Clear Timer 1 bit in IFR
			break;

		case 0x08 :	// Timer 2 Low-Order Counter
			this->t2c &= 0xFF00;
			this->t2c |= Value;
			break;

		case 0x09 :	// Timer 2 High-Order Counter
			this->t2c &= 0x00FF;
			this->t2c |= (Value << 8);
			this->ifr &= ~0x20;				// Clear Timer 2 bit in IFR
			break;

		case 0x0A :	// Shift Register
			this->sr = Value;
			break;

		case 0x0B :	// Auxiliary Control Register
			this->acr = Value;
			break;

		case 0x0C :	// Pehiperal Control Register
			this->pcr = Value;
			if ((Value & 0x0E) == 0x0C)
				this->ca2 = FALSE;
			if ((Value & 0x0E) == 0x0E)
				this->ca2 = TRUE;
			if ((Value & 0xE0) == 0xC0)
				this->cb2 = FALSE;
			if ((Value & 0xE0) == 0xE0)
				this->cb2 = TRUE;
			break;

		case 0x0D :	// Interrupt Flag Register
			this->ifr = Value;
			break;

		case 0x0E :	// Interrupt Enable Register
			if (Value & 0x80)
				this->ier |= (Value & 0x7F);
			else
				this->ier &= ~(Value & 0x7F);
			break;

		case 0x0F :	// Output Register A (No Handshake)
			this->ora = Value;
			break;
	}
}

void r6522::setPA(unsigned char value) {
	if (this->DEBUG)
		WriteLog("r6522::setPA value=%02X\n", value);

	this->pa &= this->ddra;				// Preserve the output bits
	this->pa |= (value & ~this->ddra);	// ...and set the input bits

	if ((this->acr & 0x01) != 0x01)		// Is input latching disabled?
		this->ira = this->pa;			// If so, also set IRA
}

void r6522::setPB(unsigned char value) {
	if (this->DEBUG)
		WriteLog("r6522::setPB value=%02X\n", value);

	this->pb &= this->ddrb;				// Preserve the output bits
	this->pb |= (value & ~this->ddrb);	// ...and set the input bits

	if ((this->acr & 0x02) != 0x02)		// Is input latching disabled?
		this->irb = this->pb;			// If so, also set IRB
}

void r6522::setCA1(bool value) {
	if (((value != this->ca1) && (this->pcr & 0x01) && (value == 1)) || ((value != this->ca1) && (!(this->pcr & 0x01)) && (value == 0))) {	// ((CA1 positive active edge) || (CA1 negative active edge))
		if (this->DEBUG)
			WriteLog("r6522::setCA1 current=%01X new=%01X, PCR=%02X\n", this->ca1, value, this->pcr);
		this->ira = this->pa;			// Latch PA into IRA
		this->ifr |= 0x02;				// ...and set flag in the IFR
	}
	this->ca1 = value;
}

void r6522::setCA2(bool value) {
	if ((this->pcr & 0x08) == 0x00) {	// CA2 must be in input mode to set its value
		if (((value != this->ca2) && ((this->pcr & 0x0C) == 0x04) && (value == 1)) || ((value != this->ca2) && ((this->pcr & 0x0C) == 0x00) && (value == 0))) {	// ((CA2 positive active edge) || (CA2 negative active edge))
			if (this->DEBUG)
				WriteLog("r6522::setCA2 current=%01X new=%01X, PCR=%02X\n", this->ca2, value, this->pcr);
			this->ifr |= 0x01;
		}
		this->ca2 = value;
	}
}

void r6522::setCB1(bool value) {
	if (((value != this->cb1) && (this->pcr & 0x10) && (value == 1)) || ((value != this->cb1) && (!(this->pcr & 0x01)) && (value == 0))) {	// ((CB1 positive active edge) || (CB1 negative active edge))
		if (this->DEBUG)
			WriteLog("r6522::setCB1 current=%01X new=%01X, PCR=%02X\n", this->cb1, value, this->pcr);
		this->irb = this->pb;			// Latch PB into IRB
		this->ifr |= 0x10;				// ...and set flag in the IFR
	}
	this->cb1 = value;
}

void r6522::setCB2(bool value) {
	if ((this->pcr & 0x80) == 0x00) {	// CB2 must be in input mode to set its value
		if (((value != this->ca1) && ((this->pcr & 0x0C) == 0x04) && (value == 1)) || ((value != this->ca1) && ((this->pcr & 0x0C) == 0x00) && (value == 0))) {	// ((CB2 positive active edge) || (CB2 negative active edge))
			if (this->DEBUG)
				WriteLog("r6522::setCB2 current=%01X new=%01X, PCR=%02X\n", this->cb2, value, this->pcr);
			this->ifr |= 0x08;
		}
		this->cb2 = value;
	}
}

void r6522::updateInterrupt(void) {
	if (this->ifr & (this->ier & 0x7F)) {
		this->ifr |= 0x80;		// Update top bit of IFR
		this->irq = FALSE;		// ... and the IRQ output
	} else {
		this->ifr &= 0x7F;		// Update top bit of IFR
		this->irq = TRUE;		// ... and the IRQ output
	}
}
