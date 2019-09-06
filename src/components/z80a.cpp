/* Zilog Z80 CPU object module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include <stdio.h>					// Included for FILE *
#include <string.h>					// Included for strcpy, strcat

#include "z80a.h"
#include "../beebmem.h"				// Included for RomPath variable
#include "../main.h"				// Included for WriteLog()
#include "../tube.h"				// Included for ReadTubeFromParasiteSide() and WriteTubeFromParasiteSide()
#include "../z80mem.h"
#include "../z80.h"					// Included for init_z80() and z80_execute()



/* Constructor / Deconstructor */

z80::z80(int CPUType) {
	this->cpu.type = CPUType;
}

z80::~z80(void) {
}



/* Main code */

void z80::Reset(void) {
	init_z80();

	this->cpu.af	= 0x0000;
	this->cpu.bc	= 0x0000;
	this->cpu.de	= 0x0000;
	this->cpu.hl	= 0x0000;
	this->cpu.i		= 0x00;
	this->cpu.r		= 0x00;
	this->cpu.ix	= 0x0000;
	this->cpu.iy	= 0x0000;
	this->cpu.sp	= 0x0000;
	this->cpu.pc	= 0x0000;	// Set PC to initial value
	this->cpu.iff1	= FALSE;
	this->cpu.iff2	= FALSE;

//	cyclecount			= 1000000;
	this->DEBUG			= FALSE;
}

void z80::Exec(int Cycles) {
	unsigned int	value;

	unsigned char	size;

	z80_execute();

	while (Cycles > 0) {
//		if (this->cpu.pc == 0x0000)
//		if (this->Architecture == ACORNZ80)
//			this->DEBUG = TRUE;
//			this->DEBUG = FALSE;

//		if (this->cpu.pc == 0x0000)
//			this->dumpMemory();

//		if (cyclecount > 0) {
			if (this->cpu.halt == TRUE)
				this->cpu.instruction_reg = 0x00;	// Execute NOP's when the CPU is halted
			else
				this->cpu.instruction_reg = this->readByte(this->cpu.pc);
			this->dumpRegisters();
			this->cpu.pc += 1;
//		}
		Cycles--;								// Not cycle exact at the moment, but it works for now...
	}
}

unsigned char z80::readByte(unsigned int address) {
	unsigned char returnvalue;

	switch (this->Architecture) {
		case ACORN_Z80 :
			break;

		case TORCH_Z80 :
			break;

		case PEDL_Z80 :
			break;

		default :
			return(0);
			break;
	}
}

unsigned short z80::readWord(unsigned int address) {
	unsigned short	temp;

	temp = ((this->readByte(address) << 8) | (this->readByte(address+1)));
	return (temp);
}


unsigned int z80::readLong(unsigned int address) {
	unsigned long	temp;

	temp = ((this->readByte(address) << 24) | (this->readByte(address+1) << 16) | (this->readByte(address+2) << 8) | (readByte(address+3)));
	return (temp);
}

void z80::writeByte(unsigned int address, unsigned char value) {
	switch (this->Architecture) {
		case ACORN_Z80 :
			break;

		case TORCH_Z80 :
			break;

		case PEDL_Z80 :
			break;

		default :
			break;
	}
}

void z80::writeWord(unsigned int address, unsigned short value) {
	writeByte(address, ((value & 0xFF00)>>8));
	writeByte(address+1, (value & 0x00FF));
}

void z80::writeLong(unsigned int address, unsigned int value) {
	writeByte(address, ((value & 0xFF000000)>>24));
	writeByte(address+1, ((value & 0x00FF0000)>>16));
	writeByte(address+2, ((value & 0x0000FF00)>>8));
	writeByte(address+3, (value & 0x000000FF));
}

bool z80::evaluateConditionCode(unsigned char ConditionCode) {
	return (0);
}

void z80::dumpMemory(void) {
	FILE *fp;
	char path[256];

	strcpy(path, "C:\\Users\\Virustest\\Desktop\\BeebEm414_mc68k\\BeebEm\\Z80MemoryDump.bin");

	fp = fopen(path, "rb");
	if (fp) {
		fclose(fp);	// Memory dump file already exists, so don't overwrite
	} else {
		fp = fopen(path, "wb");
		for (unsigned int i=0;i < 0x10000; i++)
			fputc(this->readByte(i), fp);
		fclose(fp);
		if (this->DEBUG)
			WriteLog("z80::dumpMemory - memory dumped to %s\n", path);
	}
}

void z80::dumpRegisters(void) {
	char		output[9];
	int		i;

	for (i=0; i<8; i++)
		if ((0x80 >> i) & this->cpu.f.reg)
			output[i] = '1';
		else
			output[i] = '0';
	output[8] = 0x00;

	WriteLog("z80::dumpRegisters PC=%04X  %04X  AF=%04X BC=%04X DE=%04X HL=%04X IX=%04X IY=%04X I=%02X R=%02X SP=%04X F=%s\n", this->cpu.pc, this->cpu.instruction_reg, this->cpu.af, this->cpu.bc, this->cpu.de, this->cpu.hl, this->cpu.ix, this->cpu.iy, this->cpu.i, this->cpu.r, this->cpu.sp, output);
}
