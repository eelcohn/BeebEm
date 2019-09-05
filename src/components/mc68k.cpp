/* Motorola 68k CPU object module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include <stdio.h>					// Included for FILE *
#include <string.h>					// Included for strcpy, strcat

#include "mc68k.h"
#include "../beebmem.h"				// Included for RomPath variable
#include "../main.h"				// Included for WriteLog()
#include "../tube.h"				// Included for ReadTubeFromParasiteSide() and WriteTubeFromParasiteSide()
#include "../peripherals/copro_casper.h"			// Included for the copro_casper object
#include "../peripherals/copro_ciscos.h"			// Included for the copro_ciscos object
#include "../peripherals/copro_cumana.h"			// Included for the copro_cumana object

extern copro_casper *obj_copro_casper;	// Object needed for memory access
extern copro_ciscos *obj_copro_ciscos;	// Object needed for memory access
extern copro_cumana *obj_copro_cumana;	// Object needed for memory access

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

/* Constructor / Deconstructor */

mc68k::mc68k(int CPUType, int FPUType, int MMUType) {
	this->cpu.type = CPUType;
	this->fpu.type = FPUType;
	this->mmu.type = MMUType;

	switch (this->cpu.type) {
		case MC68008L :
			this->INTERNAL_ADDRESS_MASK = 0x00FFFFFF;
			this->EXTERNAL_ADDRESS_MASK = 0x001FFFFF;
			break;
		case MC68008P :
			this->INTERNAL_ADDRESS_MASK = 0x00FFFFFF;
			this->EXTERNAL_ADDRESS_MASK = 0x003FFFFF;
			break;
		case MC68000 :
			this->INTERNAL_ADDRESS_MASK = 0x00FFFFFF;
			this->EXTERNAL_ADDRESS_MASK = 0x00FFFFFF;
			break;
		default :
			this->INTERNAL_ADDRESS_MASK = 0xFFFFFFFF;
			this->EXTERNAL_ADDRESS_MASK = 0xFFFFFFFF;
			break;
	}
}

mc68k::~mc68k(void) {
}



/* Main code */

void mc68k::Reset(void) {
	WriteLog("%s::Reset - Reset called\n", __FILENAME__);
	for(int rNum = 0; rNum<8; rNum++)		// Clear D0...D7
		this->cpu.d[rNum] = 0;

	for(int rNum = 0; rNum<7; rNum++)		// Clear A0...A6
		this->cpu.a[rNum] = 0;

//	for(int rNum = 0; rNum<8; rNum++)		// Clear FP0...FP7
//		FP[rNum] = 0;

	this->cpu.usp		= 0x00000000;		// Clear USP
	this->cpu.vbr		= 0x00000000;		// Vector base register (not used in 68008 and 68000)
	this->cpu.cacr		= 0x00000000;
	this->cpu.caar		= 0x00000000;
	this->cpu.isp		= 0x00000000;
	this->cpu.msp		= 0x00000000;
	this->cpu.sfc		= 0x00000000;
	this->cpu.dfc		= 0x00000000;
	this->cpu.ac0		= 0x00000000;
	this->cpu.ac1		= 0x00000000;
	this->cpu.acusr		= 0x0000;
	this->fpu.fpcr.reg	= 0x00000000;
	this->fpu.fpsr		= 0x00000000;
	this->fpu.fpiar		= 0x00000000;
	this->cpu.urp		= 0x00000000;
	this->cpu.srp		= 0x00000000;
	this->cpu.tc		= 0x0000;
	this->cpu.dtt0		= 0x00000000;
	this->cpu.dtt1		= 0x00000000;
	this->cpu.itt0		= 0x00000000;
	this->cpu.itt1		= 0x00000000;
	this->cpu.mmusr		= 0x00000000;

	this->pendingException	= -1;
	this->interruptLevel	= 0;
	this->currentInterrupt	= -1;

	this->cpu.stop		= false;								// CPU is in running mode
	this->cpu.ssp		= this->readLong(this->cpu.vbr+0x000);	// Set SSP to initial value (Note: the 68k boards from Eelco have already switched to RAM when the SSP is read)
	this->cpu.pc		= this->readLong(this->cpu.vbr+0x004);	// Set PC to initial value
	this->cpu.sr.reg	= 0x2700;								// Only supervisor bit is set on a reset
	cyclecount			= 1000000;
	this->DEBUG			= false;
}

void mc68k::Exec(int Cycles) {
	unsigned int	value;

	unsigned char	size;

	while (Cycles > 0) {
//		if (this->cpu.pc == 0x000007D4)	// Casper
//		if (this->cpu.pc == 0x00FF0322)	// CiscOS
//		if (this->cpu.pc == 0x00008662)	// Cumana
//		if (this->Architecture == CUMANA)
//			this->DEBUG = true;
//			this->DEBUG = false;

//		if (this->cpu.pc == 0x000087D6)
//			this->dumpMemory();

		if ((this->interruptLevel > ((this->cpu.sr.reg & 0700) >> 8)) && (this->currentInterrupt != this->interruptLevel)) {
			this->pendingException = (this->interruptLevel + INTERRUPT1 - 1);
			this->currentInterrupt = this->interruptLevel;				// Replace the this->currentInterrupt variable with FC0..2 = 111
		}

		if ((this->currentInterrupt != -1) && (this->currentInterrupt < this->interruptLevel)) {	// If the requested input is greater than (I2I1I0), the interrupt is serviced, otherwise it is ignored
			if (this->DEBUG == true)
				WriteLog("mc68k::Exec - New interrupt %02X being serviced, abandoning old interrupt %02X\n", this->interruptLevel, this->currentInterrupt);
			this->currentInterrupt = this->interruptLevel;
		}

		if (this->pendingException != -1) {
			if (this->DEBUG == true)
				WriteLog("mc68k::Exec - Exception %02X at %08X, jumping to handler at %08X\n", this->pendingException, this->cpu.pc, this->readLong(this->cpu.vbr+(pendingException * 4)));

			this->cpu.ssp -= 4;
			writeLong(this->cpu.ssp, this->cpu.pc);						// Push program counter to supervisor stack
			this->cpu.ssp -= 2;
			writeWord(this->cpu.ssp, this->cpu.sr.reg);					// Push Status Register to supervisor stack

			if ((this->pendingException == BUS_ERROR) || (this->pendingException == ADDRESS_ERROR)) {
				this->cpu.ssp -= 2;
				writeWord(this->cpu.ssp, this->cpu.instruction_reg);	// Push Instruction Register (opcode of last instruction) to supervisor stack
				this->cpu.ssp -= 4;
				writeLong(this->cpu.ssp, 0x00000000);					// ***TODO*** Push Access Address to supervisor stack
				this->cpu.ssp -= 2;
				writeWord(this->cpu.ssp, 0x0000);						// ***TODO*** Push "Memory access type and function code" to supervisor stack
			}

			if ((this->pendingException >= TRAP0) && (this->pendingException <= TRAP15))
				this->cpu.stop = false;									// Reset STOP flag on interrupt

			this->cpu.sr.flag.s = true;									// Set supervisor bit
			this->cpu.pc = this->readLong(this->cpu.vbr + (this->pendingException * 4));

			this->pendingException = -1;								// Clear any other pending exceptions
		}

		if ((!this->cpu.stop) && (cyclecount > 0)) {
			this->cpu.instruction_reg = this->readWord(this->cpu.pc);
			this->dumpRegisters();
			this->cpu.pc += 2;

			if ((this->cpu.instruction_reg & 0xFF00) == 0x0000)
				this->ORIhandler();			// ORI

			if ((this->cpu.instruction_reg & 0xFF00) == 0x0200)
				this->ANDIhandler();		// ANDI

			if ((this->cpu.instruction_reg & 0xFF00) == 0x0400)
				this->CMPISUBIhandler();	// SUBI

			if ((this->cpu.instruction_reg & 0xFF00) == 0x0600)
				this->ADDIhandler();		// ADDI

			if ((this->cpu.instruction_reg & 0xFF00) == 0x0A00)
				this->EORIhandler();		// EORI

			if ((this->cpu.instruction_reg & 0xFF00) == 0x0C00)
				this->CMPISUBIhandler();	// CMPI

			if ((this->cpu.instruction_reg & 0xFFC0) == 0x0800)
				this->BTSThandler();		// BTST

			if ((this->cpu.instruction_reg & 0xFFC0) == 0x0840)
				this->BCHGhandler();		// BCHG

			if ((this->cpu.instruction_reg & 0xFFC0) == 0x0880)
				this->BCLRhandler();		// BCLR

			if ((this->cpu.instruction_reg & 0xFFC0) == 0x08C0)
				this->BSEThandler();		// BSET

			if ((this->cpu.instruction_reg & 0xF138) == 0x0108)
				this->MOVEPhandler();		// MOVEP

			if (((this->cpu.instruction_reg & 0xC000) == 0x0000) && (this->cpu.instruction_reg & 0x3000) != 0)
				this->MOVEhandler();		// MOVE / MOVEA

			if ((this->cpu.instruction_reg & 0xF900) == 0x4000) {
				size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
				if (((this->cpu.instruction_reg & 0xFFC0) == 0x40C0) && (size == 0x03)) {		// MOVE from SR
					if (((this->cpu.type != MC68008L) && (this->cpu.type != MC68008P) && (this->cpu.type != MC68000)) && this->cpu.sr.flag.s)	// Privileged instruction on > 68010
						this->pendingException = PRIVILEGE_VIOLATION;
					else
						this->setOperandValue(this->cpu.instruction_reg & 0x3F, 0x01, this->cpu.sr.reg);
				}

				if (((this->cpu.instruction_reg & 0xFFC0) == 0x44C0) && (size == 0x03))			// MOVE to CCR
					this->cpu.sr.reg = ((this->cpu.sr.reg & 0xFF00) | (getOperandValue(this->cpu.instruction_reg & 0x3F, 0x01) & 0x00FF));

				if (((this->cpu.instruction_reg & 0xFFC0) == 0x46C0) && (size == 0x03)) {		// MOVE to SR
					if (this->cpu.sr.flag.s)								// Only privileged instruction on > 68010
						this->cpu.sr.reg = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x01);
					else
						this->pendingException = PRIVILEGE_VIOLATION;
				}
			}

			if (((this->cpu.instruction_reg & 0xFB00) == 0x4000) && ((this->cpu.instruction_reg & 0x00C0) != 0x00C0))			// NEG and NEGX
				this->NEGhandler();

			if ((this->cpu.instruction_reg & 0xFF00) == 0x4200) {		// CLR
				size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
				if (size == 0x03)
					this->pendingException = ILLEGAL_INSTRUCTION;
				else {
					this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, 0);
					this->cpu.sr.flag.c = false;
					this->cpu.sr.flag.v = false;
					this->cpu.sr.flag.n = false;
					this->cpu.sr.flag.z = true;
				}
			}

			if (((this->cpu.instruction_reg & 0xFF00) == 0x4600) && ((this->cpu.instruction_reg & 0x00C0) != 0x00C0))		// NOT
				this->NOThandler();

			if ((this->cpu.instruction_reg & 0xFFB8) == 0x4880)
				this->EXThandler();		// EXT

			if ((this->cpu.instruction_reg & 0xFFC0) == 0x4800)
				this->NBCDhandler();		// NBCD

			if ((this->cpu.instruction_reg & 0xFFF8) == 0x4840)
				this->SWAPhandler();		// SWAP

			if (((this->cpu.instruction_reg & 0xFFC0) == 0x4840) && ((this->cpu.instruction_reg & 0x0038) != 0x0000)) {		// PEA
				if ((this->cpu.instruction_reg & 0x3F) == 0x3A) {		// Hack because PEA's with PC+Displacement don't read the address
					value = this->cpu.pc + this->readWord(this->cpu.pc);
					this->cpu.pc += 2;
				} else
					value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x02);

				if (this->cpu.sr.flag.s) {
					this->cpu.ssp -= 4;
					writeLong(this->cpu.ssp, value);			// In Supervisor mode, so use the Supervisor Stack
				} else {
					this->cpu.usp -= 4;
					writeLong(this->cpu.usp, value);
				}
			}

			if (this->cpu.instruction_reg == 0x4AFC)		// ILLEGAL
				this->pendingException = ILLEGAL_INSTRUCTION;

			if ((this->cpu.instruction_reg & 0xFFC0) == 0x4AC0)
				this->TAShandler();		// TAS

			if ((this->cpu.instruction_reg & 0xFF00) == 0x4A00)
				this->TSThandler();		// TST

			if ((this->cpu.instruction_reg & 0xFFF0) == 0x4E40)
				this->pendingException = (TRAP0 + (this->cpu.instruction_reg & 0x000F));		// TRAP

			if ((this->cpu.instruction_reg & 0xFFF8) == 0x4E50)	
				this->LINKhandler();	// LINK

			if ((this->cpu.instruction_reg & 0xFFF8) == 0x4E58)
				this->UNLINKhandler();		// UNLINK

			if ((this->cpu.instruction_reg & 0xFFF0) == 0x4E60) {		// MOVE USP
				if (this->cpu.sr.flag.s) {
					if (this->cpu.instruction_reg & 0x0008)
						this->cpu.a[this->cpu.instruction_reg & 0x0007] = this->cpu.usp;
					else
						this->cpu.usp = this->cpu.a[this->cpu.instruction_reg & 0x0007];
				} else
					this->pendingException = PRIVILEGE_VIOLATION;
			}

			if (this->cpu.instruction_reg == 0x4E70) {		// RESET
				if (this->cpu.sr.flag.s)
					this->pendingException = RESET;			// Should do this differently; RESET just asserts the RESET line
				else
					this->pendingException = PRIVILEGE_VIOLATION;
			}

			if (this->cpu.instruction_reg == 0x4E71) {		// NOP
			}

			if (this->cpu.instruction_reg == 0x4E72) {		// STOP
				if (this->cpu.sr.flag.s) {
					value = this->readWord(this->cpu.pc);
					this->cpu.sr.reg = (value & 0xFFFF);
					this->cpu.stop = true;
				} else
					this->pendingException = PRIVILEGE_VIOLATION;
			}

			if (this->cpu.instruction_reg == 0x4E73) {		// RTE
				if (this->cpu.sr.flag.s) {
					this->cpu.sr.reg = this->readWord(this->cpu.ssp);
					this->cpu.ssp += 2;
					this->cpu.pc = this->readLong(this->cpu.ssp);
					this->cpu.ssp += 4;
				} else
					this->pendingException = PRIVILEGE_VIOLATION;
			}

			if (this->cpu.instruction_reg == 0x4E75) {		// RTS
				if (this->cpu.sr.flag.s) {
					this->cpu.pc = this->readLong(this->cpu.ssp);
					this->cpu.ssp += 4;
				} else {
					this->cpu.pc = this->readLong(this->cpu.usp);
					this->cpu.usp += 4;
				}
			}

			if (this->cpu.instruction_reg == 0x4E76)		// TRAPV
				if (this->cpu.sr.flag.v)
					this->pendingException = TRAPV_INSTRUCTION;

			if (this->cpu.instruction_reg == 0x4E77) {		// RTR
				this->cpu.sr.reg &= 0xFF00;
				this->cpu.sr.reg |= (this->readWord(this->cpu.ssp) & 0x00FF);
				this->cpu.ssp += 2;
				this->cpu.pc = this->readLong(this->cpu.ssp);
				this->cpu.ssp += 4;
			}

			if ((this->cpu.instruction_reg & 0xFFC0) == 0x4E80) {		// JSR
				value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x02);
				if ((this->cpu.instruction_reg & 0x38) == 0x10)			// Hack because JSR's with (Ax) don't read the address
					value = this->cpu.a[this->cpu.instruction_reg & 0x07];
				if ((this->cpu.instruction_reg & 0x3F) == 0x38)			// Hack because JSR's with $xxxx.W don't read the address
					value = this->signExtend(this->readWord(this->cpu.pc - 2));

				if (this->cpu.sr.flag.s) {
					this->cpu.ssp -= 4;
					writeLong(this->cpu.ssp, this->cpu.pc);			// In Supervisor mode, so use the Supervisor Stack
				} else {
					this->cpu.usp -= 4;
					writeLong(this->cpu.usp, this->cpu.pc);
				}
				this->cpu.pc = (value & this->INTERNAL_ADDRESS_MASK);
				if (this->cpu.pc & 1)
					this->pendingException = ADDRESS_ERROR;
			}

			if ((this->cpu.instruction_reg & 0xFFC0) == 0x4EC0) {		// JMP
				value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x02);
				if ((this->cpu.instruction_reg & 0x38) == 0x10)			// Hack because JMP's with (Ax) don't read the address
					value = this->cpu.a[this->cpu.instruction_reg & 0x07];
				if ((this->cpu.instruction_reg & 0x3F) == 0x38)			// Hack because JMP's with $xxxx.W don't read the address
					value = this->signExtend(this->readWord(this->cpu.pc - 2));

				this->cpu.pc = (value & this->INTERNAL_ADDRESS_MASK);
				if (this->cpu.pc & 1)
					this->pendingException = ADDRESS_ERROR;
			}

			if ((this->cpu.instruction_reg & 0xFB80) == 0x4880)
				this->MOVEMhandler();		// MOVEM

			if ((this->cpu.instruction_reg & 0xF1C0) == 0x41C0)
				this->LEAhandler();			// LEA

			if ((this->cpu.instruction_reg & 0xF1C0) == 0x4180)
				this->CHKhandler();			// CHK

			if (((this->cpu.instruction_reg & 0xF100) == 0x5000) && ((this->cpu.instruction_reg & 0x00C0) != 0x00C0))
				this->ADDQhandler();		// ADDQ

			if (((this->cpu.instruction_reg & 0xF100) == 0x5100) && ((this->cpu.instruction_reg & 0x00C0) != 0x00C0))
				this->SUBQhandler();		// SUBQ

			if ((this->cpu.instruction_reg & 0xF0C0) == 0x50C0) {		// Scc / DBcc
				if ((this->cpu.instruction_reg & 0xF0F8) == 0x50C8) {	// DBcc
					if (evaluateConditionCode((this->cpu.instruction_reg & 0x0F00) >> 16)) {
						this->cpu.d[this->cpu.instruction_reg & 0x0007] = (this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0xFFFF0000) | ((this->cpu.d[this->cpu.instruction_reg & 0x0007] -1) & 0x0000FFFF);
						if ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF) != 0x0000FFFF)
							this->cpu.pc += this->signExtend(this->readWord(this->cpu.pc));
						else
							this->cpu.pc += 2;
					} else
						this->cpu.pc += 2;
				} else {											// Scc
					if (evaluateConditionCode((this->cpu.instruction_reg & 0x0F00) >> 16))
						this->setOperandValue(this->cpu.instruction_reg & 0x3F, 0x00, 0xFF);
					else
						this->setOperandValue(this->cpu.instruction_reg & 0x3F, 0x00, 0x00);
				}
			}

			if ((this->cpu.instruction_reg & 0xF000) == 0x6000) {		// BRA / BSR / Bcc
				switch (this->cpu.instruction_reg & 0xFF00) {
					case 0x6000 :		// BRA
						if (this->cpu.instruction_reg & 0x00FF)
							if (this->cpu.instruction_reg & 0x0080)
								this->cpu.pc += (0xFFFFFF00 | (this->cpu.instruction_reg & 0x00FF));
							else
								this->cpu.pc += (this->cpu.instruction_reg & 0x00FF);
						else
							this->cpu.pc += this->signExtend(this->readWord(this->cpu.pc));
						break;

					case 0x6100 :		// BSR
						if (this->cpu.instruction_reg & 0x00FF) {
							if (this->cpu.sr.flag.s) {
								this->cpu.ssp -= 4;
								writeLong(this->cpu.ssp, this->cpu.pc);
							} else {
								this->cpu.usp -= 4;
								writeLong(this->cpu.usp, this->cpu.pc);
							}
							if (this->cpu.instruction_reg & 0x0080)
								this->cpu.pc += (0xFFFFFF00 | (this->cpu.instruction_reg & 0x00FF));
							else
								this->cpu.pc += (this->cpu.instruction_reg & 0x00FF);
						} else {
							if (this->cpu.sr.flag.s) {
								this->cpu.ssp -= 4;
								writeLong(this->cpu.ssp, this->cpu.pc+2);
							} else {
								this->cpu.usp -= 4;
								writeLong(this->cpu.usp, this->cpu.pc+2);
							}
							this->cpu.pc += this->signExtend(this->readWord(this->cpu.pc));
						}
						break;

					default :			// Bcc
						if (evaluateConditionCode((this->cpu.instruction_reg & 0x0F00) >> 8)) {
							if (this->cpu.instruction_reg & 0x00FF) {
								if (this->cpu.instruction_reg & 0x0080)
									this->cpu.pc += (0xFFFFFF00 | (this->cpu.instruction_reg & 0x00FF));
								else
									this->cpu.pc += (this->cpu.instruction_reg & 0x00FF);
							} else
								this->cpu.pc += this->signExtend(this->readWord(this->cpu.pc));
						} else
							if ((this->cpu.instruction_reg & 0x00FF) == 0)
								this->cpu.pc += 2;
						break;
				}
			}

			if ((this->cpu.instruction_reg & 0xF100) == 0x7000) {		// MOVEQ
				this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = (this->cpu.instruction_reg & 0x00FF);
				if (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x00000080)
					this->cpu.d[(this->cpu.instruction_reg & 0x0E00)] = 0xFFFFFF00 | (this->cpu.d[this->cpu.instruction_reg & 0x0E00] & 0x000000FF);

				this->cpu.sr.flag.c = 0;
				this->cpu.sr.flag.v = 0;
				this->cpu.sr.flag.z = ((this->cpu.instruction_reg & 0x00FF) == 0);
				this->cpu.sr.flag.n = ((this->cpu.instruction_reg & 0x0080) == 0x80);
			}

			if ((this->cpu.instruction_reg & 0xF1C0) == 0x80C0)
				this->DIVUhandler();		// DIVU

			if ((this->cpu.instruction_reg & 0xF1C0) == 0x81C0)
				this->DIVShandler();		// DIVS

			if ((this->cpu.instruction_reg & 0xF1F0) == 0x8100)
				this->SBCDhandler();		// SBCD

			if ((this->cpu.instruction_reg & 0xF000) == 0x8000)
				this->ORhandler();		// OR

			if ((this->cpu.instruction_reg & 0xF000) == 0x9000) {		// SUB / SUBA / SUBX
				if ((this->cpu.instruction_reg & 0xF0C0) == 0x90C0) {	// SUBA
					size = ((this->cpu.instruction_reg & 0x0100) >> 7);
					value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
					if (size == 0x01)
						this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] -= (value & 0x0000FFFF);
					else
						this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] -= value;
				} else {
					if ((this->cpu.instruction_reg & 0xF130) == 0x9100)			// SUBX
						this->SUBXhandler();

					if (((this->cpu.instruction_reg & 0xF130) != 0xD100) && ((this->cpu.instruction_reg & 0xF000) == 0x9000))		// SUB
						this->SUBhandler();
				}
			}

			if ((this->cpu.instruction_reg & 0xF000) == 0xB000) {		// EOR, CMPM, CMP and CMPA
				if ((this->cpu.instruction_reg & 0xF0C0) == 0xB0C0)		// CMPA
					this->CMPAhandler();
				else if ((this->cpu.instruction_reg & 0xF138) == 0xB108)		// CMPM
					this->CMPMhandler();
				else if ((this->cpu.instruction_reg & 0xF100) == 0xB100)		// EOR
					this->EORhandler();
				else if ((this->cpu.instruction_reg & 0xF100) == 0xB000)		// CMP
					this->CMPhandler();
			}

			if ((this->cpu.instruction_reg & 0xF000) == 0xC000) {			// MULU, MULS, ABCD, EXG and AND
				if ((this->cpu.instruction_reg & 0xF1C0) == 0xC0C0)
					this->MULUhandler();		// MULU

				if ((this->cpu.instruction_reg & 0xF1C0) == 0xC1C0)
					this->MULShandler();		// MULS

				if ((this->cpu.instruction_reg & 0xF1F0) == 0xC100)
					this->ABCDhandler();		// ABCD

				if ((this->cpu.instruction_reg & 0xF130) == 0xC100)
					this->EXGhandler();			// EXG

				else if (((this->cpu.instruction_reg & 0xF000) == 0xC000) && ((this->cpu.instruction_reg & 0x00C0) != 0x00C0))
					this->ANDhandler();		// AND
			}

			if ((this->cpu.instruction_reg & 0xF000) == 0xD000) {		// ADD
				if ((this->cpu.instruction_reg & 0xF0C0) == 0xD0C0)
					this->ADDAhandler();		// ADDA
				else {
					if ((this->cpu.instruction_reg & 0xF130) == 0xD100)
						this->ADDXhandler();	// ADDX
					if ((this->cpu.instruction_reg & 0xF130) != 0xD100)
						this->ADDhandler();	// ADD
				}
			}

			if ((this->cpu.instruction_reg & 0xFCC0) == 0xE0C0)
				this->ASxLSx_handler();		// ASR / ASL / LSR / LSL register

			if (((this->cpu.instruction_reg & 0xF010) == 0xE000) && ((this->cpu.instruction_reg & 0x00C0) != 0x00C0))
				this->ASxLSx_handler();		// ASR / ASL / LSR / LSL immediate

			if ((this->cpu.instruction_reg & 0xFCC0) == 0xE4C0)
				this->ROxROXx_handler();		// ROR / ROL / ROXR / ROXL register

			if (((this->cpu.instruction_reg & 0xF010) == 0xE010) && ((this->cpu.instruction_reg & 0x00C0) != 0x00C0))
				this->ROxROXx_handler();		// ASR / ASL / LSR / LSL immediate
		
			if (this->DEBUG == true)
				WriteLog("\n\n");
		}

		if ((this->cpu.sr.flag.t0 == false) && (this->cpu.sr.flag.t1 == true))				// Generate a trace exception when the Trace bit is set in the Status Register
			this->pendingException = TRACE;

		Cycles--;								// Not cycle exact at the moment, but it works for now...
	}
}

unsigned char mc68k::readByte(unsigned int address) {
	unsigned char returnvalue;

	switch (this->Architecture) {
		case CASPER :
			if ((address >= obj_copro_casper->ROM_ADDR) && (address < obj_copro_casper->ROM_ADDR + (obj_copro_casper->ROM_SIZE)))
				return (obj_copro_casper->romMemory[address & (obj_copro_casper->ROM_SIZE - 1)]);
			if ((address > 0x00010000) && (address < 0x00010020)) {
				returnvalue = obj_copro_casper->parasite_via->ReadRegister((address & 0x0000001F) >> 1);
				return (returnvalue);
			}
			if ((address >= obj_copro_casper->RAM_ADDR) && (address < obj_copro_casper->RAM_ADDR + (obj_copro_casper->RAM_SIZE)))
				return (obj_copro_casper->ramMemory[address & (obj_copro_casper->RAM_SIZE - 1)]);
			return(0);
			break;

		case CISCOS :
			if (obj_copro_ciscos->BOOTFLAG == true) {
				if ((address & this->EXTERNAL_ADDRESS_MASK) >= (obj_copro_ciscos->ROM_ADDR & this->EXTERNAL_ADDRESS_MASK))
					obj_copro_ciscos->BOOTFLAG = false;					// Enable RAM
				return (obj_copro_ciscos->romMemory[address & (obj_copro_ciscos->ROM_SIZE - 1)]);
			} else {
				if ((address & this->EXTERNAL_ADDRESS_MASK) < (obj_copro_ciscos->TUBE_ULA_ADDR & this->EXTERNAL_ADDRESS_MASK))
					return (obj_copro_ciscos->ramMemory[address & (obj_copro_ciscos->RAM_SIZE - 1)]);
				if ((address & this->EXTERNAL_ADDRESS_MASK) >= (obj_copro_ciscos->ROM_ADDR & this->EXTERNAL_ADDRESS_MASK))
					return (obj_copro_ciscos->romMemory[address & (obj_copro_ciscos->ROM_SIZE - 1)]);
				return (ReadTubeFromParasiteSide(address & 0x00000007));
//				return obj_copro_ciscos->tube_ula->ReadParasiteRegister(address & 0x00000007);
			}
			break;

		case CUMANA :
			if ((address & this->EXTERNAL_ADDRESS_MASK) < 0x00001000)		// &0000-&0FFF
				return (BeebReadMem((address & 0xFFFF) + 0x2000));
			if (((address & this->EXTERNAL_ADDRESS_MASK) > 0x00001FFF) && ((address & this->EXTERNAL_ADDRESS_MASK) < 0x00003000))	// &2000-&2FFF
				return (BeebReadMem((address & 0xFFFF) - 0x2000));
			if ((address & this->EXTERNAL_ADDRESS_MASK) < 0x00010000)		// &1000-&1FFF/&3000-&FFFF - Regular BBC memory
				return (BeebReadMem(address & 0xFFFF));
			return (obj_copro_cumana->ramMemory[address & (obj_copro_cumana->RAM_SIZE - 1)]);
			break;

		default :
			return(0);
			break;
	}
}

unsigned short mc68k::readWord(unsigned int address) {
	unsigned short	temp;

	if ((address & 1) != 0)
		this->pendingException = ADDRESS_ERROR;

	temp = ((this->readByte(address) << 8) | (this->readByte(address+1)));
	return (temp);
}


unsigned int mc68k::readLong(unsigned int address) {
	unsigned long	temp;

	if ((address & 1) != 0)
		this->pendingException = ADDRESS_ERROR;

	temp = ((this->readByte(address) << 24) | (this->readByte(address+1) << 16) | (this->readByte(address+2) << 8) | (readByte(address+3)));
	return (temp);
}

void mc68k::writeByte(unsigned int address, unsigned char value) {
	switch (this->Architecture) {
		case CASPER :
			if ((address > 0x00010000) && (address < 0x00010020))
				obj_copro_casper->parasite_via->WriteRegister(((address & 0x0000001F) >> 1), value);
			if ((address >= obj_copro_casper->RAM_ADDR) && (address < obj_copro_casper->RAM_ADDR + (obj_copro_casper->RAM_SIZE)))
				obj_copro_casper->ramMemory[address & (obj_copro_casper->RAM_SIZE - 1)] = value;
			break;

		case CISCOS :
			if (obj_copro_ciscos->BOOTFLAG == true) {
				if ((address & this->EXTERNAL_ADDRESS_MASK) >= (obj_copro_ciscos->ROM_ADDR & this->EXTERNAL_ADDRESS_MASK))
					obj_copro_ciscos->BOOTFLAG = false;		// Enable RAM
				return;										// Cannot write to ROM
			} else {
				if ((address & this->EXTERNAL_ADDRESS_MASK) < (obj_copro_ciscos->TUBE_ULA_ADDR & this->EXTERNAL_ADDRESS_MASK)) {
					obj_copro_ciscos->ramMemory[address & (obj_copro_ciscos->RAM_SIZE-1)] = value;
					return;
				}
				if ((address & this->EXTERNAL_ADDRESS_MASK) >= (obj_copro_ciscos->ROM_ADDR & this->EXTERNAL_ADDRESS_MASK))
					return;									// Cannot write to ROM
				WriteTubeFromParasiteSide((address & this->EXTERNAL_ADDRESS_MASK), value);
//				obj_copro_ciscos->tube_ula->WriteParasiteRegister((address & 0x00000007), value);
				return;
			}
			break;

		case CUMANA :
			if ((address & this->EXTERNAL_ADDRESS_MASK) < 0x00001000) {		// &0000-&0FFF
				BeebWriteMem(((address + 0x2000) & 0xFFFF), value);
				return;
			}
			if (((address & this->EXTERNAL_ADDRESS_MASK) > 0x00001FFF) && ((address & this->EXTERNAL_ADDRESS_MASK) < 0x00003000)) {	// &2000-&2FFF
				BeebWriteMem(((address - 0x2000) & 0xFFFF), value);
				return;
			}
			if ((address & this->EXTERNAL_ADDRESS_MASK) < 0x00010000)			// &1000-&1FFF/&3000-&FFFF - Regular BBC memory
				BeebWriteMem(address & 0xFFFF, value);
			else
				obj_copro_cumana->ramMemory[address & (obj_copro_cumana->RAM_SIZE-1)] = value;
			break;

		default :
			break;
	}
}

void mc68k::writeWord(unsigned int address, unsigned short value) {
	if ((address & 1) != 0)
		this->pendingException = ADDRESS_ERROR;
	else {
		writeByte(address, ((value & 0xFF00)>>8));
		writeByte(address+1, (value & 0x00FF));
	}
}

void mc68k::writeLong(unsigned int address, unsigned int value) {
	if ((address & 1) != 0)
		this->pendingException = ADDRESS_ERROR;
	else {
		writeByte(address, ((value & 0xFF000000)>>24));
		writeByte(address+1, ((value & 0x00FF0000)>>16));
		writeByte(address+2, ((value & 0x0000FF00)>>8));
		writeByte(address+3, (value & 0x000000FF));
	}
}

unsigned int mc68k::getOperandValue(unsigned char Operand, unsigned char Size) {
	unsigned long	Displacement = 0;
	unsigned char	Index;

//	if (this->DEBUG == true)
//		WriteLog("mc68k::getOperandValue(this->cpu.pc=%08X, Operand=%02X, Size=%02X)\n", this->cpu.pc, Operand, Size);

	switch (Operand) {
		case 0x00 :		// D0
		case 0x01 :		// D1
		case 0x02 :		// D2
		case 0x03 :		// D3
		case 0x04 :		// D4
		case 0x05 :		// D5
		case 0x06 :		// D6
		case 0x07 :		// D7
			switch (Size) {
				case 0x00:
					return (this->cpu.d[Operand & 7] & 0xFF);
					break;
				case 0x01:
					return (this->cpu.d[Operand & 7] & 0xFFFF);
					break;
				case 0x02:
					return this->cpu.d[Operand & 7];
					break;
			}
			break;
		case 0x08 :		// A0
		case 0x09 :		// A1
		case 0x0A :		// A2
		case 0x0B :		// A3
		case 0x0C :		// A4
		case 0x0D :		// A5
		case 0x0E :		// A6
			switch (Size) {
				case 0x00:
					return (this->cpu.a[Operand & 7] & 0xFF);
					break;
				case 0x01:
					return (this->cpu.a[Operand & 7] & 0xFFFF);
					break;
				case 0x02:
					return this->cpu.a[Operand & 7];
					break;
			}
			break;
		case 0x0F :		// A7
			if (this->cpu.sr.flag.s) {
				switch (Size) {
					case 0x00:
						return (this->cpu.ssp & 0xFF);
						break;
					case 0x01:
						return (this->cpu.ssp & 0xFFFF);
						break;
					case 0x02:
						return (this->cpu.ssp);
						break;
				}
			} else {
				switch (Size) {
					case 0x00:
						return (this->cpu.usp & 0xFF);
						break;
					case 0x01:
						return (this->cpu.usp & 0xFFFF);
						break;
					case 0x02:
						return (this->cpu.usp);
						break;
				}
			}
			break;
		case 0x10 :		// (A0)
		case 0x11 :		// (A1)
		case 0x12 :		// (A2)
		case 0x13 :		// (A3)
		case 0x14 :		// (A4)
		case 0x15 :		// (A5)
		case 0x16 :		// (A6)
			switch (Size) {
				case 0x00:
					return (this->readByte(this->cpu.a[Operand & 7]));
					break;
				case 0x01:
					return (this->readWord(this->cpu.a[Operand & 7]));
					break;
				case 0x02:
					return (this->readLong(this->cpu.a[Operand & 7]));
					break;
			}
			break;
		case 0x17 :		// (SP)
			if (this->cpu.sr.flag.s) {
				switch (Size) {
					case 0x00:
						return (this->readByte(this->cpu.ssp));
						break;
					case 0x01:
						return (this->readWord(this->cpu.ssp));
						break;
					case 0x02:
						return (this->readLong(this->cpu.ssp));
						break;
				}
			} else {
				switch (Size) {
					case 0x00:
						return (this->readByte(this->cpu.usp));
						break;
					case 0x01:
						return (this->readWord(this->cpu.usp));
						break;
					case 0x02:
						return (this->readLong(this->cpu.usp));
						break;
				}
			}
			break;
		case 0x18 :		// (A0)+
		case 0x19 :		// (A1)+
		case 0x1A :		// (A2)+
		case 0x1B :		// (A3)+
		case 0x1C :		// (A4)+
		case 0x1D :		// (A5)+
		case 0x1E :		// (A6)+
			switch (Size) {
				case 0x00:
					this->cpu.a[Operand & 7] += 1;
					return (this->readByte(this->cpu.a[Operand & 7] - 1));
					break;
				case 0x01:
					this->cpu.a[Operand & 7] += 2;
					return (this->readWord(this->cpu.a[Operand & 7] - 2));
					break;
				case 0x02:
					this->cpu.a[Operand & 7] += 4;
					return (this->readLong(this->cpu.a[Operand & 7] - 4));
					break;
			}
			break;
		case 0x1F :		// (SP)+
			if (this->cpu.sr.flag.s) {
				switch (Size) {
					case 0x00:
						this->cpu.ssp++;
						return (this->readByte(this->cpu.ssp - 1));
						break;
					case 0x01:
						this->cpu.ssp += 2;
						return (this->readWord(this->cpu.ssp - 2));
						break;
					case 0x02:
						this->cpu.ssp += 4;
						return (this->readLong(this->cpu.ssp - 4));
						break;
				}
			} else {
				switch (Size) {
					case 0x00:
						this->cpu.usp++;
						return (this->readByte(this->cpu.usp - 1));
						break;
					case 0x01:
						this->cpu.usp += 2;
						return (this->readWord(this->cpu.usp - 2));
						break;
					case 0x02:
						this->cpu.usp += 4;
						return (this->readLong(this->cpu.usp - 4));
						break;
				}
			}
			break;
		case 0x20 :		// -(A0)
		case 0x21 :		// -(A1)
		case 0x22 :		// -(A2)
		case 0x23 :		// -(A3)
		case 0x24 :		// -(A4)
		case 0x25 :		// -(A5)
		case 0x26 :		// -(A6)
			switch (Size) {
				case 0x00:
					this->cpu.a[Operand & 7]--;
					return (this->readByte(this->cpu.a[Operand & 7]));
					break;
				case 0x01:
					this->cpu.a[Operand & 7]-=2;
					return (this->readWord(this->cpu.a[Operand & 7]));
					break;
				case 0x02:
					this->cpu.a[Operand & 7]-=4;
					return (this->readLong(this->cpu.a[Operand & 7]));
					break;
			}
			break;
		case 0x27 :		// -(SP)
			if (this->cpu.sr.flag.s) {
				switch (Size) {
					case 0x00:
						this->cpu.ssp--;
						return (this->readByte(this->cpu.ssp));
						break;
					case 0x01:
						this->cpu.ssp -= 2;
						return (this->readWord(this->cpu.ssp));
						break;
					case 0x02:
						this->cpu.ssp -= 4;
						return (this->readLong(this->cpu.ssp));
						break;
				}
			} else {
				switch (Size) {
					case 0x00:
						this->cpu.usp--;
						return (this->readByte(this->cpu.usp));
						break;
					case 0x01:
						this->cpu.usp -= 2;
						return (this->readWord(this->cpu.usp));
						break;
					case 0x02:
						this->cpu.usp -= 4;
						return (this->readLong(this->cpu.usp));
						break;
				}
			}
			break;
		case 0x28 :		// (A0 + Displacement)
		case 0x29 :		// (A1 + Displacement)
		case 0x2A :		// (A2 + Displacement)
		case 0x2B :		// (A3 + Displacement)
		case 0x2C :		// (A4 + Displacement)
		case 0x2D :		// (A5 + Displacement)
		case 0x2E :		// (A6 + Displacement)
			Displacement = this->readWord(this->cpu.pc);
			this->cpu.pc += 2;
			switch (Size) {
				case 0x00:
					if (Displacement & 0x8000)
						return(this->readByte(this->cpu.a[Operand & 7] - (~Displacement + 1)));
					else
						return(this->readByte(this->cpu.a[Operand & 7] + Displacement));
					break;
				case 0x01:
					if (Displacement & 0x8000)
						return(this->readWord(this->cpu.a[Operand & 7] - (~Displacement + 1)));
					else
						return(this->readWord(this->cpu.a[Operand & 7] + Displacement));
					break;
				case 0x02:
					if (Displacement & 0x8000)
						return(this->readLong(this->cpu.a[Operand & 7] - (~Displacement + 1)));
					else
						return(this->readLong(this->cpu.a[Operand & 7] + Displacement));
					break;
			}
			break;
		case 0x2F :		// (SP + Displacement)
			Displacement = this->readWord(this->cpu.pc);
			this->cpu.pc += 2;
			if (this->cpu.sr.flag.s) {
				switch (Size) {
					case 0x00:
						if (Displacement & 0x8000)
							return(this->readByte(this->cpu.ssp - (~Displacement + 1)));
						else
							return(this->readByte(this->cpu.ssp + Displacement));
						break;
					case 0x01:
						if (Displacement & 0x8000)
							return(this->readWord(this->cpu.ssp - (~Displacement + 1)));
						else
							return(this->readWord(this->cpu.ssp + Displacement));
						break;
					case 0x02:
						if (Displacement & 0x8000)
							return(this->cpu.ssp - (~Displacement + 1));
						else
							return(this->cpu.ssp + Displacement);
						break;
				}
			} else {
				switch (Size) {
					case 0x00:
						if (Displacement & 0x8000)
							return(this->readByte(this->cpu.usp - (~Displacement + 1)));
						else
							return(this->readByte(this->cpu.usp + Displacement));
						break;
					case 0x01:
						if (Displacement & 0x8000)
							return(this->readWord(this->cpu.usp - (~Displacement + 1)));
						else
							return(this->readWord(this->cpu.usp + Displacement));
						break;
					case 0x02:
						if (Displacement & 0x8000)
							return(this->readLong(this->cpu.usp - (~Displacement + 1)));
						else
							return(this->readLong(this->cpu.usp + Displacement));
						break;
				}
			}
			break;
		case 0x30 :		// (A0 + Xn + Displacement)
		case 0x31 :		// (A1 + Xn + Displacement)
		case 0x32 :		// (A2 + Xn + Displacement)
		case 0x33 :		// (A3 + Xn + Displacement)
		case 0x34 :		// (A4 + Xn + Displacement)
		case 0x35 :		// (A5 + Xn + Displacement)
		case 0x36 :		// (A6 + Xn + Displacement)
			Index = this->readByte(this->cpu.pc);
			this->cpu.pc++;
			Displacement = this->readByte(this->cpu.pc);
			this->cpu.pc++;

			if (Displacement & 0x0080)
				Displacement = (0xFFFFFF00 | (Displacement & 0x00FF));
			else
				Displacement = (Displacement & 0x00FF);

			if (Index & 0x80)
				Displacement += this->cpu.a[((Index & 0x70) >> 4)];
			else
				Displacement += this->cpu.d[((Index & 0x70) >> 4)];

			switch (Size) {
				case 0x00:
					return(this->readByte(this->cpu.a[Operand & 7] + Displacement));
					break;
				case 0x01:
					return(this->readWord(this->cpu.a[Operand & 7] + Displacement));
					break;
				case 0x02:
					return(this->readLong(this->cpu.a[Operand & 7] + Displacement));
					break;
			}
			break;
		case 0x37 :		// (A7 + Xn + Displacement)
			Index = this->readByte(this->cpu.pc);
			this->cpu.pc++;
			Displacement = this->readByte(this->cpu.pc);
			this->cpu.pc++;

			if (Displacement & 0x0080)
				Displacement = (0xFFFFFF00 | (Displacement & 0x00FF));
			else
				Displacement = (Displacement & 0x00FF);

			if (Index & 0x80)
				Displacement += this->cpu.a[((Index & 0x70) >> 4)];
			else
				Displacement += this->cpu.d[((Index & 0x70) >> 4)];

			if (this->cpu.sr.flag.s) {
				switch (Size) {
					case 0x00:
						return(this->readByte(this->cpu.ssp + Displacement));
						break;
					case 0x01:
						return(this->readWord(this->cpu.ssp + Displacement));
						break;
					case 0x02:
						return(this->readLong(this->cpu.ssp + Displacement));
						break;
				}
			} else {
				switch (Size) {
					case 0x00:
						return(this->readByte(this->cpu.usp + Displacement));
						break;
					case 0x01:
						return(this->readWord(this->cpu.usp + Displacement));
						break;
					case 0x02:
						return(this->readLong(this->cpu.usp + Displacement));
						break;
				}
			}
			break;
		case 0x38 :		// (xxx).W
			this->cpu.pc += 2;
			switch (Size) {
				case 0x00 :
					return (this->readByte(this->signExtend(this->readWord(this->cpu.pc-2))));
					break;
				case 0x01 :
					return (this->readWord(this->signExtend(this->readWord(this->cpu.pc-2))));
					break;
				case 0x02 :
					return (this->readLong(this->signExtend(this->readWord(this->cpu.pc-2))));
					break;
			}
			break;
		case 0x39 :		// (xxx).L
			this->cpu.pc += 4;
			switch (Size) {
				case 0x00 :
					return (this->readByte(this->readLong(this->cpu.pc-4)));
					break;
				case 0x01 :
					return (this->readWord(this->readLong(this->cpu.pc-4)));
					break;
				case 0x02 :
					return (this->readLong(this->readLong(this->cpu.pc-4)));
					break;
			}
			break;
		case 0x3A :		// (PC + Displacement)
			Displacement = this->signExtend(this->readWord(this->cpu.pc));
			this->cpu.pc += 2;
			switch (Size) {
				case 0x00:
					return(this->readByte(this->cpu.pc - 2 + Displacement));
					break;
				case 0x01:
					return(this->readWord(this->cpu.pc - 2 + Displacement));
					break;
				case 0x02:
					return(this->readLong(this->cpu.pc - 2 + Displacement));
					break;
			}
			break;
		case 0x3B :		// (d8, this->cpu.pc, Xn)
			Displacement = this->readWord(this->cpu.pc);
			this->cpu.pc += 2;
			return (this->cpu.pc + 2 + Displacement);
			break;
		case 0x3C :		// Immediate #
			switch (Size) {
				case 0x00:
					this->cpu.pc += 2;
					return (this->readWord(this->cpu.pc-2) & 0x00FF);
					break;
				case 0x01:
					this->cpu.pc += 2;
					return (this->readWord(this->cpu.pc-2));
					break;
				case 0x02:
					this->cpu.pc += 4;
					return (this->readLong(this->cpu.pc-4));
					break;
			}
			break;
		default :
			this->pendingException=ILLEGAL_INSTRUCTION;
			return(0);
			break;
			}
	return(0);
}

void mc68k::setOperandValue(unsigned char Operand, unsigned char Size, unsigned int Value) {
	short	Displacement;
//	WriteLog("this->setOperandValue(this->cpu.pc=%08X, Operand=%02X, Size=%02X, Value=%08X)\n", this->cpu.pc, Operand, Size, Value);

	switch (Operand) {
		case 0x00 :		// D0
		case 0x01 :		// D1
		case 0x02 :		// D2
		case 0x03 :		// D3
		case 0x04 :		// D4
		case 0x05 :		// D5
		case 0x06 :		// D6
		case 0x07 :		// D7
			switch (Size) {
				case 0x00 :
					this->cpu.d[Operand & 7] &= 0xFFFFFF00;
					this->cpu.d[Operand & 7] |= (Value & 0xFF);
					return;
					break;
				case 0x01 :
					this->cpu.d[Operand & 7] &= 0xFFFF0000;
					this->cpu.d[Operand & 7] |= (Value & 0xFFFF);
					return;
					break;
				case 0x02 :
					this->cpu.d[Operand & 7] = Value;
					return;
					break;
			}
			break;
		case 0x08 :		// A0
		case 0x09 :		// A1
		case 0x0A :		// A2
		case 0x0B :		// A3
		case 0x0C :		// A4
		case 0x0D :		// A5
		case 0x0E :		// A6
			switch (Size) {
				case 0x00:
					this->cpu.a[Operand & 7] = ((this->cpu.a[Operand & 7] & 0xFFFFFF00) | (Value & 0xFF));
					return;
					break;
				case 0x01:
					this->cpu.a[Operand & 7] = ((this->cpu.a[Operand & 7] & 0xFFFF0000) | (Value & 0xFFFF));
					return;
					break;
				case 0x02:
					this->cpu.a[Operand & 7] = Value;
					return;
					break;
			}
			break;
		case 0x0F :		// A7
			if (this->cpu.sr.flag.s) {
				switch (Size) {
					case 0x00:
						((this->cpu.ssp &= 0xFFFFFF00) | (Value & 0xFF));
						return;
						break;
					case 0x01:
						((this->cpu.ssp &= 0xFFFF0000) | (Value & 0xFFFF));
						return;
						break;
					case 0x02:
						this->cpu.ssp = Value;
						return;
						break;
				}
			} else {
				switch (Size) {
					case 0x00:
						((this->cpu.usp &= 0xFFFFFF00) | (Value & 0xFF));
						return;
						break;
					case 0x01:
						((this->cpu.usp &= 0xFFFF0000) | (Value & 0xFFFF));
						return;
						break;
					case 0x02:
						this->cpu.usp = Value;
						return;
						break;
				}
			}
			break;
		case 0x10 :		// (A0)
		case 0x11 :		// (A1)
		case 0x12 :		// (A2)
		case 0x13 :		// (A3)
		case 0x14 :		// (A4)
		case 0x15 :		// (A5)
		case 0x16 :		// (A6)
			switch (Size) {
				case 0x00:
					this->writeByte(this->cpu.a[Operand & 7], Value & 0xFF);
					return;
					break;
				case 0x01:
					this->writeWord(this->cpu.a[Operand & 7], Value & 0xFFFF);
					return;
					break;
				case 0x02:
					this->writeLong(this->cpu.a[Operand & 7], Value);
					return;
					break;
			}
			break;
		case 0x17 :		// (A7)
			if (this->cpu.sr.flag.s) {
				switch (Size) {
					case 0x00:
						this->writeByte(this->cpu.ssp, Value & 0xFF);
						return;
						break;
					case 0x01:
						this->writeWord(this->cpu.ssp, Value & 0xFFFF);
						return;
						break;
					case 0x02:
						this->writeLong(this->cpu.ssp, Value);
						return;
						break;
				}
			} else {
				switch (Size) {
					case 0x00:
						this->writeByte(this->cpu.usp, Value & 0xFF);
						return;
						break;
					case 0x01:
						this->writeWord(this->cpu.usp, Value & 0xFFFF);
						return;
						break;
					case 0x02:
						this->writeLong(this->cpu.usp, Value);
						return;
						break;
				}
			}
			break;
		case 0x18 :		// (A0)+
		case 0x19 :		// (A1)+
		case 0x1A :		// (A2)+
		case 0x1B :		// (A3)+
		case 0x1C :		// (A4)+
		case 0x1D :		// (A5)+
		case 0x1E :		// (A6)+
			switch (Size) {
				case 0x00:
					this->writeByte(this->cpu.a[Operand & 7], Value & 0xFF);
					this->cpu.a[Operand & 7]++;
					break;
				case 0x01:
					this->writeWord(this->cpu.a[Operand & 7], Value & 0xFFFF);
					this->cpu.a[Operand & 7] += 2;
					break;
				case 0x02:
					this->writeLong(this->cpu.a[Operand & 7], Value);
					this->cpu.a[Operand & 7] += 4;
					break;
			}
			break;
		case 0x1F :		// (A7)+
			if (this->cpu.sr.flag.s) {
				switch (Size) {
					case 0x00:
						this->writeByte(this->cpu.ssp, Value & 0xFF);
						this->cpu.ssp++;
						break;
					case 0x01:
						this->writeWord(this->cpu.ssp, Value & 0xFFFF);
						this->cpu.ssp += 2;
						break;
					case 0x02:
						this->writeLong(this->cpu.ssp, Value);
						this->cpu.ssp += 4;
						break;
				}
			} else {
				switch (Size) {
					case 0x00:
						this->writeByte(this->cpu.usp, Value & 0xFF);
						this->cpu.usp++;
						break;
					case 0x01:
						this->writeWord(this->cpu.usp, Value & 0xFFFF);
						this->cpu.usp += 2;
						break;
					case 0x02:
						this->writeLong(this->cpu.usp, Value);
						this->cpu.usp += 4;
						break;
				}
			}
			break;
		case 0x20 :		// -(A0)
		case 0x21 :		// -(A1)
		case 0x22 :		// -(A2)
		case 0x23 :		// -(A3)
		case 0x24 :		// -(A4)
		case 0x25 :		// -(A5)
		case 0x26 :		// -(A6)
			switch (Size) {
				case 0x00:
					this->cpu.a[Operand & 7] -= 1;
					this->writeByte(this->cpu.a[Operand & 7], Value & 0xFF);
					break;
				case 0x01:
					this->cpu.a[Operand & 7] -= 2;
					this->writeWord(this->cpu.a[Operand & 7], Value & 0xFFFF);
					break;
				case 0x02:
					this->cpu.a[Operand & 7] -= 4;
					this->writeLong(this->cpu.a[Operand & 7], Value);
					break;
			}
			break;
		case 0x27 :		// -(A7)
			if (this->cpu.sr.flag.s) {
				switch (Size) {
					case 0x00:
						this->cpu.ssp -= 1;
						this->writeByte(this->cpu.ssp, Value & 0xFF);
						break;
					case 0x01:
						this->cpu.ssp -= 2;
						this->writeWord(this->cpu.ssp, Value & 0xFFFF);
						break;
					case 0x02:
						this->cpu.ssp -= 4;
						this->writeLong(this->cpu.ssp, Value);
						break;
				}
			} else {
				switch (Size) {
					case 0x00:
						this->cpu.usp -= 1;
						this->writeByte(this->cpu.usp, Value & 0xFF);
						break;
					case 0x01:
						this->cpu.usp -= 2;
						this->writeWord(this->cpu.usp, Value & 0xFFFF);
						break;
					case 0x02:
						this->cpu.usp -= 4;
						this->writeLong(this->cpu.usp, Value);
						break;
				}
			}
			break;
		case 0x28 :		// (A0 + Displacement)
		case 0x29 :		// (A1 + Displacement)
		case 0x2A :		// (A2 + Displacement)
		case 0x2B :		// (A3 + Displacement)
		case 0x2C :		// (A4 + Displacement)
		case 0x2D :		// (A5 + Displacement)
		case 0x2E :		// (A6 + Displacement)
			Displacement = this->signExtend(this->readWord(this->cpu.pc));
			this->cpu.pc += 2;
			switch (Size) {
				case 0x00:
					this->writeByte((this->cpu.a[Operand & 7] + Displacement), Value & 0xFF);
					break;
				case 0x01:
					this->writeWord((this->cpu.a[Operand & 7] + Displacement), Value & 0xFFFF);
					break;
				case 0x02:
					this->writeLong((this->cpu.a[Operand & 7] + Displacement), Value);
					break;
			}
			break;
		case 0x2F :		// (A7 + Displacement)
			Displacement = this->signExtend(this->readWord(this->cpu.pc));
			this->cpu.pc += 2;
			if (this->cpu.sr.flag.s) {
				switch (Size) {
					case 0x00:
						this->writeByte((this->cpu.ssp + Displacement), Value & 0xFF);
						break;
					case 0x01:
						this->writeWord((this->cpu.ssp + Displacement), Value & 0xFFFF);
						break;
					case 0x02:
						this->writeLong((this->cpu.ssp + Displacement), Value);
						break;
				}
			} else {
				switch (Size) {
					case 0x00:
						this->writeByte((this->cpu.usp + Displacement), Value & 0xFF);
						break;
					case 0x01:
						this->writeWord((this->cpu.usp + Displacement), Value & 0xFFFF);
						break;
					case 0x02:
						this->writeLong((this->cpu.usp + Displacement), Value);
						break;
				}
			}
			break;
		case 0x30 :		// (A0 + Xn + Displacement)
		case 0x31 :		// (A1 + Xn + Displacement)
		case 0x32 :		// (A2 + Xn + Displacement)
		case 0x33 :		// (A3 + Xn + Displacement)
		case 0x34 :		// (A4 + Xn + Displacement)
		case 0x35 :		// (A5 + Xn + Displacement)
		case 0x36 :		// (A6 + Xn + Displacement)
			Displacement = this->readWord(this->signExtend(this->readWord(this->cpu.pc)));
			this->cpu.pc += 2;
			switch (Size) {
				case 0x00:
					this->writeByte((this->cpu.a[Operand & 7] + Displacement), Value & 0xFF);
					this->cpu.a[Operand & 7]++;
					break;
				case 0x01:
					this->writeWord((this->cpu.a[Operand & 7] + Displacement), Value & 0xFFFF);
					this->cpu.a[Operand & 7]+=2;
					break;
				case 0x02:
					this->writeLong((this->cpu.a[Operand & 7] + Displacement), Value);
					this->cpu.a[Operand & 7]+=4;
					break;
			}
			break;
		case 0x37 :		// (A7 + Xn + Displacement)
			if (this->cpu.sr.flag.s) {
				Displacement = this->readWord(this->signExtend(this->readWord(this->cpu.pc)));
				switch (Size) {
					case 0x00:
						this->writeByte((this->cpu.ssp + Displacement), Value & 0xFF);
						this->cpu.ssp++;
						break;
					case 0x01:
						this->writeWord((this->cpu.ssp + Displacement), Value & 0xFFFF);
						this->cpu.ssp += 2;
						break;
					case 0x02:
						this->writeLong((this->cpu.ssp + Displacement), Value);
						this->cpu.ssp += 4;
						break;
				}
			} else {
				Displacement = this->readWord(this->signExtend(this->readWord(this->cpu.pc)));
				switch (Size) {
					case 0x00:
						this->writeByte((this->cpu.usp + Displacement), Value & 0xFF);
						this->cpu.usp++;
						break;
					case 0x01:
						this->writeWord((this->cpu.usp + Displacement), Value & 0xFFFF);
						this->cpu.usp += 2;
						break;
					case 0x02:
						this->writeLong((this->cpu.usp + Displacement), Value);
						this->cpu.usp += 4;
						break;
				}
			}
			this->cpu.pc += 2;
			break;
		case 0x38 :		// (xxx).W
			switch (Size) {
				case 0x00 :
					writeByte(this->signExtend(this->readWord(this->cpu.pc)), Value);
					break;
				case 0x01 :
					writeWord(this->signExtend(this->readWord(this->cpu.pc)), Value);
					break;
				case 0x02 :
					writeLong(this->signExtend(this->readWord(this->cpu.pc)), Value);
					break;
			}
			this->cpu.pc += 2;
			break;
		case 0x39 :		// (xxx).L
			switch (Size) {
				case 0x00 :
					writeByte(this->readLong(this->cpu.pc), Value);
					break;
				case 0x01 :
					writeWord(this->readLong(this->cpu.pc), Value);
					break;
				case 0x02 :
					writeLong(this->readLong(this->cpu.pc), Value);
					break;
			}
			this->cpu.pc += 4;
			break;
		case 0x3A :		// (this->cpu.pc + Displacement)
			Displacement = this->signExtend(this->readWord(this->cpu.pc));
			this->cpu.pc += 2;
			switch (Size) {
				case 0x00:
					this->writeByte(this->cpu.pc - 2 + Displacement, Value & 0xFF);
					return;
					break;
				case 0x01:
					this->writeWord(this->cpu.pc - 2 + Displacement, Value & 0xFFFF);
					return;
					break;
				case 0x02:
					this->writeLong(this->cpu.pc - 2 + Displacement, Value);
					return;
					break;
			}
			break;
		case 0x3B :		// (d8, this->cpu.pc, Xn)
			this->cpu.pc += 2;
			Displacement = this->readWord(this->cpu.pc);
			switch (Size) {
				case 0x00:
					this->writeByte(this->cpu.pc - 2 + Displacement, Value & 0xFF);
					return;
					break;
				case 0x01:
					this->writeWord(this->cpu.pc - 2 + Displacement, Value & 0xFFFF);
					return;
					break;
				case 0x02:
					this->writeLong(this->cpu.pc - 2 + Displacement, Value);
					return;
					break;
			}
			break;
		case 0x3C :		// Immediate #
			break;
		default :
			this->pendingException=ILLEGAL_INSTRUCTION;
			break;
	}
}

void mc68k::rewindPC(unsigned char Operand) {	// In case when a value is read with displacement, and is written with the same displacement, the pc must be decreased by 2 or 4
	switch (Operand) {
		case 0x28 :
		case 0x29 :
		case 0x2A :
		case 0x2B :
		case 0x2C :
		case 0x2D :
		case 0x2E :
		case 0x2F :
		case 0x30 :
		case 0x31 :
		case 0x32 :
		case 0x33 :
		case 0x34 :
		case 0x35 :
		case 0x36 :
		case 0x37 :
		case 0x38 :
		case 0x3A :
		case 0x3B :
			this->cpu.pc -= 2;
			break;

		case 0x39 :
			this->cpu.pc -= 4;
			break;

		default :
			break;
	}
}

bool mc68k::evaluateConditionCode(unsigned char ConditionCode) {
	switch (ConditionCode) {
		case 0x00 :	// T (true)
			return (true);
			break;
		case 0x01 : // F (false)
			return (false);
			break;
		case 0x02 : // HI (Higher)
			return ((this->cpu.sr.flag.c == 0) && (this->cpu.sr.flag.z == 0));
			break;
		case 0x03 : // LS (Lower or same)
			return ((this->cpu.sr.flag.c == 1) || (this->cpu.sr.flag.z == 1));
			break;
		case 0x04 : // CC (Carry Clear)
			return (this->cpu.sr.flag.c == 0);
			break;
		case 0x05 :	// CS (Carry Set)
			return (this->cpu.sr.flag.c == 1);
			break;
		case 0x06 :	// NE (Not Equal)
			return (this->cpu.sr.flag.z == 0);
			break;
		case 0x07 : // EQ (Equal)
			return (this->cpu.sr.flag.z == 1);
			break;
		case 0x08 :	// VC (Overflow Clear)
			return (this->cpu.sr.flag.v == 0);
			break;
		case 0x09 : // VS (Overflow Set)
			return (this->cpu.sr.flag.v == 1);
			break;
		case 0x0A :	// PL (Plus)
			return (this->cpu.sr.flag.n == 0);
			break;
		case 0x0B :	// MI (Minus)
			return (this->cpu.sr.flag.n == 1);
			break;
		case 0x0C :	// GE (Greater or Equal)
			return (((this->cpu.sr.flag.n == 1) && (this->cpu.sr.flag.v == 1)) || ((this->cpu.sr.flag.n == 0) && (this->cpu.sr.flag.v == 0)));
			break;
		case 0x0D :	// LT (Less Than)
			return (((this->cpu.sr.flag.n == 1) && (this->cpu.sr.flag.v == 0)) || ((this->cpu.sr.flag.n == 0) && (this->cpu.sr.flag.v == 1)));
			break;
		case 0x0E :	// GT (Greater Than)
			return (((this->cpu.sr.flag.n == 1) && (this->cpu.sr.flag.v == 1) && (this->cpu.sr.flag.z == 0)) || ((this->cpu.sr.flag.n == 0) && (this->cpu.sr.flag.v == 0) && (this->cpu.sr.flag.z == 0)));
			break;
		case 0x0F :	// LE (Less or Equal)
			return ((this->cpu.sr.flag.z == 1) || ((this->cpu.sr.flag.n == 1) && (this->cpu.sr.flag.v == 0)) || ((this->cpu.sr.flag.n == 0) && (this->cpu.sr.flag.v == 1)));
			break;
	}
	return (0);
}

unsigned int mc68k::signExtend(unsigned short value) {
	if (value & 0x8000)
		return(0xFFFF0000 | value);
	else
		return(0x00000000 | value);
}

void mc68k::dumpMemory(void) {
	FILE *fp;
	char path[256];

	strcpy(path, "C:\\Users\\Virustest\\Desktop\\BeebEm414_mc68k\\BeebEm\\68kMemoryDump.bin");

	fp = fopen(path, "rb");
	if (fp) {
		fclose(fp);	// Memory dump file already exists, so don't overwrite
	} else {
		fp = fopen(path, "wb");
		for (unsigned int i=0;i < (this->EXTERNAL_ADDRESS_MASK + 1); i++)
			fputc(this->readByte(i), fp);
		fclose(fp);
		if (this->DEBUG)
			WriteLog("mc68k::dumpMemory - memory dumped to %s\n", path);
	}
}

void mc68k::dumpRegisters(void) {
	char		output[17];
//	unsigned int	a7;
	int		i;

	for (i=0; i<16; i++)
		if ((0x8000 >> i) & this->cpu.sr.reg)
			output[i] = '1';
		else
			output[i] = '0';
	output[16] = 0x00;

//	if (this->cpu.sr.flag.s)
//		a7 = this->cpu.ssp;
//	else
//		a7 = this->cpu.usp;

	if (this->DEBUG) {

//		WriteLog("%08X   %04X   A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X USP=%08X SSP=%08X D0=%08X D1=%08X D2=%08X D3=%08X D4=%08X D5=%08X D6=%08X D7=%08X SR=%04X\n", this->cpu.pc, this->cpu.instruction_reg, this->cpu.a[0], this->cpu.a[1], this->cpu.a[2], this->cpu.a[3], this->cpu.a[4], this->cpu.a[5], this->cpu.a[6], this->cpu.usp, this->cpu.ssp, this->cpu.d[0], this->cpu.d[1], this->cpu.d[2], this->cpu.d[3], this->cpu.d[4], this->cpu.d[5], this->cpu.d[6], this->cpu.d[7], this->cpu.sr.reg);
		WriteLog("D0=%08X D4=%08X A0=%08X A4=%08X    T_S__INT___XNZVC\n", this->cpu.d[0], this->cpu.d[4], this->cpu.a[0], this->cpu.a[4]);
		WriteLog("D1=%08X D5=%08X A1=%08X A5=%08X SR=%s\n", this->cpu.d[1], this->cpu.d[5], this->cpu.a[1], this->cpu.a[5], output);
		WriteLog("D2=%08X D6=%08X A2=%08X A6=%08X US=%08X\n", this->cpu.d[2], this->cpu.d[6], this->cpu.a[2], this->cpu.a[6], this->cpu.usp);
		WriteLog("D3=%08X D7=%08X A3=%08X A7=%08X SS=%08X\n", this->cpu.d[3], this->cpu.d[7], this->cpu.a[3], (this->cpu.sr.flag.s ? this->cpu.ssp : this->cpu.usp), this->cpu.ssp);
		WriteLog("PC=%08X  Code=%04X", this->cpu.pc, this->cpu.instruction_reg);
	}
}

void mc68k::ABCDhandler(void) {
	unsigned int	value, value2, result, result2;

	if (this->cpu.instruction_reg & 0x0008) {				// -(An) memory to memory operation
		value = this->getOperandValue(((this->cpu.instruction_reg & 0x0007) | 0x0008), 0x00);			// Source value
		value2 = this->getOperandValue((((this->cpu.instruction_reg & 0x0E00) >> 9) | 0x0008), 0x00);	// Destination value
	} else {											// Dn to Dn operation
		value = this->cpu.d[(this->cpu.instruction_reg & 0x0007)];
		value2 = this->cpu.d[((this->cpu.instruction_reg & 0x0E00) >> 9)];
	}

	if ((value & 0x0F) > 9)
		result = 9;
	else
		result = (value & 0x0F);
	if (((value & 0xF0) >> 4) > 9)
		result += 90;
	else
		result += (((value & 0xF0) >> 4) * 10);

	if ((value2 & 0x0F) > 9)
		result2 = 9;
	else
		result2 = (value2 & 0x0F);
	if (((value2 & 0xF0) >> 4) > 9)
		result2 += 90;
	else
		result2 += (((value2 & 0xF0) >> 4) * 10);

	result = result2 + result;
	if (this->cpu.sr.flag.x)	// check for X flag
		result++;
	if (result > 99) {
		result += 0x60;
		this->cpu.sr.flag.c = true;
		this->cpu.sr.flag.x = true;
	}
	this->cpu.sr.flag.z = this->cpu.sr.flag.z & (result == 0);
	if (this->cpu.instruction_reg & 0x0008)					// -(An) memory to memory operation
		this->setOperandValue((((this->cpu.instruction_reg & 0x0E00) >> 9) | 0x0008), 0x00, ((result % 10) + ((result / 10) << 4)));
	else
		this->cpu.d[((this->cpu.instruction_reg & 0x0E00) >> 9)] = result;
}

void mc68k::ADDhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
	switch (size) {
		case 0x00 :
			if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
				result = (value & 0xFFFFFF00) | ((value & 0x000000FF) + (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF));
			else									// Destination is Dx
				result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFFFF00) | ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF) + (value & 0x000000FF));
			this->cpu.sr.flag.c = (((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&0x80)==0) && (((value|result)&0x80)!=0)) || ((value&result&0x80)!=0);
			this->cpu.sr.flag.v = ((0x80&(value^0x80)&this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&(result^0x80))!=0)||((0x80&result&(this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]^0x80)&value)!=0);
			this->cpu.sr.flag.z = ((result & 0x000000FF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00000080) == 0x80);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			break;
		case 0x01 :
			if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
				result = (value & 0xFFFF0000) | ((value & 0x0000FFFF) + (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF));
			else									// Destination is Dx
				result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFF0000) | ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF) + (value & 0x0000FFFF));
			this->cpu.sr.flag.c = (((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&0x8000)==0) && (((value|result)&0x8000)!=0)) || ((value&result&0x8000)!=0);
			this->cpu.sr.flag.v = ((0x8000&(value^0x8000)&this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&(result^0x8000))!=0)||((0x8000&result&(this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]^0x8000)&value)!=0);
			this->cpu.sr.flag.z = ((result & 0x0000FFFF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			break;
		case 0x02 :
			if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
				result = (value + this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]);
			else									// Destination is Dx
				result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] + value);
			this->cpu.sr.flag.c = (((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&0x80000000)==0) && (((value|result)&0x80000000)!=0)) || ((value&result&0x80000000)!=0);
			this->cpu.sr.flag.v = ((0x80000000&(value^0x80000000)&this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&(result^0x80000000))!=0)||((0x80000000&result&(this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]^0x80000000)&value)!=0);
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = (result & 0x80000000) == 0x80000000;
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
	if (this->cpu.instruction_reg & 0x0100) {		// Destination is operand
		this->rewindPC(this->cpu.instruction_reg & 0x38);
		this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
	} else
		this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = result;
}

void mc68k::ADDAhandler(void) {
	unsigned char	size;
	unsigned int	value;

	size = ((this->cpu.instruction_reg & 0x0100) >> 7);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
	if (size == 0x01)
		this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] += (value & 0x0000FFFF);
	else
		this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] += value;
}

void mc68k::ADDIhandler(void) {
	unsigned char	size;
	unsigned int	start;
	unsigned int	parameter1;
	unsigned int	result;
	char		sbyte;
	short		sword;
	int			slong;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	start = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
	switch (size) {
		case 0x00 :					// ADDI.B
			parameter1 = (this->readWord(this->cpu.pc) & 0xFF);
			this->cpu.pc += 2;
			if (this->DEBUG)
				WriteLog(" %02X", parameter1);
			sbyte = ((start & 0x000000FF) + parameter1);
			this->cpu.sr.flag.c = (((sbyte & 0x80) == 0) && (((start|sbyte) & 0x80) != 0)) || ((start&sbyte&0x80) != 0);
			this->cpu.sr.flag.v = ((start & 0x80 & (unsigned char)parameter1 & (sbyte ^ 0x80)) != 0) || ((0x80 & sbyte & (parameter1 ^ 0x80) & (start ^ 0x80)) !=0);
			this->cpu.sr.flag.z = ((sbyte & 0xFF) == 0);
			this->cpu.sr.flag.n = ((sbyte & 0x80) == 0x80);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			result = ((start & 0xFFFFFF00) | sbyte);
			break;
		case 0x01 :					// ADDI.W
			parameter1 = this->readWord(this->cpu.pc);
			this->cpu.pc += 2;
			if (this->DEBUG)
				WriteLog(" %04X", parameter1);
			sword = ((start & 0x0000FFFF) + parameter1);
			this->cpu.sr.flag.c = (((sword & 0x8000) == 0) && (((start|sword) & 0x8000) != 0)) || ((start&sword&0x8000) != 0);
			this->cpu.sr.flag.v = ((start & 0x8000 & (unsigned char)parameter1 & (sword ^ 0x8000)) != 0) || ((0x8000 & sword & (parameter1 ^ 0x8000) & (start ^ 0x80)) !=0);
			this->cpu.sr.flag.z = ((sword & 0x0000FFFF) == 0);
			this->cpu.sr.flag.n = ((sword & 0x00008000) == 0x8000);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			result = ((start & 0xFFFF0000) | sword);
			break;
		case 0x02 :					// ADDI.L
			parameter1 = this->readLong(this->cpu.pc);
			this->cpu.pc += 4;
			if (this->DEBUG)
				WriteLog(" %08X", parameter1);
			slong = (start + parameter1);
			this->cpu.sr.flag.c = (((slong & 0x80000000) == 0) && (((start|slong) & 0x80000000) != 0)) || ((start&slong&0x80000000) != 0);
			this->cpu.sr.flag.v = ((start & 0x80000000 & (unsigned char)parameter1 & (slong ^ 0x80000000)) != 0) || ((0x80000000 & slong & (parameter1 ^ 0x80000000) & (start ^ 0x80)) !=0);
			this->cpu.sr.flag.z = ((slong & 0xFFFFFFFF) == 0);
			this->cpu.sr.flag.n = ((slong & 0x80000000) == 0x80000000);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			result = slong;
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
	this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
}

void mc68k::ADDQhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
	switch (size) {
		case 0x00 :
			result = (value & 0xFFFFFF00) | ((value + ((this->cpu.instruction_reg & 0x0E00) >> 9)) & 0x000000FF);
			this->cpu.sr.flag.c = (((result&0x80)==0) && ((value&0x80)!=0));
			this->cpu.sr.flag.v = (0x80&result&(value^0x80))!=0;
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x80) == 0x80);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
			break;
		case 0x01 :
			result = (value & 0xFFFF0000) | ((value + ((this->cpu.instruction_reg & 0x0E00) >> 9)) & 0x0000FFFF);
			this->cpu.sr.flag.c = (((result&0x8000)==0) && ((value&0x8000)!=0));
			this->cpu.sr.flag.v = (0x8000&result&(value^0x8000))!=0;
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x8000) == 0x8000);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
			break;
		case 0x02 :
			result = value + ((this->cpu.instruction_reg & 0x0E00) >> 9);
			this->cpu.sr.flag.c = (((result&0x80000000)==0) && ((value&0x80000000)!=0));
			this->cpu.sr.flag.v = (0x80000000&result&(value^0x80000000))!=0;
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
}

void mc68k::ADDXhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	if (this->cpu.instruction_reg & 0x0008) {			// -(An) memory to memory operation
		switch (size) {
			case 0x00 :
				value = this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF;
				result = (this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x000000FF) + value + this->cpu.sr.flag.x;
				this->cpu.sr.flag.c = (((value&0x80)==0) && ((((this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x000000FF)|result)&0x80)!=0)) || (((this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x000000FF)&result&0x80)!=0);;
				this->cpu.sr.flag.v = ((0x80&((this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x000000FF)^0x80)&value&(result^0x80))!=0)||((0x80&result&(value^0x80)&(this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x000000FF))!=0);;
				this->cpu.sr.flag.n = ((result & 0x00000080) == 0x80);
				this->cpu.sr.flag.z = this->cpu.sr.flag.z & ((result & 0x000000FF) == 0);
				this->cpu.sr.flag.x = this->cpu.sr.flag.c;
				this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = (this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFFFF00) | (result & 0x000000FF);
				break;
			case 0x01 :
				value = this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF;
				result = (this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF) + value + this->cpu.sr.flag.x;
				this->cpu.sr.flag.c = (((value&0x8000)==0) && ((((this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF)|result)&0x8000)!=0)) || (((this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF)&result&0x8000)!=0);;
				this->cpu.sr.flag.v = ((0x8000&((this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF)^0x8000)&value&(result^0x8000))!=0)||((0x8000&result&(value^0x8000)&(this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF))!=0);;
				this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
				this->cpu.sr.flag.z = this->cpu.sr.flag.z & ((result & 0x0000FFFF) == 0);
				this->cpu.sr.flag.x = this->cpu.sr.flag.c;
				this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = (this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFF0000) | (result & 0x0000FFFF);
				break;
			case 0x02 :
				value = this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9];
				result = this->cpu.a[this->cpu.instruction_reg & 0x0007] + value + this->cpu.sr.flag.x;
				this->cpu.sr.flag.c = (((value&0x80000000)==0) && (((this->cpu.a[this->cpu.instruction_reg & 0x0007]|result)&0x80000000)!=0)) || ((this->cpu.a[this->cpu.instruction_reg & 0x0007]&result&0x80000000)!=0);;
				this->cpu.sr.flag.v = ((0x80000000&(this->cpu.a[this->cpu.instruction_reg & 0x0007]^0x8000)&value&(result^0x80000000))!=0)||((0x80000000&result&(value^0x80000000)&this->cpu.a[this->cpu.instruction_reg & 0x0007])!=0);;
				this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
				this->cpu.sr.flag.z = this->cpu.sr.flag.z & (result == 0);
				this->cpu.sr.flag.x = this->cpu.sr.flag.c;
				this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = result;
				break;
			case 0x03 :
				this->pendingException = ILLEGAL_INSTRUCTION;
				return;
				break;
		}
	} else {										// Dn to Dn operation
		switch (size) {
			case 0x00 :
				value = this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF;
				result = (this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x000000FF) + value + this->cpu.sr.flag.x;
				this->cpu.sr.flag.c = (((value&0x80)==0) && ((((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x000000FF)|result)&0x80)!=0)) || (((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x000000FF)&result&0x80)!=0);;
				this->cpu.sr.flag.v = ((0x80&((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x000000FF)^0x80)&value&(result^0x80))!=0)||((0x80&result&(value^0x80)&(this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x000000FF))!=0);;
				this->cpu.sr.flag.n = ((result & 0x00000080) == 0x80);
				this->cpu.sr.flag.z = this->cpu.sr.flag.z & ((result & 0x000000FF) == 0);
				this->cpu.sr.flag.x = this->cpu.sr.flag.c;
				this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFFFF00) | (result & 0x000000FF);
				break;
			case 0x01 :
				value = this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF;
				result = (this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF) + value + this->cpu.sr.flag.x;
				this->cpu.sr.flag.c = (((value&0x8000)==0) && ((((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF)|result)&0x8000)!=0)) || (((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF)&result&0x8000)!=0);;
				this->cpu.sr.flag.v = ((0x8000&((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF)^0x8000)&value&(result^0x8000))!=0)||((0x8000&result&(value^0x8000)&(this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF))!=0);;
				this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
				this->cpu.sr.flag.z = this->cpu.sr.flag.z & ((result & 0x0000FFFF) == 0);
				this->cpu.sr.flag.x = this->cpu.sr.flag.c;
				this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFF0000) | (result & 0x0000FFFF);
				break;
			case 0x02 :
				value = this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9];
				result = this->cpu.d[this->cpu.instruction_reg & 0x0007] + value + this->cpu.sr.flag.x;
				this->cpu.sr.flag.c = (((value&0x80000000)==0) && (((this->cpu.d[this->cpu.instruction_reg & 0x0007]|result)&0x80000000)!=0)) || ((this->cpu.d[this->cpu.instruction_reg & 0x0007]&result&0x80000000)!=0);;
				this->cpu.sr.flag.v = ((0x80000000&(this->cpu.d[this->cpu.instruction_reg & 0x0007]^0x8000)&value&(result^0x80000000))!=0)||((0x80000000&result&(value^0x80000000)&this->cpu.d[this->cpu.instruction_reg & 0x0007])!=0);;
				this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
				this->cpu.sr.flag.z = this->cpu.sr.flag.z & (result == 0);
				this->cpu.sr.flag.x = this->cpu.sr.flag.c;
				this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = result;
				break;
			case 0x03 :
				this->pendingException = ILLEGAL_INSTRUCTION;
				return;
				break;
		}
	}
}

void mc68k::ANDhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
	switch (size) {
		case 0x00 :
			if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
				result = (value & 0xFFFFFF00) | ((value & 0x000000FF) & (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF));
			else									// Destination is Dx
				result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFFFF00) | ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF) & (value & 0x000000FF));
			this->cpu.sr.flag.c = false;
			this->cpu.sr.flag.v = false;
			this->cpu.sr.flag.z = ((result & 0x000000FF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00000080) == 0x80);
			break;
		case 0x01 :
			if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
				result = (value & 0xFFFF0000) | ((value & 0x0000FFFF) & (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF));
			else									// Destination is Dx
				result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFF0000) | ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF) & (value & 0x0000FFFF));
			this->cpu.sr.flag.c = false;
			this->cpu.sr.flag.v = false;
			this->cpu.sr.flag.z = ((result & 0x0000FFFF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
			break;
		case 0x02 :
			if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
				result = (value & this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]);
			else									// Destination is Dx
				result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & value);
			this->cpu.sr.flag.c = false;
			this->cpu.sr.flag.v = false;
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = (result & 0x80000000) == 0x80000000;
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
	if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
		this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
	else
		this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = result;
}

void mc68k::ANDIhandler(void) {
	unsigned char	size;
	unsigned int	value;

	if (this->cpu.instruction_reg == 0x023C) {	// ANDI to CCR
		value = this->readWord(this->cpu.pc);
		this->cpu.pc += 2;
		if (this->DEBUG)
			WriteLog(" %04X", value);
		this->cpu.sr.reg &= (value & 0x000000FF);
	}
	if (this->cpu.instruction_reg == 0x027C) {	// ANDI to SR
		if (this->cpu.sr.flag.s) {
			value = this->readWord(this->cpu.pc);
			this->cpu.pc += 2;
			if (this->DEBUG)
				WriteLog(" %04X", value);
			this->cpu.sr.reg &= (value & 0x0000FFFF);
		} else
			this->pendingException = PRIVILEGE_VIOLATION;
	}
	if ((this->cpu.instruction_reg & 0xFFBF) != 0x023C) {	// ANDI
		size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
		value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
		this->cpu.sr.flag.c = false;
		this->cpu.sr.flag.v = false;
		switch (size) {
			case 0x00 :					// ANDI.B
				value &= (this->readWord(this->cpu.pc) & 0xFF);
				this->cpu.pc += 2;
				if (this->DEBUG)
					WriteLog(" %04X", value);
				this->cpu.sr.flag.z = ((value & 0x000000FF) == 0);
				this->cpu.sr.flag.n = ((value & 0x00000080) == 0x80);
				break;
			case 0x01 :					// ANDI.W
				value &= this->readWord(this->cpu.pc);
				this->cpu.pc += 2;
				if (this->DEBUG)
					WriteLog(" %04X", value);
				this->cpu.sr.flag.z = ((value & 0x0000FFFF) == 0);
				this->cpu.sr.flag.n = ((value & 0x00008000) == 0x8000);
				break;
			case 0x02 :					// ANDI.L
				value &= this->readLong(this->cpu.pc);
				this->cpu.pc += 4;
				if (this->DEBUG)
					WriteLog(" %08X", value);
				this->cpu.sr.flag.z = (value == 0);
				this->cpu.sr.flag.n = ((value &0x80000000) == 0x80000000);
				break;
			case 0x03 :
				this->pendingException = ILLEGAL_INSTRUCTION;
				return;
				break;
		}
		this->rewindPC(this->cpu.instruction_reg & 0x3F);
		this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, value);
	}
}

void mc68k::ASxLSx_handler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	this->cpu.sr.flag.v = false;
	switch (size) {
		case 0x00 :
			if (this->cpu.instruction_reg & 0x0020)
				value = (this->cpu.d[((this->cpu.instruction_reg & 0x0E00) >> 9)] & 0x3F);	// Dn modulo 64 mode
			else {
				value = ((this->cpu.instruction_reg & 0x0E00) >> 9);						// Immediate mode
				if (value == 0)
					value = 8;
			}
			if (this->cpu.instruction_reg & 0x0100) {	// Shift left
				this->cpu.sr.flag.c = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & (1 << (8 - value))) == true);
				if (((this->cpu.d[this->cpu.instruction_reg & 0x0007] & (((1 << value) - 1) << (8 - value))) == true) && ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & (((1 << value) - 1) << (8 - value))) != (((1 << value) - 1) << (8 - value))))
					this->cpu.sr.flag.v = true;	// Bit change during shift, so set overflow
				result = this->cpu.d[this->cpu.instruction_reg & 0x0007] << value;
			} else {								// Shift right
				this->cpu.sr.flag.c = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & (0x00000080 >> value)) == true);
				if (((this->cpu.d[this->cpu.instruction_reg & 0x0007] & ((1 << value) - 1)) == true) && ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & ((1 << value) - 1)) != ((1 << value) - 1)))
					this->cpu.sr.flag.v = true;	// Bit change during shift, so set overflow
				result = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x000000FF) >> value);
				if (this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x00000080)
					result |= (((1 << value) - 1) << (8 - value));	// Set MSB bits on ASR instruction
				else
					result &= ~(((1 << value) - 1) << (8 - value));	// Clear MSB bits on ASR instruction
				if (this->cpu.instruction_reg & 0x0004)
					result &= ~(((1 << value) - 1) << (8 - value));	// Clear MSB bits on LSR instruction
			}
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			this->cpu.sr.flag.z = ((result & 0x000000FF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00000080) == 0x00000080);
			this->cpu.d[this->cpu.instruction_reg & 0x0007] = (this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0xFFFFFF00) | (result & 0x000000FF);
			break;
		case 0x01 :
			if (this->cpu.instruction_reg & 0x0020)
				value = (this->cpu.d[((this->cpu.instruction_reg & 0x0E00) >> 9)] & 0x3F);	// Dn modulo 64 mode
			else {
				value = ((this->cpu.instruction_reg & 0x0E00) >> 9);						// Immediate mode
				if (value == 0)
					value = 8;
			}
			if (this->cpu.instruction_reg & 0x0100) {	// Shift left
				this->cpu.sr.flag.c = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & (1 << (16 - value))) == true);
				if (((this->cpu.d[this->cpu.instruction_reg & 0x0007] & (((1 << value) - 1) << (16 - value))) == true) && ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & (((1 << value) - 1) << (16 - value))) != (((1 << value) - 1) << (16 - value))))
					this->cpu.sr.flag.v = true;	// Bit change during shift, so set overflow
				result = this->cpu.d[this->cpu.instruction_reg & 0x0007] << value;
			} else {								// Shift right
				this->cpu.sr.flag.c = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & (0x00008000 >> value)) == true);
				if (((this->cpu.d[this->cpu.instruction_reg & 0x0007] & ((1 << value) - 1)) == true) && ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & ((1 << value) - 1)) != ((1 << value) - 1)))
					this->cpu.sr.flag.v = true;	// Bit change during shift, so set overflow
				result = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF) >> value);
				if (this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x00008000)
					result |= (((1 << value) - 1) << (16 - value));	// Set MSB bits on ASR instruction
				else
					result &= ~(((1 << value) - 1) << (16 - value));	// Clear MSB bits on ASR instruction
				if (this->cpu.instruction_reg & 0x0004)
					result &= ~(((1 << value) - 1) << (16 - value));	// Clear MSB bits on LSR instruction
			}
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			this->cpu.sr.flag.z = ((result & 0x0000FFFF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00008000) == 0x00008000);
			this->cpu.d[this->cpu.instruction_reg & 0x0007] = (this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0xFFFF0000) | (result & 0x0000FFFF);
			break;
		case 0x02 :
			if (this->cpu.instruction_reg & 0x0020)
				value = (this->cpu.d[((this->cpu.instruction_reg & 0x0E00) >> 9)] & 0x3F);	// Dn modulo 64 mode
			else {
				value = ((this->cpu.instruction_reg & 0x0E00) >> 9);						// Immediate mode
				if (value == 0)
					value = 8;
			}
			if (this->cpu.instruction_reg & 0x0100) {	// Shift left
				this->cpu.sr.flag.c = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & (1 << (32 - value))) == true);
				if (((this->cpu.d[this->cpu.instruction_reg & 0x0007] & (((1 << value) - 1) << (32 - value))) == true) && ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & (((1 << value) - 1) << (32 - value))) != (((1 << value) - 1) << (32 - value))))
					this->cpu.sr.flag.v = true;	// Bit change during shift, so set overflow
				result = this->cpu.d[this->cpu.instruction_reg & 0x0007] << value;
			} else {								// Shift right
				this->cpu.sr.flag.c = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & (0x80000000 >> value)) == true);
				if (((this->cpu.d[this->cpu.instruction_reg & 0x0007] & ((1 << value) - 1)) == true) && ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & ((1 << value) - 1)) != ((1 << value) - 1)))
					this->cpu.sr.flag.v = true;	// Bit change during shift, so set overflow
				result = this->cpu.d[this->cpu.instruction_reg & 0x0007] >> value;
				if (this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x80000000)
					result |= (((1 << value) - 1) << (32 - value));	// Set MSB bits on ASR instruction
				else
					result &= ~(((1 << value) - 1) << (32 - value));	// Clear MSB bits on ASR instruction
				if (this->cpu.instruction_reg & 0x0004)
					result &= ~(((1 << value) - 1) << (32 - value));	// Clear MSB bits on LSR instruction
			}
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
			this->cpu.d[this->cpu.instruction_reg & 0x0007] = result;
			break;
		case 0x03 :
			value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
				WriteLog("mc68k::ASxLSx_handler -- Unimplemented instruction %04X at %08X\n", this->cpu.instruction_reg, this->cpu.pc);
			break;
	}
}

void mc68k::BCHGhandler(void) {
	unsigned char	ubyte;
	unsigned int	value;

	if (this->cpu.instruction_reg & 0x0100)
		ubyte = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFF);
	else {
		ubyte = (this->readWord(this->cpu.pc) & 0x00FF);
		this->cpu.pc += 2;
	}

	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x00);
	if (value & (1 << ubyte))
		this->cpu.sr.flag.z = false;
	else
		this->cpu.sr.flag.z = true;
	value ^= (1 << ubyte);	// Clear bit
	this->rewindPC(this->cpu.instruction_reg & 0x3F);
	this->setOperandValue(this->cpu.instruction_reg & 0x3F, 0x00, value);
}

void mc68k::BCLRhandler(void) {
	unsigned char	ubyte;
	unsigned int	value;

	if (this->cpu.instruction_reg & 0x0100)
		ubyte = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFF);
	else {
		ubyte = (this->readWord(this->cpu.pc) & 0x00FF);
		this->cpu.pc += 2;
	}

	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x00);
	if (value & (1 << ubyte))
		this->cpu.sr.flag.z = false;
	else
		this->cpu.sr.flag.z = true;
	value &= (1 << ubyte);	// Clear bit
	this->rewindPC(this->cpu.instruction_reg & 0x3F);
	this->setOperandValue(this->cpu.instruction_reg & 0x3F, 0x00, value);
}

void mc68k::BSEThandler(void) {
	unsigned char	ubyte;
	unsigned int	value;

	if (this->cpu.instruction_reg & 0x0100)
		ubyte = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFF);
	else {
		ubyte = (this->readWord(this->cpu.pc) & 0x00FF);
		this->cpu.pc += 2;
	}

	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x00);
	if (value & (1 << ubyte))
		this->cpu.sr.flag.z = false;
	else
		this->cpu.sr.flag.z = true;
	value |= (1 << ubyte);	// Set bit
	this->rewindPC(this->cpu.instruction_reg & 0x3F);
	this->setOperandValue(this->cpu.instruction_reg & 0x3F, 0x00, value);
}
void mc68k::BTSThandler(void) {
	unsigned char	ubyte;
	unsigned int	value;

	if (this->cpu.instruction_reg & 0x0100)
		ubyte = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFF);
	else {
		ubyte = (this->readWord(this->cpu.pc) & 0x00FF);
		this->cpu.pc += 2;
	}

	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x00);
	if (value & (1 << ubyte))
		this->cpu.sr.flag.z = false;
	else
		this->cpu.sr.flag.z = true;
}

void mc68k::CHKhandler(void) {
	unsigned int	value;

	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x01);
	if (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] < 0) {
		this->cpu.sr.flag.n = 1;
		this->pendingException = CHK_INSTRUCTION;
	}
	if (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] > value) {
		this->cpu.sr.flag.n = 0;
		this->pendingException = CHK_INSTRUCTION;
	}
}

void mc68k::CMPhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);

	switch (size) {
		case 0x00 :
			result = ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF) - (value & 0x000000FF));
			this->cpu.sr.flag.c = (((value&0x80)==0) && ((((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF)|result)&0x80)!=0)) || (((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF)&result&0x80)!=0);
			this->cpu.sr.flag.v = ((0x80&((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF)^0x80)&value&(result^0x80))!=0)||((0x80&result&(value^0x80)&(this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF))!=0);
			this->cpu.sr.flag.z = ((result & 0x000000FF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00000080) == 0x80);
			break;
		case 0x01 :
			result = ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF) - (value & 0x0000FFFF));
			this->cpu.sr.flag.c = (((value&0x8000)==0) && ((((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF)|result)&0x8000)!=0)) || (((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF)&result&0x8000)!=0);
			this->cpu.sr.flag.v = ((0x8000&((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF)^0x8000)&value&(result^0x8000))!=0)||((0x8000&result&(value^0x8000)&(this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF))!=0);
			this->cpu.sr.flag.z = ((result & 0x0000FFFF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
			break;
		case 0x02 :
			result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] - value);
			this->cpu.sr.flag.c = (((value&0x80000000)==0) && (((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] | result)&0x80000000)!=0)) || ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&result&0x80000000)!=0);
			this->cpu.sr.flag.v = ((0x80000000&(this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]^0x80000000)&value&(result^0x80000000))!=0)||((0x80000000&result&(value^0x80000000)&this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9])!=0);
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
}

void mc68k::CMPAhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x0100) >> 8);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, (size + 1));
	if (size == 1) { // Size is L
		result = (this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] - value);
		this->cpu.sr.flag.c = (((this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9]&0x80000000)==0) && (((value|result)&0x80000000)!=0)) || ((value&result&0x80000000)!=0);
		this->cpu.sr.flag.v = ((0x80000000&(value^0x80000000)&this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9]&(result^0x80000000))!=0)||((0x80000000&result&(this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9]^0x80000000)&value)!=0);
		this->cpu.sr.flag.z = (result == 0);
		this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
	} else {		// Size is W
		result = ((this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF) - (value & 0x0000FFFF));
		this->cpu.sr.flag.c = (((this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9]&0x80000000)==0) && (((value|result)&0x80000000)!=0)) || ((value&result&0x80000000)!=0);
		this->cpu.sr.flag.v = ((0x80000000&(value^0x80000000)&this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9]&(result^0x80000000))!=0)||((0x80000000&result&(this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9]^0x80000000)&value)!=0);
		this->cpu.sr.flag.z = (result == 0);
		this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
	}
}

void mc68k::CMPISUBIhandler(void) {
	unsigned char	size;
	unsigned int	start;
	unsigned int	parameter1;
	unsigned int	result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	switch (size) {
		case 0x00 :					// CMPI.B
			parameter1 = (this->readWord(this->cpu.pc) & 0xFF);
			this->cpu.pc += 2;
			start = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
			result = ((start & 0xFF) - parameter1);
//			WriteLog("CMPI.B pc=%08X start=%02X parameter1=%04X result=%02X\n", this->cpu.pc, start, parameter1, result);
			this->cpu.sr.flag.c = (((parameter1 & 0x80) == 0) && (((start|result) & 0x80) != 0)) || ((start&result&0x80) != 0);
			this->cpu.sr.flag.v = (((start ^ 0x80) & 0x80 & (parameter1 & 0xFF) & (result ^ 0x80)) != 0) || ((0x80 & result & (parameter1 ^ 0x80) & start) !=0);
			this->cpu.sr.flag.z = ((result & 0xFF) == 0);
			this->cpu.sr.flag.n = ((result & 0x80) == 0x80);
			break;
		case 0x01 :					// CMPI.W
			parameter1 = this->readWord(this->cpu.pc);
			this->cpu.pc += 2;
			start = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
			result = ((start & 0xFFFF) - parameter1);
//			WriteLog("CMPI.W pc=%08X start=%04X parameter1=%04X result=%04X\n", this->cpu.pc, start, parameter1, result);
			this->cpu.sr.flag.c = (((parameter1 & 0x8000) == 0) && (((start|result) & 0x8000) != 0)) || ((start&result&0x8000) != 0);
			this->cpu.sr.flag.v = (((start ^ 0x8000) & 0x8000 & (parameter1 & 0xFFFF) & (result ^ 0x8000)) != 0) || ((0x8000 & result & (parameter1 ^ 0x8000) & start) !=0);
			this->cpu.sr.flag.z = ((result & 0x0000FFFF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
			break;
		case 0x02 :					// CMPI.L
			parameter1 = this->readLong(this->cpu.pc);
			this->cpu.pc += 4;
			start = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
			result = (start - parameter1);
//			WriteLog("CMPI.L pc=%08X start=%08X parameter1=%04X result=%08X\n", this->cpu.pc, start, parameter1, result);
			this->cpu.sr.flag.c = (((parameter1 & 0x80000000) == 0) && (((start|result) & 0x80000000) != 0)) || ((start&result&0x80000000) != 0);
			this->cpu.sr.flag.v = (((start ^ 0x80000000) & 0x80000000 & parameter1 & (result ^ 0x80000000)) != 0) || ((0x80000000 & result & (parameter1 ^ 0x80000000) & start) !=0);
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
	if ((this->cpu.instruction_reg & 0x0F00) == 0x0400) {						// Write result back if this is a SUBI
		this->rewindPC(this->cpu.instruction_reg & 0x3F);
		this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
		this->cpu.sr.flag.x = this->cpu.sr.flag.c;
	}
}

void mc68k::CMPMhandler(void) {
	unsigned char	size;
	unsigned int	value, value2, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue((this->cpu.instruction_reg & 0x0007) + 0x0018, size);
	value2 = this->getOperandValue(((this->cpu.instruction_reg & 0x0E00) >> 9) + 0x0018, size);
	switch (size) {
		case 0x00 :
			result = ((value2 & 0x000000FF) - (value & 0x000000FF));
			this->cpu.sr.flag.c = (((value2 & 0x00000080)==0) && (((value | value2) & 0x00000080)!=0)) || ((value & result & 0x00000080)!=0);
			this->cpu.sr.flag.v = ((0x00008000 & (value ^ 0x00000080) & value2 & (result ^ 0x00000080)) !=0) || ((0x00000080 & result & (value2 ^ 0x00000080) & value) != 0);
			this->cpu.sr.flag.z = ((result & 0x000000FF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00000080) == 0x80);
			break;
		case 0x01 :
			result = ((value2 & 0x0000FFFF) - (value & 0x0000FFFF));
			this->cpu.sr.flag.c = (((value2 & 0x00008000)==0) && (((value | value2) & 0x00008000)!=0)) || ((value & result & 0x00008000)!=0);
			this->cpu.sr.flag.v = ((0x00008000 & (value ^ 0x00008000) & value2 & (result ^ 0x00008000)) !=0) || ((0x00008000 & result & (value2 ^ 0x00008000) & value) != 0);
			this->cpu.sr.flag.z = ((result & 0x0000FFFF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
			break;
		case 0x02 :
			result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] - value);
			this->cpu.sr.flag.c = (((value2 & 0x80000000)==0) && (((value | value2) & 0x80000000)!=0)) || ((value & result & 0x80000000)!=0);
			this->cpu.sr.flag.v = ((0x80000000 & (value ^ 0x80000000) & value2 & (result ^ 0x80000000)) !=0) || ((0x80000000 & result & (value2 ^ 0x80000000) & value) != 0);
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
}

void mc68k::DIVUhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);

	if (value) {
		result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] % value);	// bits 15-0=quotient
		if (result > 65535)
			this->cpu.sr.flag.v = true;
		else
			this->cpu.sr.flag.v = false;
		result = result << 16;
		result |= ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] / value) & 0xFFFF);	// bits 31-16=remainder
		this->cpu.sr.flag.z = (result == 0);
		this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
		this->cpu.sr.flag.c = false;
		this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = result;
	} else
		this->pendingException = DIVIDE_BY_ZERO;
}

void mc68k::DIVShandler(void) {
	unsigned char	size;
	int				signedvalue, signedresult;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	signedvalue = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);

	if (signedvalue) {
		signedresult = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] % signedvalue);	// bits 15-0=quotient
		if ((signedresult < -32768) || (signedresult > 32767))
			this->cpu.sr.flag.v = true;
		else
			this->cpu.sr.flag.v = false;
		signedresult = signedresult << 16;
		signedresult |= ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] / signedvalue) & 0xFFFF);	// bits 31-16=remainder
		this->cpu.sr.flag.z = (signedresult == 0);
		this->cpu.sr.flag.n = ((signedresult & 0x80000000) == 0x80000000);
		this->cpu.sr.flag.c = false;
		this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = signedresult;
	} else
		this->pendingException = DIVIDE_BY_ZERO;
}

void mc68k::EORhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
	this->cpu.sr.flag.c = false;
	this->cpu.sr.flag.v = false;
	switch (size) {
		case 0x00 :
			result = (value ^ (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF));
			this->cpu.sr.flag.z = ((result & 0x000000FF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00000080) == 0x80);
			break;
		case 0x01 :
			result = (value ^ (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF));
			this->cpu.sr.flag.z = ((result & 0x0000FFFF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
			break;
		case 0x02 :
			result = (value ^ this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]);
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
	this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
}

void mc68k::EORIhandler(void) {
	unsigned char	size;
	unsigned int	value;

	if (this->cpu.instruction_reg == 0x0A3C) {	// EORI to CCR
		value = this->readWord(this->cpu.pc+2);
		this->cpu.pc += 2;
		this->cpu.sr.reg ^= (value & 0x000000FF);
	}
	if (this->cpu.instruction_reg == 0x0A7C) {	// EORI to SR
		if (this->cpu.sr.flag.s) {
			value = this->readWord(this->cpu.pc);
			this->cpu.pc += 2;
			this->cpu.sr.reg ^= (value & 0x0000FFFF);
		} else
			this->pendingException = PRIVILEGE_VIOLATION;
	}
	if ((this->cpu.instruction_reg & 0xFFBF) != 0x0A3C) {	// EORI
		size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
		value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
		this->cpu.sr.flag.c = false;
		this->cpu.sr.flag.v = false;
		switch (size) {
			case 0x00 :					// EORI.B
				value ^= (this->readWord(this->cpu.pc) & 0xFF);
				this->cpu.pc += 2;
				this->cpu.sr.flag.z = ((value & 0x000000FF) == 0);
				this->cpu.sr.flag.n = ((value & 0x00000080) == 0x80);
				break;
			case 0x01 :					// EORI.W
				value ^= this->readWord(this->cpu.pc);
				this->cpu.pc += 2;
				this->cpu.sr.flag.z = ((value & 0x0000FFFF) == 0);
				this->cpu.sr.flag.n = ((value & 0x00008000) == 0x8000);
				break;
			case 0x02 :					// EORI.L
				value ^= this->readLong(this->cpu.pc);
				this->cpu.pc += 4;
				this->cpu.sr.flag.z = (value == 0);
				this->cpu.sr.flag.n = ((value &0x80000000) == 0x80000000);
				break;
			case 0x03 :
				this->pendingException = ILLEGAL_INSTRUCTION;
				return;
				break;
		}
		this->rewindPC(this->cpu.instruction_reg & 0x3F);
		this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, value);
	}
}

void mc68k::EXGhandler(void) {
	unsigned int	value;

	switch (this->cpu.instruction_reg & 0x00C8) {
		case 0x0040 :	// Exchange data registers
			value = this->cpu.d[((this->cpu.instruction_reg & 0x0E00) >> 9)];
			this->cpu.d[((this->cpu.instruction_reg & 0x0E00) >> 9)] = this->cpu.d[(this->cpu.instruction_reg & 0x0007)];
			this->cpu.d[(this->cpu.instruction_reg & 0x0007)] = value;
			break;
		case 0x0048 :	// Exchange address registers
			value = this->cpu.a[((this->cpu.instruction_reg & 0x0E00) >> 9)];
			this->cpu.a[((this->cpu.instruction_reg & 0x0E00) >> 9)] = this->cpu.a[(this->cpu.instruction_reg & 0x0007)];
			this->cpu.a[(this->cpu.instruction_reg & 0x0007)] = value;
			break;
		case 0x0088 :	// Exchange data and address register
			value = this->cpu.d[((this->cpu.instruction_reg & 0x0E00) >> 9)];
			this->cpu.d[((this->cpu.instruction_reg & 0x0E00) >> 9)] = this->cpu.a[(this->cpu.instruction_reg & 0x0007)];
			this->cpu.a[(this->cpu.instruction_reg & 0x0007)] = value;
			break;
	}
}

void mc68k::EXThandler(void) {
	unsigned int	value, result;

	value = this->cpu.d[this->cpu.instruction_reg & 0x07];
	if (this->cpu.instruction_reg & 0x0040) {			// EXT.L
		result = this->signExtend(value);
		this->cpu.sr.flag.z = (result == 0);
		this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
	} else {
		if (value & 0x00000080)							// EXT.W
			result = ((value & 0xFFFF00FF) | 0x0000FF00);
		else
			result = (value & 0xFFFF00FF);
		this->cpu.sr.flag.z = ((result & 0x0000FFFF) == 0);
		this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
	}
	this->cpu.sr.flag.c = false;
	this->cpu.sr.flag.v = false;
	this->cpu.d[this->cpu.instruction_reg & 0x07] = result;
}

void mc68k::LEAhandler(void) {
	unsigned int	value;
	unsigned char	Operand;

	Operand = (this->cpu.instruction_reg & 0x3F);
	switch (Operand) {
		case 0x3A :
			value = this->readWord(this->cpu.pc);
			if (this->DEBUG)
				WriteLog(" %04X", value);
			if (value & 0x8000)
				this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = this->cpu.pc - (~value & 0xFFFF) - 1;
			else
				this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = this->cpu.pc + (value & 0xFFFF);
			this->cpu.pc += 2;
			break;

		case 0x28 :
		case 0x29 :
		case 0x2A :
		case 0x2B :
		case 0x2C :
		case 0x2D :
		case 0x2E :
			value = this->readWord(this->cpu.pc);
			if (this->DEBUG)
				WriteLog(" %04X", value);
			this->cpu.pc += 2;
			if (value & 0x8000)
				this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = this->cpu.a[Operand & 7] - (~value & 0xFFFF) - 1;
			else
				this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = this->cpu.a[Operand & 7] + (value & 0xFFFF);
			break;

		case 0x2F :
			value = this->readWord(this->cpu.pc);
			if (this->DEBUG)
				WriteLog(" %04X", value);
			this->cpu.pc += 2;
			if (value & 0x8000)
				if (this->cpu.sr.flag.s)
					this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = this->cpu.ssp - (~value & 0xFFFF) - 1;
				else
					this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = this->cpu.usp - (~value & 0xFFFF) - 1;
			else
				if (this->cpu.sr.flag.s)
					this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = this->cpu.ssp + (value & 0xFFFF);
				else
					this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = this->cpu.usp + (value & 0xFFFF);
			break;

		default :
			this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = this->getOperandValue(Operand, 0x02);
			break;
	}
}
void mc68k::LINKhandler(void) {
	unsigned int	value;

	value = this->readWord(this->cpu.pc);
	this->cpu.pc += 2;
	if (this->cpu.sr.flag.s) {
		this->cpu.ssp -= 4;
		writeLong(this->cpu.ssp, this->cpu.a[this->cpu.instruction_reg & 0x0007]);
		this->cpu.a[this->cpu.instruction_reg & 0x0007] = this->cpu.ssp;
		this->cpu.ssp += value;
	} else {
		this->cpu.usp -= 4;
		writeLong(this->cpu.usp, this->cpu.a[this->cpu.instruction_reg & 0x0007]);
		this->cpu.a[this->cpu.instruction_reg & 0x0007] = this->cpu.ssp;
		this->cpu.usp += value;
	}
}

void mc68k::MOVEhandler(void) {
	unsigned char	size;
	unsigned int	value;

	size = ((this->cpu.instruction_reg & 0x3000) >> 12);
	if (size != 0) {	// Size 0 is invalid for MOVE, it's a different opcode
		if (size == 1)	// MOVE uses different sizing bits
			size = 0;
		if (size == 3)
			size = 1;
		value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
		this->setOperandValue(((this->cpu.instruction_reg & 0x01C0) >> 3) | ((this->cpu.instruction_reg & 0x0E00) >> 9), size, value);
		if ((this->cpu.instruction_reg & 0x01C0) != 0x0040) {	// Only set flags for MOVE instruction, not for MOVEA
			this->cpu.sr.flag.c = 0;
			this->cpu.sr.flag.v = 0;
			switch (size) {
				case 0x00 :
					this->cpu.sr.flag.n = ((value & 0x00000080) == 0x80);
					this->cpu.sr.flag.z = ((value & 0x000000FF) == 0);
					break;
				case 0x01 :
					this->cpu.sr.flag.n = ((value & 0x00008000) == 0x8000);
					this->cpu.sr.flag.z = ((value & 0x0000FFFF) == 0);
					break;
				case 0x02 :
					this->cpu.sr.flag.n = ((value & 0x80000000) == 0x80000000);
					this->cpu.sr.flag.z = (value == 0);
					break;
			}
		}
	}
}

void mc68k::MOVEMhandler(void) {
	unsigned int	parameter1;
	unsigned short	uword;

	parameter1 = readWord (this->cpu.pc);
	if (this->cpu.instruction_reg & 0x0400) {				// 0==Register to memory, 1==Memory to register
		uword = 1;
		if (this->cpu.instruction_reg & 0x0040) {				// 0==Word, 1==Long
			for (int i=0; i<8; i++) {
				if (parameter1 & uword) {
					if (this->cpu.sr.flag.s) {
						this->cpu.d[i] = this->readLong(this->cpu.ssp);
						this->cpu.ssp += 4;
					} else {
						this->cpu.d[i] = this->readLong(this->cpu.usp);
						this->cpu.usp += 4;
					}
				}
				uword <<= 1;
			}
			for (int i=0; i<7; i++) {
				if (parameter1 & uword) {
					if (this->cpu.sr.flag.s) {
						this->cpu.a[i] = this->readLong(this->cpu.ssp);
						this->cpu.ssp += 4;
					} else {
						this->cpu.a[i] = this->readLong(this->cpu.usp);
						this->cpu.usp += 4;
					}
				}
				uword <<= 1;
			}
		} else {
			for (int i=0; i<8; i++) {
				if (parameter1 & uword) {
					if (this->cpu.sr.flag.s) {
						this->cpu.d[i] = this->readWord(this->cpu.ssp);
						this->cpu.ssp += 4;
					} else {
						this->cpu.d[i] = this->readWord(this->cpu.usp);
						this->cpu.usp += 4;
					}
				}
				uword <<= 1;
			}
			for (int i=0; i<7; i++) {
				if (parameter1 & uword) {
					if (this->cpu.sr.flag.s) {
						this->cpu.a[i] = this->readWord(this->cpu.ssp);
						this->cpu.ssp += 4;
					} else {
						this->cpu.a[i] = this->readWord(this->cpu.usp);
						this->cpu.usp += 4;
					}
				}
				uword <<= 1;
			}
		}
	} else {
		uword = 32768;
		if (this->cpu.instruction_reg & 0x0040) {				// 0==Word, 1==Long
			for (int i=7; i>=0; i--) {
				if (parameter1 & uword) {
					if (this->cpu.sr.flag.s) {
						this->cpu.ssp -= 4;
						writeLong(this->cpu.ssp, this->cpu.d[i]);
					} else {
						this->cpu.usp -= 4;
						writeLong(this->cpu.usp, this->cpu.d[i]);
					}
				}
				uword >>= 1;
			}
			for (int i=7; i>=0; i--) {
				if (parameter1 & uword) {
					if (this->cpu.sr.flag.s) {
						this->cpu.ssp -= 4;
						writeLong(this->cpu.ssp, this->cpu.a[i]);
					} else {
						this->cpu.usp -= 4;
						writeLong(this->cpu.usp, this->cpu.a[i]);
					}
				}
				uword >>= 1;
			}
		} else {
			for (int i=7; i>=0; i--) {
				if (parameter1 & uword) {
					if (this->cpu.sr.flag.s) {
						this->cpu.ssp -= 4;
						writeWord(this->cpu.ssp, this->cpu.d[i]);
					} else {
						this->cpu.usp -= 4;
						writeWord(this->cpu.usp, this->cpu.d[i]);
					}
				}
				uword >>= 1;
			}
			for (int i=8; i>=0; i--) {
				if (parameter1 & uword) {
					if (this->cpu.sr.flag.s) {
						this->cpu.ssp -= 4;
						writeWord(this->cpu.ssp, this->cpu.a[i]);
					} else {
						this->cpu.usp -= 4;
						writeWord(this->cpu.usp, this->cpu.a[i]);
					}
				}
				uword >>= 1;
			}
		}
	}
	this->cpu.pc += 2;
}

void mc68k::MOVEPhandler(void) {
	unsigned char	size;
	unsigned short	value;

	size = (this->cpu.instruction_reg & 0x0040);
	value = this->readWord(this->cpu.pc);
	this->cpu.pc += 2;
	if (this->cpu.instruction_reg & 0x0080) {	// From register to memory
		if (size) {	// MOVEP.L
			writeByte(this->cpu.a[(this->cpu.instruction_reg & 0x0007)] + signExtend(value)    , ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00 >> 9)] & 0xFF000000) >> 24));
			writeByte(this->cpu.a[(this->cpu.instruction_reg & 0x0007)] + signExtend(value) + 2, ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00 >> 9)] & 0x00FF0000) >> 16));
			writeByte(this->cpu.a[(this->cpu.instruction_reg & 0x0007)] + signExtend(value) + 4, ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00 >> 9)] & 0x0000FF00) >> 8));
			writeByte(this->cpu.a[(this->cpu.instruction_reg & 0x0007)] + signExtend(value) + 6, ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00 >> 9)] & 0x000000FF)));
		} else {	// MOVEP.W
			writeByte(this->cpu.a[(this->cpu.instruction_reg & 0x0007)] + signExtend(value)    , ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00 >> 9)] & 0x0000FF00) >> 8));
			writeByte(this->cpu.a[(this->cpu.instruction_reg & 0x0007)] + signExtend(value) + 2, ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00 >> 9)] & 0x000000FF)));						
		}
	} else {									// From memory to register
		this->cpu.d[(this->cpu.instruction_reg & 0x0E00 >> 9)]  = (this->readByte(this->cpu.a[(this->cpu.instruction_reg & 0x0007)] + signExtend(value)) << 24);
		this->cpu.d[(this->cpu.instruction_reg & 0x0E00 >> 9)] |= (this->readByte(this->cpu.a[(this->cpu.instruction_reg & 0x0007)] + signExtend(value)) << 16);
		if (size) {
			this->cpu.d[(this->cpu.instruction_reg & 0x0E00 >> 9)] |= (this->readByte(this->cpu.a[(this->cpu.instruction_reg & 0x0007)] + signExtend(value)) << 8);
			this->cpu.d[(this->cpu.instruction_reg & 0x0E00 >> 9)] |= (this->readByte(this->cpu.a[(this->cpu.instruction_reg & 0x0007)] + signExtend(value)));
		}
	}
}

void mc68k::MULUhandler(void) {
	unsigned int	value;

	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x01);

	this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF) * (value & 0x0000FFFF);
	this->cpu.sr.flag.c = false;
	this->cpu.sr.flag.v = false;
	this->cpu.sr.flag.z = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] == 0);
	this->cpu.sr.flag.n = ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x80000000) == 0x80000000);	// Can a MULU even produce a negative result?
}

void mc68k::MULShandler(void) {
	unsigned int	value;

	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x01);

	this->cpu.sr.flag.n = (((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x00008000) == 1) ^ ((value & 0x00008000) == 1));
	this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF) * (value & 0x0000FFFF);
	this->cpu.sr.flag.c = false;
	this->cpu.sr.flag.v = false;
	this->cpu.sr.flag.z = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] == 0);
}

void mc68k::NBCDhandler(void) {
	unsigned int	value, result;

	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x00);
	if ((value & 0x0F) > 9)
		result = 9;
	else
		result = (value & 0x0F);
	if (((value & 0xF0) >> 4) > 9)
		result += 90;
	else
		result += (((value & 0xF0) >> 4) * 10);
	this->cpu.sr.flag.c = (result != 0);
	result = 100 - result;
	if (this->cpu.sr.flag.x)	// check for X flag
		result--;
	this->cpu.sr.flag.x = this->cpu.sr.flag.c;
	this->cpu.sr.flag.z = this->cpu.sr.flag.z & (result == 0);
	this->rewindPC(this->cpu.instruction_reg & 0x3F);
	this->setOperandValue(this->cpu.instruction_reg & 0x3F, 0x00, ((result % 10) + ((result / 10) << 4)));
}

void mc68k::NEGhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
	result = 0xFFFFFFFF - value + 1;
	if ((this->cpu.instruction_reg & 0x0400) && this->cpu.sr.flag.x)				// Check X flag for NEGX
		result--;
	switch (size) {
		case 0x00 :					// NEGX.B
			this->cpu.sr.flag.c = ((value | result) & 0x80) !=0;
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x80) == 0x80);
			this->cpu.sr.flag.v = (0x80 & result & value) != 0;
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			break;
		case 0x01 :					// NEGX.W
			this->cpu.sr.flag.c = ((value | result) & 0x8000) !=0;
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x8000) == 0x8000);
			this->cpu.sr.flag.v = (0x8000 & result & value) != 0;
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			break;
		case 0x02 :					// NEGX.L
			this->cpu.sr.flag.c = ((value | result) & 0x80000000) !=0;
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
			this->cpu.sr.flag.v = (0x80000000 & result & value) != 0;
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			break;
	}
	this->rewindPC(this->cpu.instruction_reg & 0x3F);
	this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, value);
}

void mc68k::NOThandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);

	switch (size) {
		case 0x00 :					// NOT.B
			result = ~(value & 0xFF);
			this->cpu.sr.flag.z = ((result & 0x000000FF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00000080) == 0x80);
			this->cpu.sr.flag.v = false;
			this->cpu.sr.flag.c = false;
			break;
		case 0x01 :					// NOT.W
			result = ~(value & 0xFFFF);
			this->cpu.sr.flag.z = ((result & 0x0000FFFF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
			this->cpu.sr.flag.v = false;
			this->cpu.sr.flag.c = false;
			break;
		case 0x02 :					// NOT.L
			result = ~value;
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
			this->cpu.sr.flag.v = false;
			this->cpu.sr.flag.c = false;
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
	this->rewindPC(this->cpu.instruction_reg & 0x3F);
	this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
}

void mc68k::ORhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
	this->cpu.sr.flag.c = false;
	this->cpu.sr.flag.v = false;
	switch (size) {
		case 0x00 :
			if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
				result = (value & 0xFFFFFF00) | ((value & 0x000000FF) | (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF));
			else									// Destination is Dx
				result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFFFF00) | ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF) | (value & 0x000000FF));
			this->cpu.sr.flag.z = ((result & 0x000000FF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00000080) == 0x80);
			break;
		case 0x01 :
			if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
				result = (value & 0xFFFF0000) | ((value & 0x0000FFFF) | (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF));
			else									// Destination is Dx
				result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFF0000) | ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF) | (value & 0x0000FFFF));
			this->cpu.sr.flag.z = ((result & 0x0000FFFF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
			break;
		case 0x02 :
			if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
				result = (value | this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]);
			else									// Destination is Dx
				result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] | value);
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = (result & 0x80000000) == 0x80000000;
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
	if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
		this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
	else
		this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = result;
}

void mc68k::ORIhandler(void) {
	unsigned char	size;
	unsigned int	value;

	if (this->cpu.instruction_reg == 0x003C) {	// ORI to CCR
		value = this->readWord(this->cpu.pc);
		this->cpu.sr.reg |= (value & 0x000000FF);
		this->cpu.pc += 2;
	}
	if (this->cpu.instruction_reg == 0x007C) {	// ORI to SR
		if (this->cpu.sr.flag.s) {
			value = this->readWord(this->cpu.pc);
			this->cpu.pc += 2;
			this->cpu.sr.reg |= (value & 0x0000FFFF);
		} else
			this->pendingException = PRIVILEGE_VIOLATION;
	}
	if ((this->cpu.instruction_reg & 0xFFBF) != 0x003C) {	// ORI
		size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
		value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
		this->cpu.sr.flag.c = false;
		this->cpu.sr.flag.v = false;
		switch (size) {
			case 0x00 :					// ORI.B
				value |= (this->readWord(this->cpu.pc) & 0xFF);
				this->cpu.pc += 2;
				this->cpu.sr.flag.z = ((value & 0x000000FF) == 0);
				this->cpu.sr.flag.n = ((value & 0x00000080) == 0x80);
				break;
			case 0x01 :					// ORI.W
				value |= this->readWord(this->cpu.pc);
				this->cpu.pc += 2;
				this->cpu.sr.flag.z = ((value & 0x0000FFFF) == 0);
				this->cpu.sr.flag.n = ((value & 0x00008000) == 0x8000);
				break;
			case 0x02 :					// ORI.L
				value |= this->readLong(this->cpu.pc);
				this->cpu.pc += 4;
				this->cpu.sr.flag.z = (value == 0);
				this->cpu.sr.flag.n = ((value &0x80000000) == 0x80000000);
				break;
			case 0x03 :
				this->pendingException = ILLEGAL_INSTRUCTION;
				return;
				break;
		}
		this->rewindPC(this->cpu.instruction_reg & 0x3F);
		this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, value);
	}
}

void mc68k::ROxROXx_handler(void) {
	unsigned char	size, rotation;
	unsigned int	result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	this->cpu.sr.flag.v = false;
	if (size != 0x03) {
		if (this->cpu.instruction_reg & 0x0020)
			rotation = (this->cpu.d[((this->cpu.instruction_reg & 0x0E00) >> 9)] & 0x3F);	// Dn modulo 64 mode
		else {
			rotation = ((this->cpu.instruction_reg & 0x0E00) >> 9);						// Immediate mode
			if (rotation == 0)
				rotation = 8;
		}
	} else {
		rotation = 0;
	}

	while (rotation > 0) {
		switch (size) {
			case 0x00 :
				if (this->cpu.instruction_reg & 0x0100) {	// Rotate left
					this->cpu.sr.flag.c = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x00000080) != 0);
					if (this->cpu.instruction_reg & 0x0100)	// ROL
						result = (((this->cpu.d[this->cpu.instruction_reg & 0x0007] << 1) & 0x000000FF) | (this->cpu.sr.flag.c == true));
					else {									// ROXL
						result = (((this->cpu.d[this->cpu.instruction_reg & 0x0007] << 1) & 0x000000FF) | (this->cpu.sr.flag.x == true));
						this->cpu.sr.flag.x = this->cpu.sr.flag.c;
					}
				} else {									// Rotate right
					this->cpu.sr.flag.c = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x00000001) != 0);
					if (this->cpu.instruction_reg & 0x0100)	// ROR
						result = (((this->cpu.d[this->cpu.instruction_reg & 0x0007] >> 1) & 0x000000FF) | ((this->cpu.sr.flag.c == true) << 7));
					else {									// ROXR
						result = (((this->cpu.d[this->cpu.instruction_reg & 0x0007] >> 1) & 0x000000FF) | ((this->cpu.sr.flag.x == true) << 7));
						this->cpu.sr.flag.x = this->cpu.sr.flag.c;
					}
				}
				this->cpu.d[this->cpu.instruction_reg & 0x0007] = (this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0xFFFFFF00) | (result & 0x000000FF);
				break;
			case 0x01 :
				if (this->cpu.instruction_reg & 0x0100) {	// Rotate left
					this->cpu.sr.flag.c = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x00008000) != 0);
					if (this->cpu.instruction_reg & 0x0100)	// ROL
						result = (((this->cpu.d[this->cpu.instruction_reg & 0x0007] << 1) & 0x0000FFFF) | (this->cpu.sr.flag.c == true));
					else {									// ROXL
						result = (((this->cpu.d[this->cpu.instruction_reg & 0x0007] << 1) & 0x0000FFFF) | (this->cpu.sr.flag.x == true));
						this->cpu.sr.flag.x = this->cpu.sr.flag.c;
					}
				} else {									// Rotate right
					this->cpu.sr.flag.c = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x00000001) != 0);
					if (this->cpu.instruction_reg & 0x0100)	// ROR
						result = (((this->cpu.d[this->cpu.instruction_reg & 0x0007] >> 1) & 0x0000FFFF) | ((this->cpu.sr.flag.c == true) << 15));
					else {									// ROXR
						result = (((this->cpu.d[this->cpu.instruction_reg & 0x0007] >> 1) & 0x0000FFFF) | ((this->cpu.sr.flag.x == true) << 15));
						this->cpu.sr.flag.x = this->cpu.sr.flag.c;
					}
				}
				this->cpu.d[this->cpu.instruction_reg & 0x0007] = (this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0xFFFF0000) | (result & 0x0000FFFF);
				break;
			case 0x02 :
				if (this->cpu.instruction_reg & 0x0100) {	// Rotate left
					this->cpu.sr.flag.c = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x80000000) != 0);
					if (this->cpu.instruction_reg & 0x0100)	// ROL
						result = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] << 1) | (this->cpu.sr.flag.c == true));
					else {									// ROXL
						result = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] << 1) | (this->cpu.sr.flag.x == true));
						this->cpu.sr.flag.x = this->cpu.sr.flag.c;
					}
				} else {									// Rotate right
					this->cpu.sr.flag.c = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x00000001) != 0);
					if (this->cpu.instruction_reg & 0x0100)	// ROR
						result = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] >> 1) | ((this->cpu.sr.flag.c == true) << 31));
					else {									// ROXR
						result = ((this->cpu.d[this->cpu.instruction_reg & 0x0007] >> 1) | ((this->cpu.sr.flag.x == true) << 31));
						this->cpu.sr.flag.x = this->cpu.sr.flag.c;
					}
				}
				this->cpu.d[this->cpu.instruction_reg & 0x0007] = result;
				break;
			case 0x03 :
				WriteLog("mc68k::ROxROXx_handler -- Unimplemented instruction %04X at %08X\n", this->cpu.instruction_reg, this->cpu.pc);
				break;
		}
		rotation--;
	}
}

void mc68k::SBCDhandler(void) {
	unsigned int	value, value2, result, result2;

	if (this->cpu.instruction_reg & 0x0008) {			// -(An) memory to memory operation
		value = this->getOperandValue(((this->cpu.instruction_reg & 0x0007) | 0x0008), 0x00);			// Source value
		value2 = this->getOperandValue((((this->cpu.instruction_reg & 0x0E00) >> 9) | 0x0008), 0x00);	// Destination value
	} else {											// Dn to Dn operation
		value = this->cpu.d[(this->cpu.instruction_reg & 0x0007)];
		value2 = this->cpu.d[((this->cpu.instruction_reg & 0x0E00) >> 9)];
	}

	if ((value & 0x0F) > 9)
		result = 9;
	else
		result = (value & 0x0F);
	if (((value & 0xF0) >> 4) > 9)
		result += 90;
	else
		result += (((value & 0xF0) >> 4) * 10);

	if ((value2 & 0x0F) > 9)
		result2 = 9;
	else
		result2 = (value2 & 0x0F);
	if (((value2 & 0xF0) >> 4) > 9)
		result2 += 90;
	else
		result2 += (((value2 & 0xF0) >> 4) * 10);

	result = result2 - result;
	if (this->cpu.sr.flag.x)	// check for X flag
		result--;
	if (result & 0x80000000) {
		result += 100;
		this->cpu.sr.flag.c = true;
		this->cpu.sr.flag.x = true;
	}
	this->cpu.sr.flag.z = this->cpu.sr.flag.z & (result == 0);
	if (this->cpu.instruction_reg & 0x0008)					// -(An) memory to memory operation
		this->setOperandValue((((this->cpu.instruction_reg & 0x0E00) >> 9) | 0x0008), 0x00, ((result % 10) + ((result / 10) << 4)));
	else
		this->cpu.d[((this->cpu.instruction_reg & 0x0E00) >> 9)] = result;
}

void mc68k::SUBhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
	switch (size) {
		case 0x00 :
			if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
				result = (value & 0x000000FF) | ((value & 0x000000FF) - (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF));
			else									// Destination is Dx
				result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF) | ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF) - (value & 0x000000FF));
			this->cpu.sr.flag.c = (((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&0x80)==0) && (((value|result)&0x80)!=0)) || ((value&result&0x80)!=0);
			this->cpu.sr.flag.v = ((0x80&(value^0x80)&this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&(result^0x80))!=0)||((0x80&result&(this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]^0x80)&value)!=0);
			this->cpu.sr.flag.z = ((result & 0x000000FF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00000080) == 0x80);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			break;
		case 0x01 :
			if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
				result = (value & 0x0000FFFF) | ((value & 0x0000FFFF) - (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF));
			else									// Destination is Dx
				result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF) | ((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF) - (value & 0x0000FFFF));
			this->cpu.sr.flag.c = (((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&0x8000)==0) && (((value|result)&0x8000)!=0)) || ((value&result&0x8000)!=0);
			this->cpu.sr.flag.v = ((0x8000&(value^0x8000)&this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&(result^0x8000))!=0)||((0x8000&result&(this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]^0x8000)&value)!=0);
			this->cpu.sr.flag.z = ((result & 0x0000FFFF) == 0);
			this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			break;
		case 0x02 :
			if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
				result = (value - this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]);
			else									// Destination is Dx
				result = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] - value);
			this->cpu.sr.flag.c = (((this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&0x80000000)==0) && (((value|result)&0x80000000)!=0)) || ((value&result&0x80000000)!=0);
			this->cpu.sr.flag.v = ((0x80000000&(value^0x80000000)&this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]&(result^0x80000000))!=0)||((0x80000000&result&(this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9]^0x80000000)&value)!=0);
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = (result & 0x80000080) == 0x80000000;
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
	if (this->cpu.instruction_reg & 0x0100)		// Destination is operand
		this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
	else
		this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = result;
}

void mc68k::SUBQhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
	switch (size) {
		case 0x00 :
			result = (value & 0xFFFFFF00) | ((value - ((this->cpu.instruction_reg & 0x0E00) >> 9)) & 0x000000FF);
			this->cpu.sr.flag.c = (((result&0x80)==0) && ((value&0x80)!=0));
			this->cpu.sr.flag.v = (0x80&result&(value^0x80))!=0;
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x80) == 0x80);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
			break;
		case 0x01 :
			result = (value & 0xFFFF0000) | ((value - ((this->cpu.instruction_reg & 0x0E00) >> 9)) & 0x0000FFFF);
			this->cpu.sr.flag.c = (((result&0x8000)==0) && ((value&0x8000)!=0));
			this->cpu.sr.flag.v = (0x8000&result&(value^0x8000))!=0;
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x8000) == 0x8000);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
			break;
		case 0x02 :
			result = value - ((this->cpu.instruction_reg & 0x0E00) >> 9);
			this->cpu.sr.flag.c = (((result&0x80)==0) && ((value&0x80)!=0));
			this->cpu.sr.flag.v = (0x80000000&result&(value^0x80000000))!=0;
			this->cpu.sr.flag.z = (result == 0);
			this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
			this->cpu.sr.flag.x = this->cpu.sr.flag.c;
			this->setOperandValue(this->cpu.instruction_reg & 0x3F, size, result);
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
}

void mc68k::SUBXhandler(void) {
	unsigned char	size;
	unsigned int	value, result;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	if (this->cpu.instruction_reg & 0x0008) {			// -(An) memory to memory operation
		switch (size) {
			case 0x00 :
				value = this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF;
				result = (this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x000000FF) - value - this->cpu.sr.flag.x;
				this->cpu.sr.flag.c = (((value&0x80)==0) && ((((this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x000000FF)|result)&0x80)!=0)) || (((this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x000000FF)&result&0x80)!=0);;
				this->cpu.sr.flag.v = ((0x80&((this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x000000FF)^0x80)&value&(result^0x80))!=0)||((0x80&result&(value^0x80)&(this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x000000FF))!=0);;
				this->cpu.sr.flag.n = ((result & 0x00000080) == 0x80);
				this->cpu.sr.flag.z = this->cpu.sr.flag.z & ((result & 0x000000FF) == 0);
				this->cpu.sr.flag.x = this->cpu.sr.flag.c;
				this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = (this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFFFF00) | (result & 0x000000FF);
				break;
			case 0x01 :
				value = this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF;
				result = (this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF) - value - this->cpu.sr.flag.x;
				this->cpu.sr.flag.c = (((value&0x8000)==0) && ((((this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF)|result)&0x8000)!=0)) || (((this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF)&result&0x8000)!=0);;
				this->cpu.sr.flag.v = ((0x8000&((this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF)^0x8000)&value&(result^0x8000))!=0)||((0x8000&result&(value^0x8000)&(this->cpu.a[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF))!=0);;
				this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
				this->cpu.sr.flag.z = this->cpu.sr.flag.z & ((result & 0x0000FFFF) == 0);
				this->cpu.sr.flag.x = this->cpu.sr.flag.c;
				this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = (this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFF0000) | (result & 0x0000FFFF);
				break;
			case 0x02 :
				value = this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9];
				result = this->cpu.a[this->cpu.instruction_reg & 0x0007] - value - this->cpu.sr.flag.x;
				this->cpu.sr.flag.c = (((value&0x80000000)==0) && (((this->cpu.a[this->cpu.instruction_reg & 0x0007]|result)&0x80000000)!=0)) || ((this->cpu.a[this->cpu.instruction_reg & 0x0007]&result&0x80000000)!=0);;
				this->cpu.sr.flag.v = ((0x80000000&(this->cpu.a[this->cpu.instruction_reg & 0x0007]^0x8000)&value&(result^0x80000000))!=0)||((0x80000000&result&(value^0x80000000)&this->cpu.a[this->cpu.instruction_reg & 0x0007])!=0);;
				this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
				this->cpu.sr.flag.z = this->cpu.sr.flag.z & (result == 0);
				this->cpu.sr.flag.x = this->cpu.sr.flag.c;
				this->cpu.a[(this->cpu.instruction_reg & 0x0E00) >> 9] = result;
				break;
			case 0x03 :
				this->pendingException = ILLEGAL_INSTRUCTION;
				return;
				break;
		}
	} else {										// Dn to Dn operation
		switch (size) {
			case 0x00 :
				value = this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x000000FF;
				result = (this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x000000FF) - value - this->cpu.sr.flag.x;
				this->cpu.sr.flag.c = (((value&0x80)==0) && ((((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x000000FF)|result)&0x80)!=0)) || (((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x000000FF)&result&0x80)!=0);;
				this->cpu.sr.flag.v = ((0x80&((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x000000FF)^0x80)&value&(result^0x80))!=0)||((0x80&result&(value^0x80)&(this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x000000FF))!=0);;
				this->cpu.sr.flag.n = ((result & 0x00000080) == 0x80);
				this->cpu.sr.flag.z = this->cpu.sr.flag.z & ((result & 0x000000FF) == 0);
				this->cpu.sr.flag.x = this->cpu.sr.flag.c;
				this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFFFF00) | (result & 0x000000FF);
				break;
			case 0x01 :
				value = this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0x0000FFFF;
				result = (this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF) - value - this->cpu.sr.flag.x;
				this->cpu.sr.flag.c = (((value&0x8000)==0) && ((((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF)|result)&0x8000)!=0)) || (((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF)&result&0x8000)!=0);;
				this->cpu.sr.flag.v = ((0x8000&((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF)^0x8000)&value&(result^0x8000))!=0)||((0x8000&result&(value^0x8000)&(this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0x0000FFFF))!=0);;
				this->cpu.sr.flag.n = ((result & 0x00008000) == 0x8000);
				this->cpu.sr.flag.z = this->cpu.sr.flag.z & ((result & 0x0000FFFF) == 0);
				this->cpu.sr.flag.x = this->cpu.sr.flag.c;
				this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = (this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] & 0xFFFF0000) | (result & 0x0000FFFF);
				break;
			case 0x02 :
				value = this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9];
				result = this->cpu.d[this->cpu.instruction_reg & 0x0007] - value - this->cpu.sr.flag.x;
				this->cpu.sr.flag.c = (((value&0x80000000)==0) && (((this->cpu.d[this->cpu.instruction_reg & 0x0007]|result)&0x80000000)!=0)) || ((this->cpu.d[this->cpu.instruction_reg & 0x0007]&result&0x80000000)!=0);;
				this->cpu.sr.flag.v = ((0x80000000&(this->cpu.d[this->cpu.instruction_reg & 0x0007]^0x8000)&value&(result^0x80000000))!=0)||((0x80000000&result&(value^0x80000000)&this->cpu.d[this->cpu.instruction_reg & 0x0007])!=0);;
				this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
				this->cpu.sr.flag.z = this->cpu.sr.flag.z & (result == 0);
				this->cpu.sr.flag.x = this->cpu.sr.flag.c;
				this->cpu.d[(this->cpu.instruction_reg & 0x0E00) >> 9] = result;
				break;
			case 0x03 :
				this->pendingException = ILLEGAL_INSTRUCTION;
				return;
				break;
		}
	}
}

void mc68k::SWAPhandler(void) {
	unsigned int	value, result;

	value = this->cpu.d[this->cpu.instruction_reg & 0x0007];
	result = (((this->cpu.d[this->cpu.instruction_reg & 0x0007] & 0xFFFF0000) >> 16) | ((value & 0x0000FFFF) << 16));
	this->cpu.d[this->cpu.instruction_reg & 0x0007] = result;
	this->cpu.sr.flag.n = ((result & 0x80000000) == 0x80000000);
	this->cpu.sr.flag.z = (result == 0);
	this->cpu.sr.flag.v = false;
	this->cpu.sr.flag.c = false;
}

void mc68k::TAShandler(void) {
	unsigned int	value;

	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, 0x00);

	this->cpu.sr.flag.z = ((value & 0x000000FF) == 0);
	this->cpu.sr.flag.n = ((value & 0x00000080) == 0x80);
	this->cpu.sr.flag.c = false;
	this->cpu.sr.flag.v = false;

	this->rewindPC(this->cpu.instruction_reg & 0x3F);
	this->setOperandValue(this->cpu.instruction_reg & 0x3F, 0x00, (value | 0x00000080));
}

void mc68k::TSThandler(void) {
	unsigned char	size;
	unsigned int	value;

	size = ((this->cpu.instruction_reg & 0x00C0) >> 6);
	value = this->getOperandValue(this->cpu.instruction_reg & 0x3F, size);
	this->cpu.sr.flag.c = false;
	this->cpu.sr.flag.v = false;
	switch (size) {
		case 0x00 :
			this->cpu.sr.flag.z = ((value & 0x000000FF) == 0);
			this->cpu.sr.flag.n = ((value & 0x00000080) == 0x80);
			break;
		case 0x01 :
			this->cpu.sr.flag.z = ((value & 0x0000FFFF) == 0);
			this->cpu.sr.flag.n = ((value & 0x00008000) == 0x8000);
			break;
		case 0x02 :
			this->cpu.sr.flag.z = (value == 0);
			this->cpu.sr.flag.n = ((value & 0x80000000) == 0x80000000);
			break;
		case 0x03 :
			this->pendingException = ILLEGAL_INSTRUCTION;
			return;
			break;
	}
}

void mc68k::UNLINKhandler(void) {
	if (this->cpu.sr.flag.s)
		this->cpu.ssp = this->cpu.a[this->cpu.instruction_reg & 0x0007];
	else
		this->cpu.usp = this->cpu.a[this->cpu.instruction_reg & 0x0007];
	this->cpu.a[this->cpu.instruction_reg & 0x0007] = this->readLong(this->cpu.usp);
	if (this->cpu.sr.flag.s)
		this->cpu.ssp += 2;
	else
		this->cpu.usp += 2;
}
