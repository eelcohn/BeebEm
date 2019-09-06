/* Western Digital wd177x/wd179x/wd279x FDC (Floppy Disc Controller) object module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "../main.h"				// Included for WriteLog()
#include "wd177x.h"

/* Constructor / Deconstructor */

wd177x::wd177x(int FDCType) {
}

wd177x::~wd177x(void) {
}

void wd177x::Reset(void) {
	this->command				= 0x03;	// Do a 'restore' command on reset
	this->status				= 0x01;	// And set the busy bit, so it gets processed
	this->track					= 0x00;
	this->sector				= 0x01;
	this->data					= 0x00;

	this->intrq					= true;		// INTRQ is active low output
	this->dden					= NULL;		// DDEN is input
	this->enp					= NULL;		// ENP is input
	this->drq					= false;	// DRQ is active high output
	this->tr00					= NULL;		// TR00 is input

	this->DEBUG					= false;
	this->ImmediateInterrupt	= false;
	this->SpinUpSequenceCounter	= 0;
	this->commandPhase			= 0;
}

void wd177x::Exec(int Cycles) {
	bool			u;	// Update track register flag
	bool			v;	// Verify track position flag
	bool			h;	// Disable motor spin-up sequence flag
	bool			m;	// Multiple sectors flag
	bool			e;	// Settling delay flag
	bool			p;	// Write pre-compensation flag
	bool			a;	// Data address mark flag

	u = m	= ((this->command & 0x10) == 0x10);
	h		= ((this->command & 0x08) == 0x08);
	v = e	= ((this->command & 0x04) == 0x04);
	p		= ((this->command & 0x02) == 0x02);
	a		= ((this->command & 0x01) == 0x01);

	if (this->status & 0x01) {	// Busy bit is set, so a command is being processed
		if (this->DEBUG)
			WriteLog("wx177x::Exec command=%02X\n", this->command);

		this->SpinDownCounter = 10;	// 

		/****** Update the spin up counter on type 1 commands *****************************/
		if (((this->command & 0x80) == 0x00) && ((this->status & 0x20) == 0x00) && (this->index)) {
			this->SpinUpSequenceCounter++;
			if (this->SpinUpSequenceCounter == 6) {
				this->status |= 0x20;				// Set the spin-up completed flag
				this->SpinUpSequenceCounter = 0;
			}
		}

		switch ((this->command) & 0xF0) {
			case 0x00 :	// Restore
				if (this->tr00) {
					this->track		= 0x00;				// Set track register to 0
					this->intrq		= false;			// Generate interrupt
					this->status	&= 0x01;			// Clear BUSY bit in status register
				} else {
					if (this->commandPhase == 0) {
						this->track	= 0xFF;
						this->data	= 0x00;
					}
				}
				break;

			case 0x10 :	// Seek
				break;

			case 0x20 :	// Step
			case 0x30 :
				if (this->commandPhase == 0)
					this->data	= 0x00;
				break;

			case 0x40 :	// Step in
			case 0x50 :
				if (this->commandPhase == 0)
					this->dir = true;				// Set direction
				break;

			case 0x60 :	// Step out
			case 0x70 :
				if (this->commandPhase == 0)
					this->dir = false;				// Set direction
				break;

			case 0x80 :	// Read sector
			case 0x90 :
				break;

			case 0xA0 :	// Write sector
			case 0xB0 :
				break;

			case 0xC0 :	// Read address
				break;

			case 0xD0 :	// Force interrupt
				if ((this->command & 0xFC) == 0xD0) {		// Terminate immediately without interrupt
					this->status &= 0x01;					// Reset BUSY bit
					this->ImmediateInterrupt = false;		// Clear immediate interrupts, if any
				}

				if ((this->command & 0xFC) == 0xD4) {		// Interrupt on every index pulse
					if (this->index == true)
						this->intrq = true;
				}

				if ((this->command & 0xFC) == 0xD8) {		// Terminate immediately with interrupt
						this->status &= 0x01;				// Reset BUSY bit
						this->ImmediateInterrupt = false;	// Clear immediate interrupts, if any
						this->intrq = true;					// Set interrupt output
				}
				break;

			case 0xE0 :	// Read track
				break;

			case 0xF0 :	// Write track
				break;
		}
	} else {
		this->SpinDownCounter--;
		if (this->SpinDownCounter == 0) {
//			Motor off;
		}
	}			
}

unsigned char wd177x::ReadRegister(unsigned char Register) {
	unsigned char	Value;

	switch (Register) {
		case 0x00 :	// Status register
			Value = this->status;
			if (this->ImmediateInterrupt == false)	// Do not reset INTRQ signal if previous command was 0xD8 (Immediate interrupt)
				this->intrq = false;	// Reset INTRQ signal
			break;

		case 0x01 :	// Track register
			Value = this->track;
			break;

		case 0x02 :	// Sector register
			Value = this->sector;
			break;

		case 0x03 :	// Data register
			Value = this->data;
			if (this->command & 0x80) {
				this->status &= 0x02;	// Clear the 'Data Request' bit in the status register
				this->drq = false;	// De-activate the DRQ output
			}
			break;
	}

	if (this->DEBUG)
		WriteLog("wd177x::ReadRegister register=%02X, value=%02X\n", Register, Value);

	return (Value);
}

void wd177x::WriteRegister(unsigned char Register, unsigned char Value) {
	if (this->DEBUG)
		WriteLog("wd177x::WriteRegister register=%02X, value=%02X\n", Register, Value);

	switch (Register) {
		case 0x00 :	// Command register
			this->command = Value;
			if (this->ImmediateInterrupt == false)	// Do not reset INTRQ signal if previous command was 0xD8 (Immediate interrupt)
				this->intrq = false;	// Reset INTRQ signal
			this->status |= 0x01;	// Set busy bit to indicate a command has been loaded
			break;

		case 0x01 :	// Track register
			this->track = Value;
			break;

		case 0x02 :	// Sector register
			this->sector = Value;
			break;

		case 0x03 :	// Data register
			this->data = Value;
			if (this->command & 0x80) {
				this->status &= 0x02;	// Clear the 'Data Request' bit in the status register
				this->drq = false;	// De-activate the DRQ output
			}
			break;
	}
}
