/* Acorn ULA9C018 TUBE ULA object module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "../main.h"				// Included for WriteLog()
#include "ula9c018.h"

/* Constructor / Deconstructor */

ula9c018::ula9c018(void) {
	this->host_irq		= TRUE;
	this->host_rst		= TRUE;
	this->parasite_irq	= TRUE;
	this->parasite_int	= TRUE;
	this->parasite_rst	= TRUE;
	this->drq			= FALSE;

	this->DEBUG		= FALSE;
}

ula9c018::~ula9c018(void) {
}

void ula9c018::Reset(void) {
	this->parasite_rst		= FALSE;	// Reset parasite system
	this->clearRegisters();				// Clear all registers
}

void ula9c018::Exec(int Cycles) {
//	if (this->DEBUG)
//		WriteLog("ula9c018::Exec\n");
}

unsigned char ula9c018::ReadHostRegister(unsigned char Register) {
	unsigned char	i, Value;

	switch (Register) {
		case 0x00 :	// Register 1 status
			Value = this->hp_r1status | this->control;
			break;

		case 0x01 :	// Register 1 data
			Value = this->ph_r1data[0];
			if (this->ph_r1data_ptr != 0) {
				for (i = 0; i < 23; i++)
					this->ph_r1data[i] = this->ph_r1data[i + 1];
				this->ph_r1data_ptr--;
				this->ph_r1status |= 0x40;		// Set the 'not full' flag
				if (this->ph_r1data_ptr == 0)
					this->hp_r1status &= ~0x80;	// Clear the 'data available' flag
			}
			break;

		case 0x02 :	// Register 2 status
			Value = this->hp_r2status;
			break;

		case 0x03 :	// Register 2 data
			Value = this->ph_r2data;
			if (this->ph_r2status & 0x80) {
				this->hp_r2status &= ~0x80;		// Clear the 'data available' flag
				this->ph_r2status |= 0x40;		// Set the 'not full' flag
			}
			break;

		case 0x04 :	// Register 3 status
			Value = this->hp_r3status;
			break;

		case 0x05 :	// Register 3 data
			Value = this->ph_r3data[0];
			if (this->control & 0x10) {				// register 3 is in 2-byte FIFO mode
				if (this->ph_r3data_ptr != 0) {
					this->ph_r3data[0] = this->ph_r3data[1];
					this->ph_r3data_ptr--;
					this->ph_r3status |= 0x40;			// Set the 'not full' flag
					if (this->ph_r3data_ptr == 0) {
						this->hp_r3status &= ~0x80;		// Clear the 'data available' flag
						if (this->control & 0x08)
							this->parasite_int = FALSE;	// Activate parasite NMI
					}
				}
			} else {
				this->hp_r3status &= ~0x80;				// Clear the 'data available' flag
				this->ph_r3status |= 0x40;				// Set the 'not full' flag
				if (this->control & 0x08)
					this->parasite_int = FALSE;			// Activate parasite NMI
			}
			break;

		case 0x06 :	// Register 4 status
			Value = this->hp_r4status;
			break;

		case 0x07 :	// Register 4 data
			Value = this->ph_r4data;
			if (this->ph_r4status & 0x80) {
				this->host_irq = TRUE;					// De-activate host IRQ
				this->hp_r4status &= ~0x80;				// Clear the 'data available' flag
				this->ph_r4status |= 0x40;				// Set the 'not full' flag
			}
			break;
	}

	if (this->DEBUG)
		WriteLog("ula9c018::ReadHostRegister register=%02X, value=%02X\n", Register, Value);

	return (Value);
}

unsigned char ula9c018::ReadParasiteRegister(unsigned char Register) {
	unsigned char	Value;

	switch (Register) {
		case 0x00 :	// Register 1 status
			Value = this->ph_r1status | this->control;
			break;

		case 0x01 :	// Register 1 data
			Value = this->hp_r1data;
			if (this->ph_r1status & 0x80) {
				this->ph_r1status &= ~0x80;
				this->hp_r1status |= 0x40;
			}
			if ((this->control & 0x02) && (this->ph_r1status & 0x80))
				this->parasite_irq = TRUE;			// De-activate parasite IRQ
			break;

		case 0x02 :	// Register 2 status
			Value = this->ph_r2status;
			break;

		case 0x03 :	// Register 2 data
			Value = this->hp_r2data;
			if (this->ph_r2status & 0x80) {
				this->ph_r2status &= ~0x80;
				this->hp_r2status |= 0x40;
			}
			break;

		case 0x04 :	// Register 3 status
			Value = this->ph_r3status;
			break;

		case 0x05 :	// Register 3 data
			Value = this->hp_r3data[0];
			if (this->control & 0x10) {				// register 3 is in 2-byte FIFO mode
				if (this->hp_r3data_ptr != 0) {
					this->hp_r3data[0] = this->hp_r3data[1];
					this->hp_r3data_ptr--;
					this->hp_r3status |= 0x40;			// Set the 'not full' flag
					if (this->hp_r3data_ptr == 0) {
						this->ph_r3status &= ~0x80;		// Clear the 'data available' flag
						if (this->control & 0x08)
							this->parasite_int = FALSE;	// Activate parasite NMI
					}
				}
			} else {
				this->ph_r3status &= ~0x80;				// Clear the 'data available' flag
				this->hp_r3status |= 0x40;				// Set the 'not full' flag
				if (this->control & 0x08)
					this->parasite_int = FALSE;			// Activate parasite NMI
			}
			break;

		case 0x06 :	// Register 4 status
			Value = this->ph_r4status;
			break;

		case 0x07 :	// Register 4 data
			Value = this->hp_r4data;
			if (this->ph_r4status & 0x80) {
				this->parasite_irq = TRUE;			// De-activate parasite IRQ
				this->ph_r4status &= ~0x80;			// Clear the 'data available' flag
				this->hp_r4status |= 0x40;			// Set the 'not full' flag
			}
			break;
	}

	if (this->DEBUG)
		WriteLog("ula9c018::ReadParasiteRegister register=%02X, value=%02X\n", Register, Value);

	return (Value);
}

void ula9c018::WriteHostRegister(unsigned char Register, unsigned char Value) {
	if (this->DEBUG)
		WriteLog("ula9c018::WriteHostRegister register=%02X, value=%02X\n", Register, Value);

	switch (Register) {
		case 0x00 :	// Register 1 status
			if (Value & 0x80)
				this->control |= (Value & 0x3F);
			else
				this->control &= ~(Value & 0x3F);

			if (this->control & 0x40)
				this->clearRegisters();				// T-bit: Reset TUBE ULA
			if (this->control & 0x20)
				this->parasite_rst = FALSE;			// P-bit: Reset parasite system
			else
				this->parasite_rst = TRUE;
			break;

		case 0x01 :	// Register 1 data
			this->hp_r1data = Value;
			this->hp_r1status &= 0xBF;				// Clear F1-flag
			this->ph_r1status |= 0x80;				// Set A1-flag (data available)
			if (this->control & 0x02)
				this->parasite_irq = FALSE;			// Activate parasite IRQ
			break;

		case 0x03 :	// Register 2 data
			this->hp_r2data = Value;
			this->hp_r2status &= 0xBF;				// Clear F2-flag
			this->ph_r2status |= 0x80;				// Set A2-flag (data available)
			break;

		case 0x05 :	// Register 3 data
			if (this->control & 0x10) {				// register 3 is in 2-byte FIFO mode
				if (this->hp_r3data_ptr != 2)
					this->hp_r3data[this->hp_r3data_ptr++] = Value;
				if (this->hp_r3data_ptr == 2) {
					this->ph_r3status |= 0x80;		// Set A3-flag (data available)
					this->hp_r3status &= ~0x40;		// Clear F3 flag
					if (this->control & 0x08)
						this->parasite_int = FALSE;	// Activate parasite NMI
				}
			} else {
				this->hp_r3data[0] = Value;
				this->ph_r3status |= 0x80;			// Set A3-flag (data available)
				this->hp_r3status &= ~0x40;			// Clear F3 flag
				if (this->control & 0x08)
					this->parasite_int = FALSE;		// Activate parasite NMI
			}
			break;

		case 0x07 :	// Register 4 data
			this->hp_r4data = Value;
			this->hp_r4status &= 0xBF;				// Clear F4-flag
			this->ph_r4status |= 0x80;				// Set A4-flag (data available)
			break;
			if (this->control & 0x04)				// If the J-bit is set, then...
				this->parasite_irq = FALSE;			// Activate parasite IRQ
	}
}

void ula9c018::WriteParasiteRegister(unsigned char Register, unsigned char Value) {
	if (this->DEBUG)
		WriteLog("ula9c018::WriteParasiteRegister register=%02X, value=%02X\n", Register, Value);

	switch (Register) {
		case 0x01 :	// Register 1 data
			if (this->ph_r1data_ptr != 24) {		// todo: when the FIFO is full, and the 'not full' flag is cleared, and a byte is written anyways, will it overwrite r1data[24]???
				this->ph_r1data[this->ph_r1data_ptr++] = Value;
				this->hp_r1status |= 0x80;			// Set A1-flag (data available)
				if (this->ph_r1data_ptr == 24)
					this->ph_r1status &= ~0x40;		// Clear the 'not full' flag
				WriteLog(" ph_r1data_ptr=%02X\n", this->ph_r1data_ptr);
			}
			break;

		case 0x03 :	// Register 2 data
			this->ph_r2data = Value;
			this->ph_r2status &= ~0x40;				// Clear F1-flag
			this->hp_r2status |= 0x80;				// Set A2-flag (data available)
			break;

		case 0x05 :	// Register 3 data
			if (this->control & 0x10) {				// register 3 is in 2-byte FIFO mode
				if (this->ph_r3data_ptr != 2)
					this->ph_r3data[this->ph_r3data_ptr++] = Value;
				if (this->ph_r3data_ptr == 2) {
					this->hp_r3status |= 0x80;		// Set A3-flag (data available)
					this->ph_r3status &= ~0x40;		// Clear F3 flag
				}
			} else {
				this->ph_r3data[0] = Value;
				this->hp_r3status |= 0x80;			// Set A3-flag (data available)
				this->ph_r3status &= ~0x40;			// Clear F3 flag
			}
			break;

		case 0x07 :	// Register 4 data
			this->ph_r4data = Value;
			this->ph_r4status &= 0xBF;				// Clear F1-flag
			this->hp_r4status |= 0x80;				// Set A4-flag (data available)
			if (this->control & 0x01)				// If the Q-bit is set, then...
				this->host_irq = FALSE;				// Activate host IRQ
			break;
	}
}

void ula9c018::clearRegisters(void) {
	unsigned char	i;

	this->control			= 0x00;
	this->hp_r1status		= 0x40;
	this->hp_r2status		= 0x40;
	this->hp_r3status		= 0xC0;
	this->hp_r4status		= 0x40;
	this->hp_r1data			= 0x00;
	this->hp_r2data			= 0x00;
	this->hp_r3data[0]		= 0x00;
	this->hp_r3data[1]		= 0x00;
	this->hp_r4data			= 0x00;
	this->ph_r1status		= 0x40;
	this->ph_r2status		= 0x40;
	this->ph_r3status		= 0x40;
	this->ph_r4status		= 0x40;
	for (i=0; i<24; i++)
		this->ph_r1data[i]	= 0x00;
	this->ph_r2data			= 0x00;
	this->ph_r3data[0]		= 0xFF;
	this->ph_r3data[1]		= 0xFF;
	this->ph_r4data			= 0x00;
	this->ph_r1data_ptr		= 0;
	this->ph_r3data_ptr		= 0;
	this->hp_r3data_ptr		= 0;
}