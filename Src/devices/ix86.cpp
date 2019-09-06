/* Intel x86 TUBE module for the BeebEm emulator
  Written by Eelco Huininga 2016
https://en.wikipedia.org/wiki/X86_instruction_listings
http://mlsite.net/8086/
https://pdos.csail.mit.edu/6.828/2012/readings/i386/c17.htm
http://www.scs.stanford.edu/05au-cs240c/lab/i386/s17_02.htm
http://ref.x86asm.net/coder.html
http://ref.x86asm.net/coder32.html
http://aturing.umcs.maine.edu/~meadow/courses/cos335/8086-instformat.pdf
*/

#include <stdio.h>				// Included for FILE *
#include <string.h>				// Included for strcpy, strcat

#include "ix86.h"
#include "../beebmem.h"				// Included for RomPath variable
#include "../main.h"				// Included for WriteLog()
#include "../tube.h"				// Included for ReadTubeFromParasiteSide() and WriteTubeFromParasiteSide()

int Enable_x86 = 0;
int x86Tube = 0;



/* Constructor / Deconstructor */

ix86::ix86(int cpuType, int fpuType, unsigned int clockSpeed, unsigned char *readMemory, unsigned char *writeMemory, unsigned char *readIO, unsigned char *writeIO) {
	this->cpu.type		= cpuType;
	this->fpu.type		= fpuType;
	this->clockSpeed	= clockSpeed;
	this->readMemory	= readMemory;
	this->writeMemory	= writeMemory;
	this->readIO		= readIO;
	this->writeIO		= writeIO;

	switch (this->cpu.type) {
		case I8088 :
		case NECV20 :
		case I8086 :
		case NECV30 :
		case I80188 :
		case I80186 :
			// The i80188/i80186 have built-in DMA-, PIC- and PIT-controllers
//			dma	= new i8237;
//			pic	= new i8253;
//			pit	= new i8259;
			// 20-bit address bus (1 megabyte addressing space)
			INTERNAL_ADDRESS_MASK = 0x000FFFFF;
			EXTERNAL_ADDRESS_MASK = 0x000FFFFF;
			break;

		case I80286 :
			// 24-bit address bus (16 megabyte addressing space)
			INTERNAL_ADDRESS_MASK = 0x00FFFFFF;
			EXTERNAL_ADDRESS_MASK = 0x00FFFFFF;
			break;

		default :
			// This should never happen
			WriteLog("ix86::ix86 BUG FOUND: Unknown CPU type %i", cpuType);
			break;
	}

	switch (this->fpu.type) {
		case I8087 :
		case I80287 :
//			this->fpu	= new ix87(this->fpu.type);
			break;

		default :
//			this->fpu	= null;
			break;
	}
}

ix86::~ix86(void) {
	free(ramMemory);
	free(romMemory);
}



/* Main code */

void ix86::Reset(void) {
	switch (this->cpu.type) {
		case I8088 :
		case NECV20 :
		case I8086 :
		case NECV30 :
		case I80188 :
		case I80186 :
			this->cpu.ax.x		= 0x0000;	// Accumulator
			this->cpu.bx.x		= 0x0000;	// Base
			this->cpu.cx.x		= 0x0000;	// Count
			this->cpu.dx.x		= 0x0000;	// Data
			this->cpu.si		= 0x0000;	// Source Index
			this->cpu.di		= 0x0000;	// Destination Index
			this->cpu.bp		= 0x0000;	// Base Pointer
			this->cpu.sp		= 0x0000;	// Stack Pointer
			this->cpu.ds		= 0x0000;	// Data Segment
			this->cpu.es		= 0x0000;	// Extra Segment (External / Global Data)
			this->cpu.ss		= 0x0000;	// Stack Segment
			this->cpu.ip		= 0x0000;	// Instruction Pointer (Program counter)
			this->cpu.cs		= 0xFFFF;	// Code Segment
			this->cpu.flags.reg	= 0xF002;	// Flags
			this->cpu.relocreg	= 0x20FF;	// Relocation register
			this->cpu.umcs		= 0xFFFB;	// Upper Memory CS
			this->cpu.mmcs		= 0x0000;	// Mid-range Memory CS
			this->cpu.mpcs		= 0x0000;	// Mid-range Memory CS
			this->cpu.lmcs		= 0x0000;	// Lower Memory CS
			this->cpu.pacs		= 0x0000;	// Peripheral Address CS
			break;

		case I80286 :
			this->cpu.ax.x		= 0x0000;	// Accumulator
			this->cpu.bx.x		= 0x0000;	// Base
			this->cpu.cx.x		= 0x0000;	// Count
			this->cpu.dx.x		= 0x0000;	// Data
			this->cpu.si		= 0x0000;	// Source Index
			this->cpu.di		= 0x0000;	// Destination Index
			this->cpu.bp		= 0x0000;	// Base Pointer
			this->cpu.sp		= 0x0000;	// Stack Pointer
			this->cpu.ds		= 0x0000;	// Data Segment
			this->cpu.es		= 0x0000;	// Extra Segment (External / Global Data)
			this->cpu.ss		= 0x0000;	// Stack Segment
			this->cpu.msw.reg	= 0xFFF0;	// Machine Status Word
			this->cpu.ip		= 0xFFF0;	// Instruction Pointer (Program counter)
			this->cpu.cs		= 0xF000;	// Code Segment
			this->cpu.flags.reg	= 0x0002;	// Flags
			break;

		default :
			break;
	}

	switch (this->fpu.type) {
		case I8087 :
		case I80187 :
		case I80287 :
			this->cpu.msw.reg	= 0x0004;	// Set EM bit
			break;

		default :
			this->cpu.msw.reg	= 0x0000;
			break;
	}

	this->cpu.halt			= false;			// CPU is in running mode
	this->cpu.wait			= false;			// CPU WAIT signal is not asserted
	this->cyclecount		= 1000000;
	this->pendingInterrupt	= -1;				// Clear any pending interrupts
	this->DEBUG				= false;
}

void ix86::Exec(int Cycles) {
	unsigned char	mod, reg, rm, segmentOverride, level;
	bool			s, w, rep;
	unsigned short	rep_pc, quotient, remainder;
	unsigned int	temp, disp, result;


	while (Cycles > 0) {
		/* Check for interrupts */
		if (this->intr)
			this->pendingInterrupt = this->interrupt_nr;

		/* Check for non-maskable interrupts */
		if (this->nmi)
			this->pendingInterrupt = 2;

		/* Process any pending interrupts first */
		if (this->pendingInterrupt != -1) {
			this->cpu.sp -= 2;
			this->writeMemWord(this->getAddress(SS, SP), this->cpu.flags.reg);

			this->cpu.sp -= 2;
			this->writeMemWord(this->getAddress(SS, SP), this->cpu.cs);

			this->cpu.sp -= 2;
			this->writeMemWord(this->getAddress(SS, SP), this->cpu.ip);

			this->cpu.flags.in = false;
			this->cpu.flags.tf = false;
			this->cpu.ip = this->readMemWord(this->pendingInterrupt * 4);
			this->cpu.cs = this->readMemWord((this->pendingInterrupt * 4) + 2);

//			TODO: Update cycles
		}

		if ((!this->cpu.halt) && (this->cyclecount > 0)) {
			if (rep == true)
				rep_pc = this->cpu.ip;					// Remember the current IP if the previous opcode was a REP

			this->cpu.instruction_reg.reg = this->readMemByte(this->getAddress(CS, IP));
			this->dumpRegisters();
			this->cpu.ip++;

			s = (this->cpu.instruction_reg.reg & 0x02) ? true : false;
			w = (this->cpu.instruction_reg.reg & 0x01) ? true : false;

			switch (this->cpu.instruction_reg.reg) {
				/* ADD */
				case 0x00 :
				case 0x01 :
				case 0x02 :
				case 0x03 :
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					temp = this->getRegisterValue(w, reg);
					disp = this->getOperandValue(w, mod, rm);
					result = temp + disp;

					this->setOverflowFlag(w, result, temp, disp);
					this->setSignFlag(w, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(w, result);
					this->setParityFlag(w, result);

					if (s)
						this->setRegisterValue(w, reg, result);
					else
						this->setOperandValue(w, mod, rm, result);
//					TODO: update cycles
					break;

				/* ADD AL, imm */
				case 0x04 :
					temp = this->cpu.ax.l;
					disp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;
					result = this->cpu.ax.l + disp;

					this->setOverflowFlag(false, result, temp, disp);
					this->setSignFlag(false, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(false, result);
					this->setParityFlag(false, result);

					this->cpu.ax.l = result;
					Cycles += 4;
					break;

				case 0x05 :	// ADD [E]AX, imm
					temp = this->cpu.ax.x;
					disp = this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;
					result = temp + disp;

					this->setOverflowFlag(true, result, temp, disp);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(true, result);
					this->setParityFlag(true, result);

					this->cpu.ax.x = result;
					Cycles += 2;
					break;

				case 0x06 :	// PUSH ES
					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.es);
					Cycles += 2;
					break;

				case 0x07 :	// POP ES
					this->cpu.es = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 7;
					break;

				case 0x08 :	// OR
				case 0x09 :
				case 0x0A :
				case 0x0B :
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					temp = (this->getRegisterValue(w, reg) | this->getOperandValue(w, mod, rm));

					if (s)
						this->setRegisterValue(w, reg, temp);
					else
						this->setOperandValue(w, mod, rm, temp);

					this->cpu.flags.of = false;
					this->setSignFlag(w, temp);
					this->setZeroFlag(temp);
					this->cpu.flags.cf = false;
					this->setParityFlag(w, temp);
//					TODO: update cycles
					break;

				case 0x0C :	// OR AL, imm8
					this->cpu.ax.l |= this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					this->cpu.flags.of = false;
					this->setSignFlag(false, this->cpu.ax.l);
					this->setZeroFlag(this->cpu.ax.l);
					this->cpu.flags.cf = false;
					this->setParityFlag(false, this->cpu.ax.l);

					Cycles += 2;
					break;

				case 0x0D :	// OR [E]AX, imm
					this->cpu.ax.x |= this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;

					this->cpu.flags.of = false;
					this->setSignFlag(true, this->cpu.ax.x);
					this->setZeroFlag(this->cpu.ax.x);
					this->cpu.flags.cf = false;
					this->setParityFlag(true, this->cpu.ax.x);

					Cycles += 2;
					break;

				case 0x0E :	// PUSH CS
					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.cs);
					break;

				case 0x0F :	// POP CS / Opcode extention
					if (this->cpu.type == I80286) {
						temp = this->readMemByte(this->getAddress(CS, IP));
						this->cpu.ip++;

						switch (temp) {
							case 0x00 :	// Protection control (286+)
								if (this->cpu.msw.pe) {
									temp = this->readMemByte(this->getAddress(CS, IP));
									this->cpu.ip++;

									mod = ((temp & 0xC0) >> 6);
									reg = ((temp & 0x38) >> 3);
									rm  = (temp & 0x07);

									switch (reg) {
										case 0x00 : // SLDT       mod 000 r/m (286+ Protected)
											this->setOperandValue(true, mod, rm, this->cpu.ldtr.base);
											// TODO: Implement instruction
											Cycles += 2; // 2,3
											break;

										case 0x01 : // STR        mod 001 r/m (286+ Protected)
											this->setOperandValue(true, mod, rm, this->cpu.tr);
											Cycles += 2; // 2,3
											break;

										case 0x02 : // LLDT       mod 010 r/m (286+ Protected)
											this->cpu.ldtr.base = this->getOperandValue(true, mod, rm);
											// TODO: Implement instruction
											Cycles += 17; // 17,19
											break;

										case 0x03 : // LTR        mod 011 r/m (286+ Protected)
											this->cpu.tr = this->getOperandValue(true, mod, rm);
											Cycles += 23; // 23,27
											break;

										case 0x04 : // VERR READ  mod 100 r/m (286+ Protected)
						//					TODO: implement instruction
						//					TODO: set flags
											Cycles += 14; // 14,16
											break;

										case 0x05 : // VERR WRITE mod 101 r/m (286+ Protected)
						//					TODO: implement instruction
						//					TODO: set flags
											Cycles += 14; // 14,16
											break;

										default :   // Illegal opcode
											pendingInterrupt = 0x06;
											break;
									}
									break;
								} else
									pendingInterrupt = 0x06;
								break;

							case 0x01 :	// Protection control (286+)
								temp = this->readMemByte(this->getAddress(CS, IP));
								this->cpu.ip++;

								mod = ((temp & 0xC0) >> 6);
								reg = ((temp & 0x38) >> 3);
								rm  = (temp & 0x07);

								switch (reg) {
									case 0x00 : // SGDT       mod 000 r/m (286+)
										this->setOperandValue(true, mod, rm, this->cpu.gdtr.base);
										Cycles += 9;
										break;

									case 0x01 : // SIDT       mod 001 r/m (286+)
										this->setOperandValue(true, mod, rm, this->cpu.idtr.base);
										Cycles += 9;
										break;

									case 0x02 : // LGDT       mod 010 r/m (286+)
										this->cpu.gdtr.base = this->getOperandValue(true, mod, rm);
										Cycles += 11;
										break;

									case 0x03 : // LIDT       mod 011 r/m (286+)
										this->cpu.idtr.base = this->getOperandValue(true, mod, rm);
										Cycles += 11;
										break;

									case 0x04 : // SMSW       mod 100 r/m (286+)
										this->setOperandValue(true, mod, rm, this->cpu.msw.reg);
					//					TODO: update cycles
										break;

									case 0x06 : // LMSW       mod 011 r/m (286+)
										this->cpu.msw.reg = this->getOperandValue(true, mod, rm);
					//					TODO: update cycles
										break;

									default :   // Illegal opcode
										pendingInterrupt = 0x06;
										break;
								}
								break;

							case 0x02 :	// LAR mod reg r/m (286+)
								if (this->cpu.msw.pe) {
									temp = this->readMemByte(this->getAddress(CS, IP));
									this->cpu.ip++;

									mod = ((temp & 0xC0) >> 6);
									reg = ((temp & 0x38) >> 3);
									rm  = (temp & 0x07);

				//					TODO: implement instruction
				//					TODO: set flags
									Cycles += 14; // 14,16
								} else
									pendingInterrupt = 0x06;
								break;

							case 0x03 :	// LSL mod reg r/m (286+)
								if (this->cpu.msw.pe) {
									temp = this->readMemByte(this->getAddress(CS, IP));
									this->cpu.ip++;

									mod = ((temp & 0xC0) >> 6);
									reg = ((temp & 0x38) >> 3);
									rm  = (temp & 0x07);

				//					TODO: implement instruction
				//					TODO: set flags
									Cycles += 14; // 14,16
								} else
									pendingInterrupt = 0x06;
								break;

							case 0x05 :	// LOADALL (286)
								if (this->cpu.type == I80286) {
									this->cpu.msw.reg	= this->readMemWord(0x00806);
									this->cpu.tr		= this->readMemWord(0x00816);
									this->cpu.flags.reg	= this->readMemWord(0x00818);
									this->cpu.ip		= this->readMemWord(0x0081A);
									this->cpu.ldtr.base	= this->readMemWord(0x0081C);
									this->cpu.ds		= this->readMemWord(0x0081E);
									this->cpu.ss		= this->readMemWord(0x00820);
									this->cpu.cs		= this->readMemWord(0x00822);
									this->cpu.es		= this->readMemWord(0x00824);
									this->cpu.di		= this->readMemWord(0x00826);
									this->cpu.si		= this->readMemWord(0x00828);
									this->cpu.bp		= this->readMemWord(0x0082A);
									this->cpu.sp		= this->readMemWord(0x0082C);
									this->cpu.bx.x		= this->readMemWord(0x0082E);
									this->cpu.dx.x		= this->readMemWord(0x00830);
									this->cpu.cx.x		= this->readMemWord(0x00832);
									this->cpu.ax.x		= this->readMemWord(0x00834);
									this->cpu.esd		= ((this->readMemWord(0x00836) * (2 ^ 32)) | this->readMemLong(0x00838));
									this->cpu.csd		= ((this->readMemWord(0x0083C) * (2 ^ 32)) | this->readMemLong(0x0083E));
									this->cpu.ssd		= ((this->readMemWord(0x00842) * (2 ^ 32)) | this->readMemLong(0x00844));
									this->cpu.dsd		= ((this->readMemWord(0x00848) * (2 ^ 32)) | this->readMemLong(0x0084A));
									this->cpu.gdtr.base	= this->readMemWord(0x0084E);
									this->cpu.gdtr.limit	= this->readMemLong(0x00850);
									this->cpu.ldt		= ((this->readMemWord(0x00854) * (2 ^ 32)) | this->readMemLong(0x00856));
									this->cpu.idt		= ((this->readMemWord(0x0085A) * (2 ^ 32)) | this->readMemLong(0x0085C));
									this->cpu.tss		= ((this->readMemWord(0x00860) * (2 ^ 32)) | this->readMemLong(0x00862));
									Cycles += 190;
								} else
									pendingInterrupt = 0x06;
								break;

							case 0x06 :	// CTS (286+)
								this->cpu.msw.ts = false;
								Cycles += 2;
								break;

							default :
								pendingInterrupt = 0x06;
								break;
						}
					} else {
						/* POP CS (8088/8086) */
						this->cpu.cs = this->readMemWord(this->getAddress(SS, SP));
						this->cpu.sp += 2;
						Cycles += 7;
					}
					break;

				case 0x10 :	// ADC
				case 0x11 :
				case 0x12 :
				case 0x13 :
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					temp = this->getRegisterValue(w, reg);
					disp = this->getOperandValue(w, mod, rm);
					result = temp + disp;
					if (this->cpu.flags.cf)
						result++;

					this->setOverflowFlag(w, result, temp, disp);
					this->setSignFlag(w, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(w, result);
					this->setParityFlag(w, result);

					if (s)
						this->setRegisterValue(w, reg, result);
					else
						this->setOperandValue(w, mod, rm, result);
//					TODO: update cycles
					break;

				case 0x14 :	// ADC AL, imm
					temp = this->cpu.ax.l;
					disp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;
					result = this->cpu.ax.l + disp;
					if (this->cpu.flags.cf)
						result++;

					this->setOverflowFlag(false, result, temp, disp);
					this->setSignFlag(false, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(false, result);
					this->setParityFlag(false, result);

					this->cpu.ax.l = result;
					Cycles += 4;
					break;

				case 0x15 :	// ADC [E]AX, imm
					temp = this->cpu.ax.x;
					disp = this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;
					result = temp + disp;
					if (this->cpu.flags.cf)
						result++;

					this->setOverflowFlag(true, result, temp, disp);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(true, result);
					this->setParityFlag(true, result);

					this->cpu.ax.x = result;

					Cycles += 2;
					break;

				case 0x16 :	// PUSH SS
					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.ss);
					Cycles += 2;
					break;

				case 0x17 :	// POP SS
					this->cpu.ss = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 7;
					break;

				case 0x18 :	// SBB
				case 0x19 :
				case 0x1A :
				case 0x1B :
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					temp = this->getRegisterValue(w, reg);
					disp = this->getOperandValue(w, mod, rm);
					result = temp - disp;
					if (this->cpu.flags.cf)
						result++;

					this->setOverflowFlag(w, result, temp, disp);
					this->setSignFlag(w, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(w, result);
					this->setParityFlag(w, result);

					if (s)
						this->setRegisterValue(w, reg, result);
					else
						this->setOperandValue(w, mod, rm, result);
//					TODO: update cycles
					break;

				case 0x1C :	// SBB AL, imm
					temp = this->cpu.ax.l;
					disp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;
					result = this->cpu.ax.l - disp;
					if (this->cpu.flags.cf)
						result++;

					this->setOverflowFlag(false, result, temp, disp);
					this->setSignFlag(false, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(false, result);
					this->setParityFlag(false, result);

					this->cpu.ax.l = result;
					Cycles += 4;
					break;

				case 0x1D :	// SBB [E]AX, imm
					temp = this->cpu.ax.x;
					disp = this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;
					result = temp - disp;
					if (this->cpu.flags.cf)
						result++;

					this->setOverflowFlag(true, result, temp, disp);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(true, result);
					this->setParityFlag(true, result);

					this->cpu.ax.x = result;

					Cycles += 2;
					break;

				case 0x1E :	// PUSH DS
					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.ds);
					break;

				case 0x1F :	// POP DS
					this->cpu.ds = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 7;
					break;

				case 0x20 :	// AND
				case 0x21 :
				case 0x22 :
				case 0x23 :
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					temp = (this->getRegisterValue(w, reg) & this->getOperandValue(w, mod, rm));

					if (s)
						this->setRegisterValue(w, reg, temp);
					else
						this->setOperandValue(w, mod, rm, temp);

					this->cpu.flags.of = false;
					this->setSignFlag(w, temp);
					this->setZeroFlag(temp);
					this->cpu.flags.cf = false;
					this->setParityFlag(w, temp);
//					TODO: update cycles
					break;

				case 0x24 :	// AND AL, imm
					this->cpu.ax.l &= this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					this->cpu.flags.of = false;
					this->setSignFlag(false, this->cpu.ax.l);
					this->setZeroFlag(this->cpu.ax.l);
					this->cpu.flags.cf = false;
					this->setParityFlag(false, this->cpu.ax.l);

					Cycles += 2;
					break;

				case 0x25 :	// AND [E]AX, imm
					this->cpu.ax.x &= this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;

					this->cpu.flags.of = false;
					this->setSignFlag(true, this->cpu.ax.x);
					this->setZeroFlag(this->cpu.ax.x);
					this->cpu.flags.cf = false;
					this->setParityFlag(true, this->cpu.ax.x);

					Cycles += 2;
					break;

				case 0x26 :	// ES:
					segmentOverride = ES;
//					No clock cycles used
					break;

				case 0x27 :	// DAA
					if (((this->cpu.ax.l & 0x0F) > 9) | (this->cpu.flags.af == true)) {
						this->cpu.ax.l += 6;
						this->cpu.flags.af = true;
					} else
						this->cpu.flags.af = false;
					if ((this->cpu.ax.l > 0x9F) | (this->cpu.flags.cf == true)) {
						this->cpu.ax.l += 0x60;
						this->cpu.flags.cf = true;
					} else
						this->cpu.flags.cf = false;
					this->setSignFlag(false, this->cpu.ax.l);
					this->setZeroFlag(this->cpu.ax.l);
					this->setParityFlag(false, this->cpu.ax.l);

					Cycles += 4;
					break;

				case 0x28 :	// SUB
				case 0x29 :
				case 0x2A :
				case 0x2B :
					temp = this->readMemByte(this->getAddress(CS, IP));

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					temp = (this->getRegisterValue(w, reg));
					disp = this->getOperandValue(w, mod, rm);
					result = temp - disp;

					this->setOverflowFlag(w, result, temp, disp);
					this->setSignFlag(w, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(w, result);
					this->setParityFlag(w, result);

					if (s)
						this->setRegisterValue(w, reg, result);
					else
						this->setOperandValue(w, mod, rm, result);
//					TODO: update cycles
					break;

				case 0x2C :	// SUB AL, imm
					temp = this->cpu.ax.l;
					disp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;
					result = this->cpu.ax.l - disp;

					this->setOverflowFlag(false, result, temp, disp);
					this->setSignFlag(false, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(false, result);
					this->setParityFlag(false, result);

					this->cpu.ax.l = result;
					Cycles += 4;
					break;

				case 0x2D :	// SUB [E]AX, imm
					temp = this->cpu.ax.x;
					disp = this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;
					result = temp - disp;

					this->setOverflowFlag(true, result, temp, disp);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(true, result);
					this->setParityFlag(true, result);

					this->cpu.ax.x = result;

					Cycles += 2;
					break;

				case 0x2E :	// CS:
					segmentOverride = CS;
//					No clock cycles used
					break;

				case 0x2F :	// DAS
					if (((this->cpu.ax.l & 0x0F) > 9) | (this->cpu.flags.af == true)) {
						this->cpu.ax.l -= 6;
						this->cpu.flags.af = true;
					} else
						this->cpu.flags.af = false;
					if ((this->cpu.ax.l > 0x9F) | (this->cpu.flags.cf == true)) {
						this->cpu.ax.l -= 0x60;
						this->cpu.flags.cf = true;
					} else
						this->cpu.flags.cf = false;
					this->setSignFlag(false, this->cpu.ax.x);
					this->setZeroFlag(this->cpu.ax.x);
					Cycles += 4;
					break;


				case 0x30 :	// XORB
				case 0x31 :	// XORW
				case 0x32 :
				case 0x33 :
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					temp = (this->getRegisterValue(w, reg) ^ this->getOperandValue(w, mod, rm));

					if (s)
						this->setRegisterValue(w, reg, temp);
					else
						this->setOperandValue(w, mod, rm, temp);

					this->cpu.flags.of = false;
					this->setSignFlag(w, temp);
					this->setZeroFlag(temp);
					this->cpu.flags.cf = false;
					this->setParityFlag(w, temp);
//					TODO: update cycles
					break;

				case 0x34 :	// XOR AL, imm
					this->cpu.ax.l ^= this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					this->cpu.flags.of = false;
					this->setSignFlag(false, this->cpu.ax.l);
					this->setZeroFlag(this->cpu.ax.l);
					this->cpu.flags.cf = false;
					this->setParityFlag(false, this->cpu.ax.l);

					Cycles += 2;
					break;

				case 0x35 :	// XOR [E]AX, imm
					this->cpu.ax.x ^= this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;

					this->cpu.flags.of = false;
					this->setSignFlag(true, this->cpu.ax.x);
					this->setZeroFlag(this->cpu.ax.x);
					this->cpu.flags.cf = false;
					this->setParityFlag(true, this->cpu.ax.x);

					Cycles += 2;
					break;

				case 0x36 :	// SS:
					segmentOverride = SS;
//					No clock cycles used
					break;

				case 0x37 :	// AAA
					if (((this->cpu.ax.l & 0x0F) > 9) | (this->cpu.flags.af == true)) {
						this->cpu.ax.l = (this->cpu.ax.l + 6) & 0x0F;
						this->cpu.ax.h += 1;
						this->cpu.flags.af = true;
						this->cpu.flags.cf = true;
					} else {
						this->cpu.flags.af = false;
						this->cpu.flags.cf = false;
					}
					Cycles += 4;
					break;

				case 0x38 :	// CMP
				case 0x39 :
				case 0x3A :
				case 0x3B :
					temp = this->readMemByte(this->getAddress(CS, IP));

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					temp = (this->getRegisterValue(w, reg));
					disp = this->getOperandValue(w, mod, rm);
					result = temp - disp;

					this->setOverflowFlag(w, result, temp, disp);
					this->setSignFlag(w, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(w, result);
					this->setParityFlag(w, result);

//					TODO: update cycles
					break;

				case 0x3C :	// CMP AL, imm
					temp = this->cpu.ax.l;
					disp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;
					result = this->cpu.ax.l - disp;

					this->setOverflowFlag(false, result, temp, disp);
					this->setSignFlag(false, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(false, result);
					this->setParityFlag(false, result);

					Cycles += 4;
					break;

				case 0x3D :	// CMP [E]AX, imm
					temp = this->cpu.ax.x;
					disp = this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;
					result = temp - disp;

					this->setOverflowFlag(true, result, temp, disp);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(true, result);
					this->setParityFlag(true, result);

					Cycles += 2;
					break;

				case 0x3E :	// DS:
					segmentOverride = DS;
//					No clock cycles used
					break;

				case 0x3F :	// AAS
					if (((this->cpu.ax.l & 0x0F) > 9) | (this->cpu.flags.af == true)) {
						this->cpu.ax.l -= 6;
						this->cpu.ax.l &= 0x0F;
						this->cpu.ax.h -= 1;
						this->cpu.flags.af = true;
						this->cpu.flags.cf = true;
					} else {
						this->cpu.flags.af = false;
						this->cpu.flags.cf = false;
					}
					Cycles += 4;
					break;

				case 0x40 :	// INC AX
					result = temp = this->cpu.ax.x;
					result++;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.ax.x = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x41 :	// INC CX
					result = temp = this->cpu.cx.x;
					result++;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.cx.x = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x42 :	// INC DX
					result = temp = this->cpu.dx.x;
					result++;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.dx.x = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x43 :	// INC BX
					result = temp = this->cpu.bx.x;
					result++;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.bx.x = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x44 :	// INC SP
					result = temp = this->cpu.sp;
					result++;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.sp = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x45 :	// INC BP
					result = temp = this->cpu.bp;
					result++;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.bp = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x46 :	// INC SI
					result = temp = this->cpu.si;
					result++;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.si = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x47 :	// INC DI
					result = temp = this->cpu.di;
					result++;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.di = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x48 :	// DEC AX
					result = temp = this->cpu.ax.x;
					result--;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.ax.x = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x49 :	// DEC CX
					result = temp = this->cpu.cx.x;
					result--;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.cx.x = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x4A :	// DEC DX
					result = temp = this->cpu.dx.x;
					result--;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.dx.x = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x4B :	// DEC BX
					result = temp = this->cpu.bx.x;
					result--;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.bx.x = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x4C :	// DEC SP
					result = temp = this->cpu.sp;
					result--;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.sp = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x4D :	// DEC BP
					result = temp = this->cpu.bp;
					result--;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.bp = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x4E :	// DEC SI
					result = temp = this->cpu.si;
					result--;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.si = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x4F :	// DEC DI
					result = temp = this->cpu.di;
					result--;

					this->setOverflowFlag(true, result, temp, 1);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, 1);
					this->setParityFlag(true, result);

					this->cpu.di = result;
					if (this->cpu.type == I80286) {
						Cycles += 3;
					} else {
						Cycles += 2;
					}
					break;

				case 0x50 :	// PUSH AX
					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.ax.x);
					Cycles += 2;
					break;

				case 0x51 :	// PUSH CX
					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.cx.x);
					Cycles += 2;
					break;

				case 0x52 :	// PUSH DX
					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.dx.x);
					Cycles += 2;
					break;

				case 0x53 :	// PUSH BX
					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.bx.x);
					Cycles += 2;
					break;

				case 0x54 :	// PUSH SP
					switch (this->cpu.type) {
						case I8088 :
						case I8086 :
						case I80188 :
						case I80186 :
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), this->cpu.sp);
							break;

						default :
							this->writeMemWord(this->getAddress(SS, SP-2), this->cpu.sp);
							this->cpu.sp -= 2;
							break;
					}
					Cycles += 2;
					break;

				case 0x55 :	// PUSH BP
					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.bp);
					Cycles += 2;
					break;

				case 0x56 :	// PUSH SI
					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.si);
					Cycles += 2;
					break;

				case 0x57 :	// PUSH DI
					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.di);
					Cycles += 2;
					break;

				case 0x58 :	// POP AX
					this->cpu.ax.x = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 4;
					break;

				case 0x59 :	// POP CX
					this->cpu.cx.x = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 4;
					break;

				case 0x5A :	// POP DX
					this->cpu.dx.x = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 4;
					break;

				case 0x5B :	// POP BX
					this->cpu.bx.x = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 4;
					break;

				case 0x5C :	// POP SP
					this->cpu.sp = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 4;
					break;

				case 0x5D :	// POP BP
					this->cpu.bp = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 4;
					break;

				case 0x5E :	// POP SI
					this->cpu.si = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 4;
					break;

				case 0x5F :	// POP DI
					this->cpu.di = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 4;
					break;

				case 0x60 :	// PUSHA
					switch (this->cpu.type) {
						case I80188 :
						case I80186 :
						case I80286 :
							temp = this->cpu.sp;
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), this->cpu.ax.x);
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), this->cpu.cx.x);
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), this->cpu.dx.x);
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), this->cpu.bx.x);
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), temp);
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), this->cpu.bp);
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), this->cpu.si);
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), this->cpu.di);
							Cycles += 18;
							break;

						default :
							pendingInterrupt = 0x06;
							break;
					}
					break;

				case 0x61 :	// POPA
					switch (this->cpu.type) {
						case I80188 :
						case I80186 :
						case I80286 :
							this->cpu.di = this->readMemWord(this->getAddress(SS, SP));
							this->cpu.sp += 2;
							this->cpu.si = this->readMemWord(this->getAddress(SS, SP));
							this->cpu.sp += 2;
							this->cpu.bp = this->readMemWord(this->getAddress(SS, SP));
							this->cpu.sp += 4;
							this->cpu.bx.x = this->readMemWord(this->getAddress(SS, SP));
							this->cpu.sp += 2;
							this->cpu.dx.x = this->readMemWord(this->getAddress(SS, SP));
							this->cpu.sp += 2;
							this->cpu.cx.x = this->readMemWord(this->getAddress(SS, SP));
							this->cpu.sp += 2;
							this->cpu.ax.x = this->readMemWord(this->getAddress(SS, SP));
							this->cpu.sp += 2;
							Cycles += 24;
							break;

						default :
							pendingInterrupt = 0x06;
							break;
						}
					break;

				case 0x62 :	// BOUND mod reg r/m (188+)
					switch (this->cpu.type) {
						case I80188 :
						case I80186 :
						case I80286 :
							temp = this->readMemByte(this->getAddress(CS, IP));
							this->cpu.ip++;

							mod = ((temp & 0xC0) >> 6);
							reg = ((temp & 0x38) >> 3);
							rm  = (temp & 0x07);

							temp = this->getRegisterValue(true, reg);
							disp = this->getOperandValue(true, mod, rm);

							if ((temp < this->readMemWord(this->getAddress(CS, disp))) | (temp > this->readMemWord(this->getAddress(CS, disp + 2)))) {
								this->cpu.ip -= 2;
								pendingInterrupt = 0x05;
							}
							Cycles += 10;
							break;

						default :
							pendingInterrupt = 0x06;
							break;
					}
					break;

				case 0x63 :	// ARPL mod reg r/m (286+)
					if (this->cpu.type == I80286) {
						temp = this->readMemByte(this->getAddress(CS, IP));
						this->cpu.ip++;

						mod = ((temp & 0xC0) >> 6);
						reg = ((temp & 0x38) >> 3);
						rm  = (temp & 0x07);

						temp = this->getRegisterValue(true, reg);
						disp = this->getOperandValue(true, mod, rm);

	//					TODO: implement instruction
	//					TODO: set ZF flag
						Cycles += 10; // 10 or 11
					} else
						pendingInterrupt = 0x06;
					break;

				case 0x68 :	// PUSH imm16 (188+)
				case 0x6A :	// PUSH imm8 (188+)
					switch (this->cpu.type) {
						case I80188 :
						case I80186 :
						case I80286 :
							if (s) {
								this->cpu.sp -= 1;
								this->writeMemByte(this->getAddress(SS, SP), this->readMemByte(this->getAddress(CS, IP)));
								this->cpu.ip++;
							} else {
								this->cpu.sp -= 2;
								this->writeMemWord(this->getAddress(SS, SP), this->readMemWord(this->getAddress(CS, IP)));
								this->cpu.ip += 2;
							}
							Cycles += 2;
							break;

						default :
							pendingInterrupt = 0x06;
							break;
					}
					break;

				case 0x69 :	// IMUL
				case 0x6B :
					switch (this->cpu.type) {
						case I80188 :
						case I80186 :
						case I80286 :
							temp = this->readMemByte(this->getAddress(CS, IP));
							this->cpu.ip++;

							mod = ((temp & 0xC0) >> 6);
							reg = ((temp & 0x38) >> 3);
							rm  = (temp & 0x07);

							if (s) {
								temp = this->readMemByte(this->getAddress(CS, IP));
								this->cpu.ip++;
							} else {
								temp = this->readMemWord(this->getAddress(CS, IP));
								this->cpu.ip += 2;
							}

							disp = this->getOperandValue(w, mod, rm);

		//					TODO: implement instruction
		//					TODO: set overflow & carry flags
		//					TODO: update cycles
							break;

						default :
							pendingInterrupt = 0x06;
							break;
					}
					break;

				case 0x6C :	// INSB (188+)
				case 0x6D :	// INSW[D] (188+)
					switch (this->cpu.type) {
						case I80188 :
						case I80186 :
						case I80286 :
							if (w) {
								writeMemByte(this->readIOByte(this->cpu.dx.x), this->getAddress(ES, DI));
								if (this->cpu.flags.df == false)
									this->cpu.di--;
								else
									this->cpu.di++;
							}
							writeMemByte(this->readIOByte(this->cpu.dx.x), this->getAddress(ES, DI));
							if (this->cpu.flags.df == false)
								this->cpu.di--;
							else
								this->cpu.di++;
							Cycles += 5;
							break;

						default :
							pendingInterrupt = 0x06;
							break;
					}
					break;

				case 0x6E :	// OUTSB (188+)
				case 0x6F :	// OUTSW[D] (188+)
					switch (this->cpu.type) {
						case I80188 :
						case I80186 :
						case I80286 :
							if (w) {
								this->writeIOByte(this->cpu.dx.x, this->readMemByte(this->getAddress(DS, SI)));
								if (this->cpu.flags.df == true)
									this->cpu.si--;
								else
									this->cpu.si++;
							}
							this->writeIOByte(this->cpu.dx.x, this->readMemByte(this->getAddress(DS, SI)));
							if (this->cpu.flags.df == true)
								this->cpu.si--;
							else
								this->cpu.si++;
							Cycles += 5;
							break;

						default:
							pendingInterrupt = 0x05;
						break;
					}
					break;

				case 0x70 :	// Jcc
				case 0x71 :
				case 0x72 :
				case 0x73 :
				case 0x74 :
				case 0x75 :
				case 0x76 :
				case 0x77 :
				case 0x78 :
				case 0x79 :
				case 0x7A :
				case 0x7B :
				case 0x7C :
				case 0x7D :
				case 0x7E :
				case 0x7F :
					if (evaluateConditionCode(this->cpu.instruction_reg.reg & 0x0F)) {
						disp = this->readMemByte(this->getAddress(CS, IP));
						if (disp & 0x80)
							this->cpu.ip -= (256 - disp);
						else
							this->cpu.ip += disp;
						if (this->cpu.type == I80286)
							Cycles += 7;
						else
							Cycles += 16;
					} else {
						if (this->cpu.type == I80286)
							Cycles += 3;
						else
							Cycles += 4;
					}
					break;

				case 0x80 :	// Opcode extention
				case 0x81 :
				case 0x82 :
				case 0x83 :
					// TODO: proper handling of s and w bits
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					switch (reg) {
						case 0x00 :	// ADD
							temp = this->getOperandValue(w, mod, rm);
							disp = this->readMemByte(this->getAddress(CS, IP));
							result = temp + disp;

							this->setOverflowFlag(true, result, temp, disp);
							this->setSignFlag(true, result);
							this->setZeroFlag(result);
							this->setAdjustFlag(result, temp, disp);
							this->setCarryFlag(true, result);
							this->setParityFlag(true, result);

							this->setOperandValue(w, mod, rm, temp);
							Cycles += 2;
							break;

						case 0x01 :	// OR
							temp = this->getOperandValue(w, mod, rm);
							disp = this->readMemByte(this->getAddress(CS, IP));
							result = temp | disp;

							this->cpu.flags.of = false;
							this->setSignFlag(w, result);
							this->setZeroFlag(result);
							this->cpu.flags.cf = false;
							this->setParityFlag(w, result);

							this->setOperandValue(w, mod, rm, temp);
							Cycles += 2;
							break;

						case 0x02 :	// ADC
							temp = this->getOperandValue(w, mod, rm);
							disp = this->readMemByte(this->getAddress(CS, IP));
							result = temp + disp;
							if (this->cpu.flags.cf)
								result++;

							this->setOverflowFlag(true, result, temp, disp);
							this->setSignFlag(true, result);
							this->setZeroFlag(result);
							this->setAdjustFlag(result, temp, disp);
							this->setCarryFlag(true, result);
							this->setParityFlag(true, result);

							this->setOperandValue(w, mod, rm, temp);
							Cycles += 2;
							break;

						case 0x03 :	// SBB
							temp = this->getOperandValue(w, mod, rm);
							disp = this->readMemByte(this->getAddress(CS, IP));
							result = temp - disp;
							if (this->cpu.flags.cf)
								result++;

							this->setOverflowFlag(true, result, temp, disp);
							this->setSignFlag(true, result);
							this->setZeroFlag(result);
							this->setAdjustFlag(result, temp, disp);
							this->setCarryFlag(true, result);
							this->setParityFlag(true, result);

							this->setOperandValue(w, mod, rm, temp);
							Cycles += 2;
							break;

						case 0x04 :	// AND
							temp = this->getOperandValue(w, mod, rm);
							disp = this->readMemByte(this->getAddress(CS, IP));
							result = temp & disp;

							this->cpu.flags.of = false;
							this->setSignFlag(w, result);
							this->setZeroFlag(result);
							this->cpu.flags.cf = false;
							this->setParityFlag(w, result);

							this->setOperandValue(w, mod, rm, temp);
							Cycles += 2;
							break;

						case 0x05 :	// SUB
							temp = this->getOperandValue(w, mod, rm);
							disp = this->readMemByte(this->getAddress(CS, IP));
							result = temp - disp;

							this->setOverflowFlag(true, result, temp, disp);
							this->setSignFlag(true, result);
							this->setZeroFlag(result);
							this->setAdjustFlag(result, temp, disp);
							this->setCarryFlag(true, result);
							this->setParityFlag(true, result);

							this->setOperandValue(w, mod, rm, temp);
							Cycles += 2;
							break;

						case 0x06 :	// XOR
							temp = this->getOperandValue(w, mod, rm);
							disp = this->readMemByte(this->getAddress(CS, IP));
							result = temp ^ disp;

							this->cpu.flags.of = false;
							this->setSignFlag(w, result);
							this->setZeroFlag(result);
							this->cpu.flags.cf = false;
							this->setParityFlag(w, result);

							this->setOperandValue(w, mod, rm, temp);
							Cycles += 2;
							break;

						case 0x07 :	// CMP
							temp = this->getOperandValue(w, mod, rm);
							disp = this->readMemByte(this->getAddress(CS, IP));
							result = temp - disp;

							this->setOverflowFlag(true, result, temp, disp);
							this->setSignFlag(true, result);
							this->setZeroFlag(result);
							this->setAdjustFlag(result, temp, disp);
							this->setCarryFlag(true, result);
							this->setParityFlag(true, result);

							Cycles += 2;
							break;
					}
					break;

				case 0x84 :	// TEST w mod reg r/m
				case 0x85 :
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					temp = (this->getRegisterValue(w, reg) & this->getOperandValue(w, mod, rm));

					this->cpu.flags.of = false;
					this->setSignFlag(w, temp);
					this->setZeroFlag(temp);
					this->cpu.flags.cf = false;
					this->setParityFlag(w, temp);

//					TODO: update cycles
					break;

				case 0x86 :	// XCHG w mod reg r/m
				case 0x87 :
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					temp = this->getOperandValue(w, mod, rm);
					this->setOperandValue(w, mod, rm, this->getRegisterValue(w, reg));
					this->setRegisterValue(w, reg, temp);

//					TODO: Cycles
					break;

				case 0x88 :	// MOV register/memory to/from register
				case 0x89 :
				case 0x8A :
				case 0x8B :
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					if (s) {
						this->setRegisterValue(w, reg, this->getOperandValue(w, mod, rm));
						Cycles += 2;
					} else
						this->setOperandValue(w, mod, rm, (this->getRegisterValue(w, reg)));

					Cycles += 2;
					break;

				case 0x8C :	// MOV segment, r/m16
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					switch (reg) {
						case 0x00 :
							this->cpu.es = this->getOperandValue(true, mod, rm);
							break;

						case 0x01 :
							this->cpu.cs = this->getOperandValue(true, mod, rm);
							break;

						case 0x02 :
							this->cpu.ss = this->getOperandValue(true, mod, rm);
							break;

						case 0x03 :
							this->cpu.ds = this->getOperandValue(true, mod, rm);
							break;

						default :
							pendingInterrupt = 0x06;
							break;
					}

					Cycles += 2;
					break;

				case 0x8D :	// LEA
					reg = this->getRegisterValue(w, ((temp & 0x38) >> 3));
					this->setRegisterValue(true, reg, this->getRegisterValue(w, reg));
					break;

				case 0x8E :	// MOV r/m16, segment
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					switch (reg) {
						case 0x00 :
							this->setOperandValue(true, mod, rm, this->cpu.es);
							break;

						case 0x01 :
							this->setOperandValue(true, mod, rm, this->cpu.cs);
							break;

						case 0x02 :
							this->setOperandValue(true, mod, rm, this->cpu.ss);
							break;

						case 0x03 :
							this->setOperandValue(true, mod, rm, this->cpu.ds);
							break;

						default :
							pendingInterrupt = 0x06;
							break;
					}

					Cycles += 2;
					break;

				case 0x8F :	// Opcode extention
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					switch (reg) {
						case 0x00 :	// POP r/m
							this->setOperandValue(true, mod, rm, this->readMemWord(this->getAddress(SS, SP)));
							this->cpu.sp += 2;
							Cycles += 4;
							break;

						default :	// Illegal opcode
							pendingInterrupt = 0x06;
							break;
					}
					break;

				case 0x90 :	// NOP (XCHG AX, AX)
					Cycles += 3;
					break;

				case 0x91 :	// XCHG with AX
				case 0x92 :
				case 0x93 :
				case 0x94 :
				case 0x95 :
				case 0x96 :
				case 0x97 :
					temp = this->cpu.ax.x;
					this->cpu.ax.x = this->getRegisterValue(true, this->cpu.instruction_reg.reg & 0x07);
					this->setRegisterValue(true, this->cpu.instruction_reg.reg & 0x07, temp);
					Cycles += 3;
					break;

				case 0x98 :	// CBW
					if (this->cpu.ax.l & 0x80)
						this->cpu.ax.h = 0xFF;
					else
						this->cpu.ax.h = 0x00;
					Cycles += 3;
					break;

				case 0x99 :	// CWD
					if (this->cpu.ax.x & 0x8000)
						this->cpu.dx.x = 0xFFFF;
					else
						this->cpu.dx.x = 0x0000;
					Cycles += 2;
					break;

				case 0x9A :	// CALL (Direct intersegment)
					this->cpu.sp -= 4;
					writeMemLong(this->getAddress(SS, SP), this->cpu.ip + 6);
					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.cs);
					temp = readMemLong(this->getAddress(CS, IP));
					this->cpu.ip += 4;
					this->cpu.cs = this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip = temp;

					Cycles += 17;
					break;

				case 0x9B :	// WAIT
					if (this->cpu.wait == true)
						this->cpu.ip--;
					Cycles += 6;
					break;

				case 0x9C :	// PUSHF
					if ((this->cpu.type == I80186) || (this->cpu.type == I80286)) {
						this->cpu.sp -= 2;
						this->writeMemWord(this->getAddress(SS, SP), this->cpu.flags.reg);
					} else
						pendingInterrupt = 0x06;

					Cycles += 4;
					break;

				case 0x9D :	// POPF
					if ((this->cpu.type == I80186) || (this->cpu.type == I80286)) {
						this->cpu.flags.reg = this->readMemWord(this->getAddress(SS, SP));
						this->cpu.sp += 2;
					} else
						pendingInterrupt = 0x06;

					Cycles += 5;
					break;

				case 0x9E :	// SAHF
					this->cpu.flags.reg = ((this->cpu.flags.reg & 0xFF00) | this->cpu.ax.h);
					Cycles += 3;
					break;

				case 0x9F :	// LAHF
					this->cpu.ax.h = (this->cpu.flags.reg & 0x00FF);
					Cycles += 2;
					break;

				case 0xA0 :	// MOV
				case 0xA1 :
				case 0xA2 :
				case 0xA3 :
					disp = this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;
					disp += (this->cpu.ds << 4);
					// TODO: 286 addressing
					// TODO: segment override

					if (s) {
						if (w) {
							this->writeMemWord(disp, this->cpu.ax.x);
						} else {
							this->writeMemByte(disp, this->cpu.ax.l);
						}
					} else {
						if (w) {
							this->cpu.ax.x = this->readMemWord(disp);
						} else {
							this->cpu.ax.l = this->readMemByte(disp);
						}
						Cycles += 2;
					}
					Cycles += 2;
					break;

				case 0xA4 :	// MOVSB
					this->writeMemByte(this->getAddress(ES, DI), this->readMemByte(this->getAddress(DS, SI)));
					if (this->cpu.flags.df) {
						this->cpu.di--;
						this->cpu.si--;
					} else {
						this->cpu.di++;
						this->cpu.si++;
					}
					Cycles += 7;
					break;

				case 0xA5 :	// MOVSW
					this->writeMemWord(this->getAddress(ES, DI), this->readMemWord(this->getAddress(DS, SI)));
					if (this->cpu.flags.df) {
						this->cpu.di -= 2;
						this->cpu.si -= 2;
					} else {
						this->cpu.di += 2;
						this->cpu.si += 2;
					}
					Cycles += 7;
					break;

				case 0xA6 :	// CMPSB
					temp = this->readMemByte(this->getAddress(ES, DI));
					disp = this->readMemByte(this->getAddress(ES, SI));
					if (this->cpu.flags.df == false) {
						this->cpu.di++;
						this->cpu.si++;
					} else {
						this->cpu.di--;
						this->cpu.si--;
					}

					result = temp - disp;

					this->setOverflowFlag(false, result, temp, disp);
					this->setSignFlag(false, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(false, result);
					this->setParityFlag(false, result);

					Cycles += 10;
					break;

				case 0xA7 :	// CMPSW
					temp = this->readMemWord(this->getAddress(ES, DI));
					disp = this->readMemWord(this->getAddress(ES, SI));
					if (this->cpu.flags.df == false) {
						this->cpu.di += 2;
						this->cpu.si += 2;
					} else {
						this->cpu.di -= 2;
						this->cpu.si -= 2;
					}
					result = temp - disp;

					this->setOverflowFlag(true, result, temp, disp);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(true, result);
					this->setParityFlag(true, result);

					Cycles += 10;
					break;

				case 0xA8 :	// TEST AL
					temp = this->cpu.ax.l & this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					this->cpu.flags.of = false;
					this->setSignFlag(false, temp);
					this->setZeroFlag(temp);
					this->cpu.flags.cf = false;
					this->setParityFlag(false, temp);

					Cycles += 2;
					break;

				case 0xA9 :	// TEST [E]AX
					temp = this->cpu.ax.x & this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;
					this->cpu.flags.of = false;
					this->setSignFlag(true, temp);
					this->setZeroFlag(temp);
					this->cpu.flags.cf = false;
					this->setParityFlag(true, temp);

					Cycles += 2;
					break;

				case 0xAA :	// STOSB
					this->writeMemByte(this->getAddress(ES, DI), this->cpu.ax.l);
					if (this->cpu.flags.df == false)
						this->cpu.di++;
					else
						this->cpu.di--;
					Cycles += 4;
					break;

				case 0xAB :	// STOSW
					this->writeMemWord(this->getAddress(ES, DI), this->cpu.ax.x);
					if (this->cpu.flags.df == false)
						this->cpu.di += 2;
					else
						this->cpu.di -= 2;
					Cycles += 4;
					break;

				case 0xAC :	// LODSB
					this->cpu.ax.l = this->readMemByte(this->getAddress(ES, DI));
					if (this->cpu.flags.df == false)
						this->cpu.di++;
					else
						this->cpu.di--;
					Cycles += 5;
					break;

				case 0xAD :	// LODSW
					this->cpu.ax.x = readMemWord(this->getAddress(ES, DI));
					if (this->cpu.flags.df == false)
						this->cpu.di += 2;
					else
						this->cpu.di -= 2;
					Cycles += 5;
					break;

				case 0xAE :	// SCASB
					temp = this->readMemByte(this->getAddress(ES, DI));
					if (this->cpu.flags.df == false)
						this->cpu.di++;
					else
						this->cpu.di--;
					result = this->cpu.ax.l - temp;

					this->setOverflowFlag(true, result, temp, disp);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(true, result);
					this->setParityFlag(true, result);

					Cycles += 7;
					break;

				case 0xAF :	// SCASW
					temp = this->readMemWord(this->getAddress(ES, DI));
					if (this->cpu.flags.df == false)
						this->cpu.di += 2;
					else
						this->cpu.di -= 2;
					result = this->cpu.ax.x - temp;

					this->setOverflowFlag(true, result, temp, disp);
					this->setSignFlag(true, result);
					this->setZeroFlag(result);
					this->setAdjustFlag(result, temp, disp);
					this->setCarryFlag(true, result);
					this->setParityFlag(true, result);

					Cycles += 7;
					break;

				case 0xB0 :	// MOV AL
				case 0xB1 :	// MOV CL
				case 0xB2 :	// MOV DL
				case 0xB3 :	// MOV BL
				case 0xB4 :	// MOV AH
				case 0xB5 :	// MOV CH
				case 0xB6 :	// MOV DH
				case 0xB7 :	// MOV BH
				case 0xB8 :	// MOV AX
				case 0xB9 :	// MOV CX
				case 0xBA :	// MOV DX
				case 0xBB :	// MOV BX
				case 0xBC :	// MOV SP
				case 0xBD :	// MOV BP
				case 0xBE :	// MOV SI
				case 0xBF :	// MOV DI
					w = ((this->cpu.instruction_reg.reg & 0x08) == 0x08);
					reg = (this->cpu.instruction_reg.reg & 0x07);
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					this->setRegisterValue(w, reg, temp);
					break;

				case 0xC2 :	// RET NEAR
					temp = this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					this->cpu.sp += temp;
					Cycles += 10; // 10+m
					break;

				case 0xC3 :	// RET NEAR
					this->cpu.ip = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 10; // 10+m
					break;

				case 0xC4 :	// LES
					reg = this->getRegisterValue(w, ((temp & 0x38) >> 3));

					this->setRegisterValue(true, reg, this->readMemWord(this->getRegisterValue(w, reg)));
					this->cpu.es = this->readMemWord(this->getRegisterValue(w, reg));
					break;

				case 0xC5 :	// LDS
					reg = this->getRegisterValue(w, ((temp & 0x38) >> 3));

					this->setRegisterValue(true, reg, this->readMemWord(this->getRegisterValue(w, reg)));
					this->cpu.ds = this->readMemWord(this->getRegisterValue(w, reg));
					break;

				case 0xC6 :	// Opcode extention
				case 0xC7 :	// Opcode extention
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					switch (reg) {
						case 0x00 :	// MOV imm8/16 to r/m
							if (w) {
								temp = this->readMemWord(this->getAddress(CS, IP));
								this->cpu.ip += 2;
							} else {
								temp = this->readMemByte(this->getAddress(CS, IP));
								this->cpu.ip++;
							}
							this->setOperandValue(w, mod, rm, temp);
							break;

						default :
							pendingInterrupt = 0x06;
							break;
					}
					break;

				case 0xC8 :	// ENTER (188+)
					switch (this->cpu.type) {
						case I80188 :
						case I80186 :
						case I80286 :
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), this->cpu.bp);

							disp = this->readMemWord(this->getAddress(CS, IP));
							this->cpu.ip += 2;
							level = this->readMemByte(this->getAddress(CS, IP));
							this->cpu.ip++;

							temp = this->cpu.sp;

							while (level != 0) {
								this->cpu.bp -= 2;
								this->cpu.sp -= 2;
								this->writeMemWord(this->getAddress(SS, SP), this->cpu.bp);
								level--;
							}

							this->cpu.bp = temp;
							this->cpu.sp = temp - disp;
								
							Cycles += 5;
							break;

						default:
							pendingInterrupt = 0x05;
							break;
					}
					break;

				case 0xC9 :	// LEAVE (188+)
					switch (this->cpu.type) {
						case I80188 :
						case I80186 :
						case I80286 :
							this->cpu.sp = this->cpu.bp;
							this->cpu.bp = this->readMemWord(this->getAddress(SS, SP));
							this->cpu.sp += 2;
							Cycles += 8;
							break;

						default:
							pendingInterrupt = 0x05;
							break;
					}
					break;

				case 0xCA :	// RETF imm16
					temp = this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;
					this->cpu.ip = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					this->cpu.cs = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					this->cpu.sp += temp;
					Cycles += 7;
					break;

				case 0xCB :	// RETF
					this->cpu.ip = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					this->cpu.cs = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					Cycles += 7;
					break;

				case 0xCC :	// INT3
					this->pendingInterrupt = 0x03;
					Cycles += 33;
					break;

				case 0xCD :	// INT
					this->pendingInterrupt = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;
					Cycles += 37;
					break;

				case 0xCE :	// INTO
					if (this->cpu.flags.of == true) {
						pendingInterrupt = 0x04;
						Cycles += 35;
					} else
						Cycles += 3;
					break;

				case 0xCF :	// IRET
					this->cpu.ip = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					this->cpu.cs = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;
					this->cpu.flags.reg = this->readMemWord(this->getAddress(SS, SP));
					this->cpu.sp += 2;

					pendingInterrupt = -1;
					switch (cpu.type) {
						case I8086 :
							Cycles += 22;
							break;

						case I80188 :
						case I80186 :
							Cycles += 28;
							break;
						case I80286 :
							if (this->cpu.msw.pe)
								Cycles += 31;
							else
								Cycles += 17;
							break;
					}
					break;

				case 0xC0 :	// Opcode extention
				case 0xC1 :	// Opcode extention
				case 0xD0 :	// Opcode extention
				case 0xD1 :	// Opcode extention
				case 0xD2 :	// Opcode extention
				case 0xD3 :	// Opcode extention
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					switch (this->cpu.instruction_reg.reg) {
						case 0xC0 :
						case 0xC1 :
							if ((this->cpu.type == I80188) | (this->cpu.type == I80186) | (this->cpu.type == I80286)) {
								disp = this->readMemByte(this->getAddress(CS, IP));
								this->cpu.ip++;
							} else
								pendingInterrupt = 0x06;
							break;

						case 0xD0 :
						case 0xD1 :
							disp = 1;
							break;

						case 0xD2 :
						case 0xD3 :
							disp = this->cpu.cx.l;
							break;
					}

					temp = this->getOperandValue(w, mod, rm);
					while (disp != 0) {
						switch (reg) {
							case 0x00 :	// ROL
								if (w) {
									if (temp & 0x8000) {
										temp <<= 1;
										temp |= 0x0001;
										this->cpu.flags.cf = true;
									} else {
										temp <<= 1;
										this->cpu.flags.cf = false;
									}
								} else {
									if (temp & 0x80) {
										temp <<= 1;
										temp |= 0x0001;
										this->cpu.flags.cf = true;
									} else {
										temp <<= 1;
										this->cpu.flags.cf = false;
									}
								}
			//					TODO set overflow flag
								break;

							case 0x01 :	// ROR
								if (temp & 0x0001) {
									if (w) {
										temp >>= 1;
										temp |= 0x8000;
										this->cpu.flags.cf = true;
									} else {
										temp >>= 1;
										temp |= 0x80;
										this->cpu.flags.cf = true;
									}
								} else {
									temp >>= 1;
									this->cpu.flags.cf = false;
								}
			//					TODO set overflow flag
								break;

							case 0x02 :	// RCL
								temp <<= 1;
			//					TODO set overflow flag
								break;

							case 0x03 :	// RCR
								temp >>= 1;
			//					TODO set overflow flag
								break;

							case 0x04 :	// SHL (=SAL)
							case 0x06 :	// SAL (=SHL)
								if (w) {
									if (temp & 0x8000)
										this->cpu.flags.cf = true;
									else
										this->cpu.flags.cf = false;
									temp = ((temp << 1) & 0xFFFF);
								} else {
									if (temp & 0x80)
										this->cpu.flags.cf = true;
									else
										this->cpu.flags.cf = false;
									temp = ((temp << 1) & 0xFF);
								}
			//					TODO set overflow flag
								break;

							case 0x05 :	// SHR
								if (w) {
									if (temp & 0x8000)
										this->cpu.flags.of = true;
									else
										this->cpu.flags.of = false;
								} else {
									if (temp & 0x80)
										this->cpu.flags.of = true;
									else
										this->cpu.flags.of = false;
								}
								temp >>= 1;
								break;

							case 0x07 :	// SAR
								temp >>= 1;
								if (w) {
									if (temp & 0x4000)
										temp |= 0x8000;
								} else {
									if (temp & 0x40)
										temp |= 0x80;
								}
								this->cpu.flags.of = false;
								break;
						}
						disp--;
					}
					this->setOperandValue(w, mod, rm, temp);
					break;

				case 0xD4 :	// AAM
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					// NEC V20 and V30 (and possibly other NEC V-series CPUs) always use base 10, and
					// ignore the argument, causing a number of incompatibilities
					if ((this->cpu.type == NECV20) | (this->cpu.type == NECV30))
						temp = 10;

					this->cpu.ax.h = this->cpu.ax.l / temp;
					this->cpu.ax.l = this->cpu.ax.l % temp;
					Cycles += 17;
					break;

				case 0xD5 :	// AAD
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					this->cpu.ax.l = (this->cpu.ax.h * temp) + this->cpu.ax.l;
					this->cpu.ax.h = 0;
					Cycles += 19;
					break;

				case 0xD6 :	// SALC (Undocumented)
					if (this->cpu.flags.cf)
						this->cpu.ax.l = 0xFF;
					else
						this->cpu.ax.l = 0x00;
					// TODO: Cycles
					break;

				case 0xD7 :	// XLAT
					this->cpu.ax.l = (this->cpu.bx.x + this->cpu.ax.l);
					Cycles += 5;
					break;

				case 0xD8 :	// Floating point opcodes 1001 1ttt mod lll r/m
				case 0xD9 :
				case 0xDA :
				case 0xDB :
				case 0xDC :
				case 0xDD :
				case 0xDE :
				case 0xDF :
					if (this->fpu.type == NOFPU)
						pendingInterrupt = 0x07;
					else {
						temp = this->readMemByte(this->getAddress(CS, IP));
						this->cpu.ip++;

						mod = ((temp & 0xC0) >> 6);
						reg = ((temp & 0x38) >> 3);
						rm  = (temp & 0x07);

	//					TODO: implement instruction
	//					TODO: set flags
	//					TODO: update cycles
					}
					break;

				case 0xE0 :	// LOOPNZ / LOOPNE
					if ((--this->cpu.cx.x != 0) && (this->cpu.flags.zf == false)) {
						disp = this->readMemByte(this->getAddress(CS, IP));
						if (disp & 0x80)
							this->cpu.ip -= (256 - disp);
						else
							this->cpu.ip += disp;
					}
					Cycles += 11;
					break;

				case 0xE1 :	// LOOPZ / LOOPE
					if ((--this->cpu.cx.x != 0) && (this->cpu.flags.zf == true)) {
						disp = this->readMemByte(this->getAddress(CS, IP));
						if (disp & 0x80)
							this->cpu.ip -= (256 - disp);
						else
							this->cpu.ip += disp;
					}
					Cycles += 11;
					break;

				case 0xE2 :	// LOOP (CX times)
					if (--this->cpu.cx.x != 0) {
						disp = this->readMemByte(this->getAddress(CS, IP));
						if (disp & 0x80)
							this->cpu.ip -= (256 - disp);
						else
							this->cpu.ip += disp;
					}
					Cycles += 11;
					break;

				case 0xE3 :	// JCXZ
					if (this->cpu.cx.x == 0) {
						disp = this->readMemByte(this->getAddress(CS, IP));
						if (disp & 0x80)
							this->cpu.ip -= (256 - disp);
						else
							this->cpu.ip += disp;
					}
					Cycles += 9;
					break;

				case 0xE4 :	// IN AL, imm8
					this->cpu.ax.l = this->readIOByte(readMemByte(this->getAddress(CS, IP)));
					this->cpu.ip++;
					Cycles += 12;
					break;

				case 0xE5 :	// IN [E]AX, imm8
					this->cpu.ax.x = this->readIOWord(readMemWord(this->getAddress(CS, IP)));
					this->cpu.ip += 2;
					Cycles += 12;
					break;

				case 0xE6 :	// OUT imm8, AL
					this->writeIOByte(this->readMemByte(this->getAddress(DS, SI)), this->cpu.ax.l);
					this->cpu.ip++;
					Cycles += 10;
					break;

				case 0xE7 :	// OUT imm8, [E]AX
					this->writeIOWord(this->readMemByte(this->getAddress(DS, SI)), this->cpu.ax.x);
					this->cpu.ip++;
					Cycles += 10;
					break;

				case 0xE8 :	// CALL (Direct within segment)
					temp = this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;

					this->cpu.sp -= 2;
					this->writeMemWord(this->getAddress(SS, SP), this->cpu.ip);

					if (temp & 0x8000)
						this->cpu.ip -= (temp & 0x7FFF);
					else
						this->cpu.ip += temp;
					break;

				case 0xE9 :	// JMP (Direct within segment)
					temp = this->readMemWord(this->getAddress(CS, IP));

					if (temp & 0x8000)
						this->cpu.ip -= (0x10002-temp);
					else
						this->cpu.ip += temp;
					break;

				case 0xEA :	// JMP (Direct intersegment)
					this->cpu.cs -= this->readMemWord(this->getAddress(CS, IP));
					this->cpu.ip += 2;
					this->cpu.ip = this->readMemWord(this->getAddress(CS, IP));
					break;

					
				case 0xEB :	// JMP (Direct within segment-short)
					temp = this->readMemByte(this->getAddress(CS, IP));

					if (temp & 0x80)
						this->cpu.ip -= (0x101-temp);
					else
						this->cpu.ip += temp;
					break;

				case 0xEC :	// IN AL, DX
					this->cpu.ax.l = this->readIOByte(this->cpu.dx.x);
					Cycles += 12;
					break;

				case 0xED :	// IN [E]AX, DX
					this->cpu.ax.x = this->readIOWord(this->cpu.dx.x);
					Cycles += 12;
					break;

				case 0xEE :	// OUT DX, AL
					this->writeIOByte(this->cpu.dx.x, this->cpu.ax.l);
					this->cpu.ip++;
					Cycles += 10;
					break;

				case 0xEF :	// OUT DX, [E]AX
					this->writeIOWord(this->cpu.dx.x, this->cpu.ax.x);
					this->cpu.ip++;
					Cycles += 10;
					break;

				case 0xF0 :	// LOCK
					this->lock = false;
					break;

				case 0xF2 :	// REPNZ
				case 0xF3 :	// REPZ
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					switch (temp) {
						case 0x6C :	// INSB r/m8
							break;

						case 0x6D :	// INSW r/m16
							break;

						case 0x6E :	// OUTSB DX, r/m8
							break;

						case 0x6F :	// OUTSW DX, r/m16/32
							break;

						case 0xA4 :	// MOVSB m8, m8
							break;

						case 0xA5 :	// MOVSW m16, m16/m32, m32
							break;

						case 0xA6 :	// CMPSB
							break;

						case 0xA7 :	// CMPSW
							break;

						case 0xAA :	// STOSB
							break;

						case 0xAB :	// STOSW
							break;

						case 0xAE :	// SCASB
							break;

						case 0xAF :	// SCASW
							break;

					}
					break;

				case 0xF4 :	// HLT
					this->cpu.halt = true;
					Cycles += 5;
					break;

				case 0xF5 :	// CMC
					this->cpu.flags.cf = !this->cpu.flags.cf;
					Cycles += 2;
					break;

				case 0xF6 :	// Opcode extention
				case 0xF7 :	// Opcode extention
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					switch (reg) {
						case 0x00 :	// TEST imm r/m
						case 0x01 :	// 0x01 Is undocumented
							if (w) {
								temp = this->cpu.ax.x & this->readMemWord(this->getAddress(CS, IP));
								this->cpu.ip += 2;
							} else {
								temp = this->cpu.ax.l & this->readMemByte(this->getAddress(CS, IP));
								this->cpu.ip++;
							}

							this->cpu.flags.of = false;
							this->setSignFlag(w, temp);
							this->setZeroFlag(temp);
							this->cpu.flags.cf = false;
							this->setParityFlag(w, temp);

							Cycles += 2;
							break;

						case 0x02 :	// NOT
							disp = this->getOperandValue(w, mod, rm);
							this->setOperandValue(w, mod, rm, (~disp));
							Cycles += 16;
							break;

						case 0x03 :	// NEG
							temp = this->getOperandValue(w, mod, rm);
							if (temp == 0)
								this->cpu.flags.cf = false;
							else {
								if (w)
									result = 0x10000 - temp;
								else
									result = 0x100 - temp;

								this->setOverflowFlag(w, result, 0, result);
								this->setSignFlag(w, result);
								this->setZeroFlag(result);
								this->cpu.flags.cf = true;
								this->setParityFlag(w, result);

								this->setOperandValue(w, mod, rm, result);
							}

							Cycles += 16;
							break;

						case 0x04 :	// MUL AX, r/m8 or MUL DX:AX, r/m16
							disp = this->getOperandValue(w, mod, rm);
							if (w) {
								disp *= this->cpu.ax.x;
								this->setRegisterValue(true, DX, ((disp & 0xFFFF0000) >> 16));
								this->setRegisterValue(true, AX, (disp & 0xFFFF));
								if (this->cpu.dx.x = 0) {
									this->cpu.flags.cf = false;
									this->cpu.flags.of = false;
								} else {
									this->cpu.flags.cf = false;
									this->cpu.flags.of = false;
								}
							} else {
								this->setRegisterValue(true, AX, (this->cpu.ax.l * disp));
								if (this->cpu.ax.h = 0) {
									this->cpu.flags.cf = false;
									this->cpu.flags.of = false;
								} else {
									this->cpu.flags.cf = false;
									this->cpu.flags.of = false;
								}
							}
							break;

						case 0x05 :	// IMUL AX, r/m8 or IMUL DX:AX, r/m16
							disp = this->getOperandValue(w, mod, rm);
							if (w) {
								disp *= this->cpu.ax.x;
								this->setRegisterValue(true, DX, ((disp & 0xFFFF0000) >> 16));
								this->setRegisterValue(true, AX, (disp & 0xFFFF));
								if (this->cpu.dx.x = 0) {
									this->cpu.flags.cf = false;
									this->cpu.flags.of = false;
								}
							} else {
								this->setRegisterValue(true, AX, (this->cpu.ax.l * disp));
								if (this->cpu.ax.h = 0) {
									this->cpu.flags.cf = false;
									this->cpu.flags.of = false;
								}
							}
							break;

						case 0x06 :	// DIV
							disp = this->getOperandValue(w, mod, rm);
							if (disp) {
								if (w) {
									remainder = (this->getRegisterValue(w, AX) % disp);
									quotient = (this->getRegisterValue(w, AX) / disp);
									if (temp < 0x100) {
										this->cpu.ax.l = quotient;
										this->cpu.ax.h = remainder;
									} else
										pendingInterrupt = 0x00;
								} else {
									temp = ((this->getRegisterValue(w, DX) << 16) | (this->getRegisterValue(w, AX)));
									remainder = (temp % disp);
									temp = (temp / disp);
									if (temp < 0x10000) {
										this->cpu.ax.x = quotient;
										this->cpu.dx.x = remainder;
									} else
										pendingInterrupt = 0x00;
								}
							} else
								pendingInterrupt = 0x00;
							break;

						case 0x07 :	// IDIV
							disp = this->getOperandValue(w, mod, rm);
							if (disp) {
								if (w) {
									remainder = (this->getRegisterValue(w, AX) % disp);
									quotient = (this->getRegisterValue(w, AX) / disp);
									if (temp < 0x100) {
										this->cpu.ax.l = quotient;
										this->cpu.ax.h = remainder;
									} else
										pendingInterrupt = 0x00;
								} else {
									temp = ((this->getRegisterValue(w, DX) << 16) | (this->getRegisterValue(w, AX)));
									remainder = (temp % disp);
									temp = (temp / disp);
									if (temp < 0x10000) {
										this->cpu.ax.x = quotient;
										this->cpu.dx.x = remainder;
									} else
										pendingInterrupt = 0x00;
								}
							} else
								pendingInterrupt = 0x00;
							break;

						default :	// Illegal opcode
							pendingInterrupt = 0x06;
							break;
					}
					break;

				case 0xF8 :	// CLC
					this->cpu.flags.cf = false;
					Cycles += 2;
					break;

				case 0xF9 :	// STC
					this->cpu.flags.cf = true;
					Cycles += 2;
					break;

				case 0xFA :	// CLI
					this->cpu.flags.in = false;
					Cycles += 3;
					break;

				case 0xFB :	// STI
					this->cpu.flags.in = true;
					Cycles += 3;
					break;

				case 0xFC :	// CLD
					this->cpu.flags.df = false;
					Cycles += 2;
					break;

				case 0xFD :	// STD
					this->cpu.flags.df = true;
					Cycles += 2;
					break;

				case 0xFE :	// Opcode extention
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					switch (reg) {
						case 0x00 :	// INC r/m8
							result = temp = this->getOperandValue(false, mod, rm);
							result++;

							this->setOverflowFlag(false, result, temp, 1);
							this->setSignFlag(false, result);
							this->setZeroFlag(result);
							this->setAdjustFlag(result, temp, 1);
							this->setParityFlag(false, result);

							this->setOperandValue(false, mod, rm, result);
							Cycles += 15;
							break;

						case 0x01 :	// DEC r/m8
							result = temp = this->getOperandValue(false, mod, rm);
							result--;

							this->setOverflowFlag(false, result, temp, 1);
							this->setSignFlag(false, result);
							this->setZeroFlag(result);
							this->setAdjustFlag(result, temp, 1);
							this->setParityFlag(false, result);

							this->setOperandValue(false, mod, rm, result);
							Cycles += 15;
							break;

						default :	// Illegal opcode
							pendingInterrupt = 0x06;
							break;
					}
					break;

				case 0xFF :	// Opcode extention
					temp = this->readMemByte(this->getAddress(CS, IP));
					this->cpu.ip++;

					mod = ((temp & 0xC0) >> 6);
					reg = ((temp & 0x38) >> 3);
					rm  = (temp & 0x07);

					switch (reg) {
						case 0x00 :	// INC r/m16
							result = temp = this->getOperandValue(true, mod, rm);
							result++;

							this->setOverflowFlag(true, result, temp, 1);
							this->setSignFlag(true, result);
							this->setZeroFlag(result);
							this->setAdjustFlag(result, temp, 1);
							this->setParityFlag(true, result);

							this->setOperandValue(true, mod, rm, result);
							Cycles += 15;
							break;

						case 0x01 :	// DEC r/m16
							result = temp = this->getOperandValue(true, mod, rm);
							result--;

							this->setOverflowFlag(true, result, temp, 1);
							this->setSignFlag(true, result);
							this->setZeroFlag(result);
							this->setAdjustFlag(result, temp, 1);
							this->setParityFlag(true, result);

							this->setOperandValue(true, mod, rm, result);
							Cycles += 15;
							break;

						case 0x02 :	// CALL (Indirect within Segment)
							disp = this->getOperandValue(true, mod, rm);
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), this->cpu.ip);
							this->cpu.ip = disp;
							break;

						case 0x03 :	// CALL (Indirect Intersegment)
							disp = this->getOperandValue(true, mod, rm);

							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), this->cpu.cs);
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), this->cpu.ip);

							this->cpu.cs = this->readMemWord(this->getAddress(CS, disp));
							this->cpu.ip = this->readMemWord(this->getAddress(CS, disp + 2));
							break;

						case 0x04 :	// JMP (Indirect within segment)
							disp = this->getOperandValue(true, mod, rm);
							this->cpu.ip = disp;
							break;

						case 0x05 :	// JMP (Indirect Intersegment)
							disp = this->getOperandValue(true, mod, rm);
							this->cpu.cs = this->readMemWord(this->getAddress(CS, disp));
							this->cpu.ip = this->readMemWord(this->getAddress(CS, disp + 2));
							break;

						case 0x06 :	// PUSH r/m
							disp = this->getOperandValue(true, mod, rm);
							this->cpu.sp -= 2;
							this->writeMemWord(this->getAddress(SS, SP), disp);
							Cycles += 5;
							break;

						default :	// Illegal opcode
							pendingInterrupt = 0x06;
							break;
					}
					break;

				default : // Illegal opcode
					pendingInterrupt = 0x06;
					break;
			}

			if (this->cpu.flags.tf == true) {
				pendingInterruptBeforeTrace = pendingInterrupt;
				pendingInterrupt = 0x01;
			}
		}
	}
}

unsigned int ix86::getAddress(unsigned char Segment_reg, unsigned char Offset_reg) {
	unsigned short Segment, Offset;

	switch (Segment_reg) {
		case CS :
			Segment = this->cpu.cs;
			break;
		case DS :
			Segment = this->cpu.ds;
			break;
		case ES :
			Segment = this->cpu.es;
			break;
		case SS :
			Segment = this->cpu.ss;
			break;
		default :
			WriteLog("ix86::getAddress - Error: Function called with wrong Segment_reg identifier (0x%02X)\n", Segment_reg);
			break;
	}
	switch (Offset_reg) {
		case IP :
			Offset = this->cpu.ip;
			break;
		case SI :
			Offset = this->cpu.si;
			break;
		case DI :
			Offset = this->cpu.di;
			break;
		case BP :
			Offset = this->cpu.bp;
			break;
		case SP :
			Offset = this->cpu.sp;
			break;
		default :
			WriteLog("ix86::getAddress - Error: Function called with wrong Offset_reg identifier (0x%02X)\n", Offset_reg);
			break;
	}
	return ((Segment << 4) + Offset);
}

unsigned char ix86::readMemByte(unsigned int address) {
	if (this->BOOTFLAG == true) {
		if ((address & EXTERNAL_ADDRESS_MASK) >= (this->ROM_ADDR & EXTERNAL_ADDRESS_MASK))
			this->BOOTFLAG = false;					// Enable RAM
		return (this->romMemory[address & (this->ROM_SIZE)-1]);
	} else {
		if ((address & EXTERNAL_ADDRESS_MASK) >= (this->ROM_ADDR & EXTERNAL_ADDRESS_MASK))
			return (this->romMemory[address & (this->ROM_SIZE-1)]);
		else
			return (this->ramMemory[address & (this->RAM_SIZE-1)]);
	}
}

unsigned short ix86::readMemWord(unsigned int address) {
	return ((this->readMemByte(address)) | (this->readMemByte(address+1) << 8));
}

unsigned int ix86::readMemLong(unsigned int address) {
	return (this->readMemByte(address) | (this->readMemByte(address+1) << 8) | (this->readMemByte(address+2) << 16) | (readMemByte(address+3) << 24));
}

void ix86::writeMemByte(unsigned int address, unsigned char value) {
	if (this->BOOTFLAG == true) {
		if ((address & EXTERNAL_ADDRESS_MASK) >= (this->ROM_ADDR & EXTERNAL_ADDRESS_MASK))
			this->BOOTFLAG = false;					// Enable RAM
		return;									// Cannot write to ROM
	} else {
		if ((address & EXTERNAL_ADDRESS_MASK) <= (this->TUBE_ULA_ADDR & EXTERNAL_ADDRESS_MASK)) {
			ramMemory[address & (this->RAM_SIZE-1)] = value;
			return;
		}
		if ((address & EXTERNAL_ADDRESS_MASK) >= (this->ROM_ADDR & EXTERNAL_ADDRESS_MASK))
			return;
		WriteTubeFromParasiteSide((address & EXTERNAL_ADDRESS_MASK), value);
		return;
	}
}

void ix86::writeMemWord(unsigned int address, unsigned short value) {
	this->writeMemByte(address, ((value & 0xFF00)>>8));
	this->writeMemByte(address+1, (value & 0x00FF));
}

void ix86::writeMemLong(unsigned int address, unsigned int value) {
	this->writeMemByte(address, ((value & 0x000000FF)));
	this->writeMemByte(address+1, ((value & 0x0000FF00)>>8));
	this->writeMemByte(address+2, ((value & 0x00FF0000)>>16));
	this->writeMemByte(address+3, (value & 0xFF000000)>>24);
}

unsigned char ix86::readIOByte(unsigned int address) {
	switch (address) {				// Generic I/O addresses
		case 0x0080 :				// TUBE ULA register 1 data
		case 0x0082 :				// TUBE ULA register 1 status
		case 0x0084 :				// TUBE ULA register 2 data
		case 0x0086 :				// TUBE ULA register 2 status
		case 0x0088 :				// TUBE ULA register 3 data
		case 0x008A :				// TUBE ULA register 3 status
		case 0x008C :				// TUBE ULA register 4 data
		case 0x008E :				// TUBE ULA register 4 status
			return (ReadTubeFromParasiteSide((address & 0x0000000F) >> 1));
			break;
	}

	switch (this->cpu.type) {
		case I80186 :				// 80186 specific I/O addresses
			switch (address) {
				case 0xFF22 :                      // Interrupt controller: end of interrupt register
					return (0);
					break;

				case 0xFF24 :                      // Poll register
					return (0);
					break;
	
				case 0xFF26 :                      // Poll Status Register
					return (0);
					break;

				case 0xFF28 :                      // Interrupt mask register
					return (0);
					break;

				case 0xFF2A :                      // Priority mask register
					return (0);
					break;

				case 0xFF2C :                      // In-service register
					return (0);
					break;

				case 0xFF2E :                      // Interrupt request register
					return (0);
					break;

				case 0xFF30 :                      // Interrupt status register
					return (0);
					break;

				case 0xFF32 :                      // Timer control register
					return (0);
					break;

				case 0xFF34 :                      // DMA0 control register
					return (0);
					break;

				case 0xFF36 :                      // DMA1 control register
					return (0);
					break;

				case 0xFF38 :                      // INT0 control register
					return (0);
					break;

				case 0xFF3A :                      // INT1 control register
					return (0);
					break;

				case 0xFF3C :                      // INT2 control register
					return (0);
					break;

				case 0xFF3E :                      // INT3 control register
					return (0);
					break;

				case 0xFF50 :                      // Timer 0: count register
					return (0);
					break;

				case 0xFF52 :                      // Timer 0: max count a register
					return (0);
					break;

				case 0xFF54 :                      // Timer 0: max count b register
					return (0);
					break;

				case 0xFF56 :                      // Timer 0: mode/control word
					return (0);
					break;

				case 0xFF58 :                      // Timer 1: count register
					return (0);
					break;

				case 0xFF5A :                      // Timer 1: max count a register
					return (0);
					break;

				case 0xFF5C :                      // Timer 1: max count b register
					return (0);
					break;

				case 0xFF5E :                      // Timer 1: mode/control word
					return (0);
					break;

				case 0xFF60 :                      // Timer 2: count register
					return (0);
					break;

				case 0xFF62 :                      // Timer 2: max count a register
					return (0);
					break;

				case 0xFF66 :                      // Timer 2: mode/control word
					return (0);
					break;

				case 0xFFA0 :                      // Upper memory chip select (UCS)
					return (this->cpu.umcs & 0x00FF);
					break;

				case 0xFFA1 :                      // Upper memory chip select (UCS)
					return ((this->cpu.umcs & 0xFF00) >> 8);
					break;

				case 0xFFA2 :                      // Lower memory chip select (LCS)
					return (this->cpu.lmcs & 0x00FF);
					break;

				case 0xFFA3 :                      // Lower memory chip select (LCS)
					return ((this->cpu.lmcs & 0xFF00) >> 8);
					break;

				case 0xFFA4 :                      // Peripheral chip select (PCS)
					return (this->cpu.pacs & 0x00FF);
					break;

				case 0xFFA5 :                      // Peripheral chip select (PCS)
					return ((this->cpu.pacs & 0xFF00) >> 8);
					break;

				case 0xFFA6 :                      // Mid range memory chip select (MCS0-3) location
					return (0);
					break;

				case 0xFFA8 :                      // Mid range memory chip select (MCS0-3) size
					return (0);
					break;

				case 0xFFC0 :                      // DMA channel 0: source pointer
					return (0);
					break;

				case 0xFFC2 :                      // DMA channel 0: source pointer (upper 4 bits)
					return (0);
					break;

				case 0xFFC4 :                      // DMA channel 0: destination pointer
					return (0);
					break;

				case 0xFFC6 :                      // DMA channel 0: destination pointer (upper 4 bits)
					return (0);
					break;

				case 0xFFC8 :                      // DMA channel 0: transfer count
					return (0);
					break;

				case 0xFFCA :                      // DMA channel 0: control word
					return (0);
					break;

				case 0xFFD0 :                      // DMA channel 1: source pointer
					return (0);
					break;

				case 0xFFD2 :                      // DMA channel 1: source pointer (upper 4 bits)
					return (0);
					break;

				case 0xFFD4 :                      // DMA channel 1: destination pointer
					return (0);
					break;

				case 0xFFD6 :                      // DMA channel 1: destination pointer (upper 4 bits)
					return (0);
					break;

				case 0xFFD8 :                      // DMA channel 1: transfer count
					return (0);
					break;

				case 0xFFDA :                      // DMA channel 1: control word
					return (0);
					break;

				case 0xFFE0 :                      // Refresh Control Unit register 1
					return (0);
					break;

				case 0xFFE2 :                      // Refresh Control Unit register 2
					return (0);
					break;

				case 0xFFE4 :                      // Refresh Control Unit register 3
					return (0);
					break;

				case 0xFFF0 :                      // Power Saving Control register (80C186 only)
					return (0);
					break;

				case 0xFFFE :                      // Relocation register
					return (this->cpu.relocreg & 0x00FF);
					break;

				case 0xFFFF :                      // Relocation register
					return ((this->cpu.relocreg & 0xFF00) >> 8);
					break;
			}
			break;

		case I80286 :				// 80286 specific I/O addresses
			switch (address) {
				case 0x0092 :				// A20 gate ( > 80286)
					// Do some A20 gating stuff
					return (0);
					break;
			}
			break;
	}
	return (0);
}

unsigned short ix86::readIOWord(unsigned int address) {
	return ((this->readIOByte(address)) | (this->readIOByte(address+1) << 8));
}

void ix86::writeIOByte(unsigned int address, unsigned char value) {
	switch (address) {				// Generic I/O addresses
		case 0x0080 :				// TUBE ULA register 1 data
		case 0x0082 :				// TUBE ULA register 1 status
		case 0x0084 :				// TUBE ULA register 2 data
		case 0x0086 :				// TUBE ULA register 2 status
		case 0x0088 :				// TUBE ULA register 3 data
		case 0x008A :				// TUBE ULA register 3 status
		case 0x008C :				// TUBE ULA register 4 data
		case 0x008E :				// TUBE ULA register 4 status
			WriteTubeFromParasiteSide(((address & 0x0000000F) >> 1), value);
			break;
	}

	switch (this->cpu.type) {
		case I80186 :				// 80186 specific I/O addresses
			switch (address) {
				case 0xFF22 :                      // Interrupt controller: end of interrupt register
					break;

				case 0xFF24 :                      // Poll register
					break;
	
				case 0xFF26 :                      // Poll Status Register
					break;

				case 0xFF28 :                      // Interrupt mask register
					break;

				case 0xFF2A :                      // Priority mask register
					break;

				case 0xFF2C :                      // In-service register
					break;

				case 0xFF2E :                      // Interrupt request register
					break;

				case 0xFF30 :                      // Interrupt status register
					break;

				case 0xFF32 :                      // Timer control register
					break;

				case 0xFF34 :                      // DMA0 control register
					break;

				case 0xFF36 :                      // DMA1 control register
					break;

				case 0xFF38 :                      // INT0 control register
					break;

				case 0xFF3A :                      // INT1 control register
					break;

				case 0xFF3C :                      // INT2 control register
					break;

				case 0xFF3E :                      // INT3 control register
					break;

				case 0xFF50 :                      // Timer 0: count register
					break;

				case 0xFF52 :                      // Timer 0: max count a register
					break;

				case 0xFF54 :                      // Timer 0: max count b register
					break;

				case 0xFF56 :                      // Timer 0: mode/control word
					break;

				case 0xFF58 :                      // Timer 1: count register
					break;

				case 0xFF5A :                      // Timer 1: max count a register
					break;

				case 0xFF5C :                      // Timer 1: max count b register
					break;

				case 0xFF5E :                      // Timer 1: mode/control word
					break;

				case 0xFF60 :                      // Timer 2: count register
					break;

				case 0xFF62 :                      // Timer 2: max count a register
					break;

				case 0xFF66 :                      // Timer 2: mode/control word
					break;

				case 0xFFA0 :                      // Upper memory chip select (UCS)
					this->cpu.umcs = (this->cpu.umcs & 0xFF00) | value;
					break;

				case 0xFFA1 :                      // Upper memory chip select (UCS)
					this->cpu.umcs = ((value << 8) | (this->cpu.umcs & 0x00FF));
					break;

				case 0xFFA2 :                      // Lower memory chip select (LCS)
					this->cpu.lmcs = (this->cpu.lmcs & 0xFF00) | value;
					break;

				case 0xFFA3 :                      // Lower memory chip select (LCS)
					this->cpu.lmcs = ((value << 8) | (this->cpu.lmcs & 0x00FF));
					break;

				case 0xFFA4 :                      // Peripheral chip select (PCS)
					this->cpu.pacs = (this->cpu.pacs & 0xFF00) | value;
					break;

				case 0xFFA5 :                      // Peripheral chip select (PCS)
					this->cpu.pacs = ((value << 8) | (this->cpu.pacs & 0x00FF));
					break;

				case 0xFFA6 :                      // Mid range memory chip select (MCS0-3) location
					break;

				case 0xFFA8 :                      // Mid range memory chip select (MCS0-3) size
					break;

				case 0xFFC0 :                      // DMA channel 0: source pointer
					break;

				case 0xFFC2 :                      // DMA channel 0: source pointer (upper 4 bits)
					break;

				case 0xFFC4 :                      // DMA channel 0: destination pointer
					break;

				case 0xFFC6 :                      // DMA channel 0: destination pointer (upper 4 bits)
					break;

				case 0xFFC8 :                      // DMA channel 0: transfer count
					break;

				case 0xFFCA :                      // DMA channel 0: control word
					break;

				case 0xFFD0 :                      // DMA channel 1: source pointer
					break;

				case 0xFFD2 :                      // DMA channel 1: source pointer (upper 4 bits)
					break;

				case 0xFFD4 :                      // DMA channel 1: destination pointer
					break;

				case 0xFFD6 :                      // DMA channel 1: destination pointer (upper 4 bits)
					break;

				case 0xFFD8 :                      // DMA channel 1: transfer count
					break;

				case 0xFFDA :                      // DMA channel 1: control word
					break;

				case 0xFFE0 :                      // Refresh Control Unit register 1
					break;

				case 0xFFE2 :                      // Refresh Control Unit register 2
					break;

				case 0xFFE4 :                      // Refresh Control Unit register 3
					break;

				case 0xFFF0 :                      // Power Saving Control register (80C186 only)
					break;

				case 0xFFFE :                      // Relocation register
					this->cpu.relocreg = (this->cpu.relocreg & 0xFF00) | value;
					break;

				case 0xFFFF :                      // Relocation register
					this->cpu.relocreg = ((value << 8) | (this->cpu.relocreg & 0x00FF));
					break;
			}
			break;

		case I80286 :				// 80286 specific I/O addresses
			switch (address) {
				case 0x0092 :				// A20 gate ( > 80286)
					// Do some A20 gating stuff
					break;
			}
			break;
	}
}

void ix86::writeIOWord(unsigned int address, unsigned short value) {
	this->writeIOByte(address, ((value & 0xFF00)>>8));
	this->writeIOByte(address+1, (value & 0x00FF));
}

bool ix86::evaluateConditionCode(unsigned char ConditionCode) {
	switch (ConditionCode) {
		case 0x00 :	// O (Overflow)
			return (this->cpu.flags.of == true);
			break;
		case 0x01 :	// NO (Not Overflow)
			return (this->cpu.flags.of == false);
			break;
		case 0x02 :	// C/B/NAE (Carry / Below / Not Above or Equal)
			return (this->cpu.flags.cf == true);
			break;
		case 0x03 :	// NC/AE/NB (Not Carry / Above or Equal / Not Below)
			return (this->cpu.flags.cf == false);
			break;
		case 0x04 :	// E/Z (Equal / Zero)
			return (this->cpu.flags.zf == true);
			break;
		case 0x05 :	// NE/NZ (Not Equal / Not Zero)
			return (this->cpu.flags.zf == false);
			break;
		case 0x06 :	// BE/NA (Below or Equal / Not Above)
			return ((this->cpu.flags.cf == true) || (this->cpu.flags.zf == true));
			break;
		case 0x07 :	// A/NBE (Above / Not Below nor Equal)
			return ((this->cpu.flags.cf == false) && (this->cpu.flags.zf == false));
			break;
		case 0x08 :	// S (Sign / Negative)
			return (this->cpu.flags.sf == true);
			break;
		case 0x09 :	// NS (Not Sign / Positive)
			return (this->cpu.flags.sf == false);
			break;
		case 0x0A :	// P/PE (Parity / Parity Even)
			return (this->cpu.flags.pf == true);
			break;
		case 0x0B :	// NP/PO (Not Parity / Parity Odd)
			return (this->cpu.flags.pf == false);
			break;
		case 0x0C :	// L/NGE (Less / Not Greater or Equal)
			return (this->cpu.flags.sf != this->cpu.flags.of);
			break;
		case 0x0D :	// GE / NL (Greater or Equal / Not Less)
			return (this->cpu.flags.sf == this->cpu.flags.of);
			break;
		case 0x0E :	// LE / NG (Less / Not Greater)
			return ((this->cpu.flags.zf == true) || (this->cpu.flags.zf != this->cpu.flags.of));
			break;
		case 0x0F :	// GE / NL (Greater or Equal / Not Less)
			return ((this->cpu.flags.zf == false) && (this->cpu.flags.zf == this->cpu.flags.of));
			break;
	}
	return (0);
}

unsigned int ix86::getOperandValue(bool w, unsigned char mod, unsigned char rm) {
	unsigned short	disp;

	switch (mod) {
		case 0x00 :
			if (rm == 0x06) {
				this->cpu.ip += 2;
				return ((this->readMemByte(this->getAddress(CS, IP-1)) << 8) | this->readMemByte(this->getAddress(CS, IP-2)));
			} else
				disp = this->readMemByte(this->getRegisterValue(w, rm));
			break;

		case 0x01 :
			disp = this->readMemByte(this->getAddress(CS, IP));
			this->cpu.ip++;
			if (disp & 0x80)
				disp |= 0xFF00;
			break;

		case 0x02 :
			disp = this->readMemWord(this->getAddress(CS, IP));
			this->cpu.ip += 2;
			break;

		case 0x03 :
			disp = this->getRegisterValue(w, rm);
			break;
	}

	if ((mod == 0x01) | (mod == 0x02)) {
		switch (rm) {
			case 0x00 :
				disp += this->cpu.bx.x + this->cpu.si;
				break;

			case 0x01 :
				disp += this->cpu.bx.x + this->cpu.di;
				break;

			case 0x02 :
				disp += this->cpu.bp + this->cpu.si;
				break;

			case 0x03 :
				disp += this->cpu.bp + this->cpu.si;
				break;

			case 0x04 :
				disp += this->cpu.si;
				break;

			case 0x05 :
				disp += this->cpu.di;
				break;

			case 0x06 :
				disp += this->cpu.bp;
				break;

			case 0x07 :
				disp += this->cpu.bx.x;
				break;
		}
	}
	
	return (disp);
}

void ix86::setOperandValue(bool w, unsigned char mod, unsigned char rm, unsigned short value) {
	unsigned short disp;

	switch (mod) {
		case 0x00 :
			if (rm == 0x06) {
				this->cpu.ip += 2;
//				return ((this->readMemByte(this->getAddress(CS, IP-1)) << 8) | this->readMemByte(this->getAddress(CS, IP-2)));
			} else
				disp = this->readMemByte(this->getRegisterValue(w, rm));
			break;

		case 0x01 :
			disp = this->readMemByte(this->getAddress(CS, IP));
			this->cpu.ip++;
			if (disp & 0x80)
				disp |= 0xFF00;
			break;

		case 0x02 :
			disp = this->readMemWord(this->getAddress(CS, IP));
			this->cpu.ip += 2;
			break;

		case 0x03 :
			disp = this->getRegisterValue(w, rm);
			break;
	}

	if ((mod == 0x01) | (mod == 0x02)) {
		switch (rm) {
			case 0x00 :
				disp += this->cpu.bx.x + this->cpu.si;
				break;

			case 0x01 :
				disp += this->cpu.bx.x + this->cpu.di;
				break;

			case 0x02 :
				disp += this->cpu.bp + this->cpu.si;
				break;

			case 0x03 :
				disp += this->cpu.bp + this->cpu.si;
				break;

			case 0x04 :
				disp += this->cpu.si;
				break;

			case 0x05 :
				disp += this->cpu.di;
				break;

			case 0x06 :
				disp += this->cpu.bp;
				break;

			case 0x07 :
				disp += this->cpu.bx.x;
				break;
		}
	}

	if (w)
		this->writeMemWord(disp, value);
	else
		this->writeMemByte(disp, (value & 0x00FF));
}

unsigned int ix86::getRegisterValue (bool w, unsigned char reg) {
	switch (w) {
		case false :
			switch (reg) {
				case 0x00 :
					return(this->cpu.ax.l);
					break;

				case 0x01 :
					return(this->cpu.cx.l);
					break;

				case 0x02 :
					return(this->cpu.dx.l);
					break;

				case 0x03 :
					return(this->cpu.bx.l);
					break;

				case 0x04 :
					return(this->cpu.ax.h);
					break;

				case 0x05 :
					return(this->cpu.cx.h);
					break;

				case 0x06 :
					return(this->cpu.dx.h);
					break;

				case 0x07 :
					return(this->cpu.bx.h);
					break;
			}
			break;

		case true :
			switch (reg) {
				case 0x00 :
					return(this->cpu.ax.x);
					break;

				case 0x01 :
					return(this->cpu.cx.x);
					break;

				case 0x02 :
					return(this->cpu.dx.x);
					break;

				case 0x03 :
					return(this->cpu.bx.x);
					break;

				case 0x04 :
					return(this->cpu.sp);
					break;

				case 0x05 :
					return(this->cpu.bp);
					break;

				case 0x06 :
					return(this->cpu.si);
					break;

				case 0x07 :
					return(this->cpu.di);
					break;
			}
			break;
	}
	// This should never happen, so throw an error message
	WriteLog("ix86::getRegisterValue: Function called with invalid value!");
	return(0);
}

void ix86::setRegisterValue (bool w, unsigned char reg, unsigned short value) {
	switch (w) {
		case false :
			switch (reg) {
				case 0x00 :
					this->cpu.ax.l = value & 0xff;
					break;

				case 0x01 :
					this->cpu.cx.l = value & 0xff;
					break;

				case 0x02 :
					this->cpu.dx.l = value & 0xff;
					break;

				case 0x03 :
					this->cpu.bx.l = value & 0xff;
					break;

				case 0x04 :
					this->cpu.ax.h = value & 0xff;
					break;

				case 0x05 :
					this->cpu.cx.h = value & 0xff;
					break;

				case 0x06 :
					this->cpu.dx.l = value & 0xff;
					break;

				case 0x07 :
					this->cpu.bx.l = value & 0xff;
					break;
			}
			break;

		case true :
			switch (reg) {
				case 0x00 :
					this->cpu.ax.x = value;
					break;

				case 0x01 :
					this->cpu.cx.x = value;
					break;

				case 0x02 :
					this->cpu.dx.x = value;
					break;

				case 0x03 :
					this->cpu.bx.x = value;
					break;

				case 0x04 :
					this->cpu.sp = value;
					break;

				case 0x05 :
					this->cpu.bp = value;
					break;

				case 0x06 :
					this->cpu.si = value;
					break;

				case 0x07 :
					this->cpu.di = value;
					break;
			}
			break;
	}
}

void ix86::dumpRegisters(void) {
	if (this->DEBUG)
		WriteLog("%08X   %02X   IP=%02X AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X BP=%04X SP=%04X CS=%04X DS=%04X ES=%04X SS=%04X SR=%08X", this->getAddress(CS, IP), this->cpu.instruction_reg, this->cpu.ip, this->cpu.ax.x, this->cpu.bx.x, this->cpu.cx.x, this->cpu.dx.x, this->cpu.si, this->cpu.di, this->cpu.bp, this->cpu.sp, this->cpu.cs, this->cpu.ds, this->cpu.es, this->cpu.ss, this->cpu.flags.reg);
}

void ix86::dumpTubeMemory(void) {
	FILE *fp;
	char path[256];

	strcpy(path, "C:\\Users\\Virustest\\Desktop\\BeebEm414_mc68k\\BeebEm\\x86TubeMemory.bin");

	fp = fopen(path, "rb");
	if (fp) {
		fclose(fp);	// Memory dump file already exists, so don't overwrite
	} else {
		fp = fopen(path, "wb");
//		fwrite(this->ramMemory, this->RAM_SIZE, 1, fp);
		for (unsigned int i=0;i < (EXTERNAL_ADDRESS_MASK +1); i++)
			fputc(this->readMemByte(i), fp);
		fclose(fp);
		WriteLog("ix86::dumpTubeMemory - RAM memory dumped to %s\n", path);
	}
}

void ix86::setZeroFlag(unsigned int value) {
	this->cpu.flags.zf = (value ? false : true);
}

void ix86::setSignFlag(bool w, unsigned int value) {
	if (w)
		this->cpu.flags.sf = ((value & 0x8000) ? true : false);
	else
		this->cpu.flags.sf = ((value & 0x80) ? true : false);
}

void ix86::setParityFlag(bool w, unsigned int value) {
	static const bool parity[0x100] = {
		true , false, false, true , false, true , true , false, false, true , true , false, true , false, false, true ,
		false, true , true , false, true , false, false, true , true , false, false, true , false, true , true , false, 
		false, true , true , false, true , false, false, true , true , false, false, true , false, true , true , false,
		true , false, false, true , false, true , true , false, false, true , true , false, true , false, false, true , 
		false, true , true , false, true , false, false, true , true , false, false, true , false, true , true , false,
		true , false, false, true , false, true , true , false, false, true , true , false, true , false, false, true , 
		true , false, false, true , false, true , true , false, false, true , true , false, true , false, false, true ,
		false, true , true , false, true , false, false, true , true , false, false, true , false, true , true , false, 
		false, true , true , false, true , false, false, true , true , false, false, true , false, true , true , false,
		true , false, false, true , false, true , true , false, false, true , true , false, true , false, false, true , 
		true , false, false, true , false, true , true , false, false, true , true , false, true , false, false, true ,
		false, true , true , false, true , false, false, true , true , false, false, true , false, true , true , false, 
		true , false, false, true , false, true , true , false, false, true , true , false, true , false, false, true ,
		false, true , true , false, true , false, false, true , true , false, false, true , false, true , true , false, 
		false, true , true , false, true , false, false, true , true , false, false, true , false, true , true , false,
		true , false, false, true , false, true , true , false, false, true , true , false, true , false, false, true
	};

	if (w)
		this->cpu.flags.pf = parity[((value & 0xFF00) >> 8)] ^ parity[value & 0x00FF];
	else
		this->cpu.flags.pf = parity[value & 0x00FF];
}

void ix86::setOverflowFlag(bool w, unsigned int result, unsigned int value1, unsigned int value2) {
	if (w)
		this->cpu.flags.of = (((result ^ value1) & (result ^ value2)) & 0x8000) ? true : false;
	else
		this->cpu.flags.of = (((result ^ value1) & (result ^ value2)) & 0x0080) ? true : false;
}

void ix86::setAdjustFlag(unsigned int result, unsigned int value1, unsigned int value2) {
	this->cpu.flags.af = ((result ^ value1 ^ value2) & 0x10) ? true : false;
}

void ix86::setCarryFlag(bool w, unsigned int value) {
	if (w)
		this->cpu.flags.cf = (value & 0x10000) ? true : false;
	else
		this->cpu.flags.cf = (value & 0x100) ? true : false;
}
