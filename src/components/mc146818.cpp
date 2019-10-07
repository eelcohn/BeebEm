/* Motorola MC146818 RTC (Real Time Clock) object module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include <stdio.h>					// Included for FILE *
#include <string.h>					// Included for strcpy, strcat
#include <ctime>					// Included for tm object

#include "../main.h"				// Included for WriteLog()
#include "mc146818.h"

	time_t			now = time(0);
	tm				*ltm = localtime(&now);
//	tm				*ltm = localtime((time_t *) time(0));



/* Constructor / Deconstructor */

mc146818::mc146818() {
}

mc146818::~mc146818(void) {
}



/* Main code */

void mc146818::Reset(void) {
	unsigned char	i;

	this->irq = true;

	for (i = 0; i < 63; i++)
		this->registers[i] = 0;
}

void mc146818::Exec(int Cycles) {
	if ((this->registers[1] == ltm->tm_sec) && (this->registers[3] == ltm->tm_min) && (this->registers[3] == ltm->tm_hour)) {
		this->registers[0x0C] |= 0x40;		// Set AF flag
		if (this->registers[0x0B] & 0x20) {	// Check the AIE flag
			this->irq = false;				// Lower IRQ output
			this->registers[0x0C] |= 0x80;	// Set IRQF flag
		}
	}
}

unsigned char mc146818::Read(void) {
	unsigned char	Value;

	switch (this->SelectedRegister) {
		case 0x00 :	// Seconds
			if (this->registers[0x0C] & 0x04)			// Check DM flag in register C
				Value = ltm->tm_sec;					// Return binary value
			else
				Value = this->binToBcd(ltm->tm_sec);	// BCD value
			break;

		case 0x01 :	// Seconds alarm
			break;

		case 0x02 :	// Minutes
			if (this->registers[0x0C] & 0x04)			// Check DM flag in register C
				Value = ltm->tm_min;
			else
				Value = this->binToBcd(ltm->tm_min);
			break;

		case 0x03 :	// Minutes alarm
			break;

		case 0x04 :	// Hours
			if (this->registers[0x0C] & 0x04)			// Check DM flag in register C
				Value = ltm->tm_hour;
			else
				Value = this->binToBcd(ltm->tm_hour);
			break;

		case 0x05 :	// Hours alarm
			break;

		case 0x06 :	// Day of week
			if (this->registers[0x0C] & 0x04)			// Check DM flag in register C
				Value = ltm->tm_wday + 1;
			else
				Value = this->binToBcd(ltm->tm_wday);
			break;

		case 0x07 :	// Date of month
			if (this->registers[0x0C] & 0x04)			// Check DM flag in register C
				Value = ltm->tm_mday;
			else
				Value = this->binToBcd(ltm->tm_mday);
			break;

		case 0x08 :	// Month
			if (this->registers[0x0C] & 0x04)			// Check DM flag in register C
				Value = ltm->tm_mon + 1;
			else
				Value = this->binToBcd(ltm->tm_mon);
			break;

		case 0x09 :	// Year
			if (this->registers[0x0C] & 0x04)			// Check DM flag in register C
				Value = (ltm->tm_year % 100);
			else
				Value = this->binToBcd(ltm->tm_year);
			break;

		case 0x0C :	// Register C
			Value = this->registers[0x0C];
			this->registers[0x0C] = 0x00;				// Clear all flags on read
			this->irq = true;					// Clear IRQ
			break;

		default :
			Value = this->registers[this->SelectedRegister];
			break;
	}

	if (this->DEBUG)
		WriteLog("mc146818::Read selectedRegister=%02X Value=%02X", this->SelectedRegister, Value);

	return(Value);
}

void mc146818::Write(unsigned char Value) {
	if (this->DEBUG)
		WriteLog("mc146818::Write selectedRegister=%02X Value=%02X", this->SelectedRegister, Value);

}

void mc146818::setAD(unsigned char Value) {
	this->ad = Value;
}

unsigned char mc146818::binToBcd(unsigned char Value) {
	return (((Value / 10) << 4) + (Value % 10));
}

unsigned char mc146818::bcdToBin(unsigned char Value) {
	return((Value >> 4) * 10 + (Value & 0x0F));
}
