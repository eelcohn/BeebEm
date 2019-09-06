/* Acorn WD1772 FDC module for the BeebEm emulator
  Written by Eelco Huininga 2017
*/

#include "fdc_acorn_1772.h"
//#include "main.h"					// Included for WriteLog()

bool fdc_acorn_1772_enabled	= false;



/* Constructor / Deconstructor */

fdc_acorn_1772::fdc_acorn_1772(void) {
	this->DEBUG						= false;

	this->fdc						= new wd177x(WD1772);
	this->fdc->DEBUG				= false;

	this->drive_control_register	= new ls174;
	this->drive_control_register->DEBUG	= false;
}

fdc_acorn_1772::~fdc_acorn_1772(void) {
	delete this->drive_control_register;
	delete this->fdc;
}

/* Reset all hardware components of the expansion board */

void fdc_acorn_1772::Reset(void) {
	this->fdc->Reset();
}

/* Emulate all hardware components of the expansion board */

void fdc_acorn_1772::Exec(int Cycles) {
	// Emulate the chips on the expansion board
	this->fdc->Exec(Cycles);
}
