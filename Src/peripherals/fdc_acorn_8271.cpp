/* Acorn i8271 FDC module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "fdc_acorn_8271.h"
//#include "main.h"					// Included for WriteLog()

bool fdc_acorn_8271_enabled	= false;



/* Constructor / Deconstructor */

fdc_acorn_8271::fdc_acorn_8271(void) {
	this->DEBUG					= false;

	this->fdc				= new i8271;

	this->fdc->DEBUG		= false;
}

fdc_acorn_8271::~fdc_acorn_8271(void) {
	delete this->fdc;
}

/* Reset all hardware components of the expansion board */

void fdc_acorn_8271::Reset(void) {
	this->fdc->Reset();
}

/* Emulate all hardware components of the expansion board */

void fdc_acorn_8271::Exec(int Cycles) {
	// Emulate the chips on the expansion board
	this->fdc->Exec(Cycles);
}
