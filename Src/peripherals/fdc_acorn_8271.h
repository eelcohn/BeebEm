/* Acorn i8271 FDC module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "../devices/i8271.h"

extern bool fdc_acorn_i8271_enabled;		// Global variable indicating wether or not the Acorn 8271 FDC is selected and active



class fdc_acorn_8271 {
public:
	/****** Constructor / Deconstructor ******************************************/
	fdc_acorn_8271::fdc_acorn_8271(void);
	fdc_acorn_8271::~fdc_acorn_8271(void);

	bool			DEBUG;

	/****** Chip definitions *****************************************************/
	i8271			*fdc;

	void			fdc_acorn_8271::Reset(void);
	void			fdc_acorn_8271::Exec(int Cycles);
	unsigned char	fdc_acorn_8271::readByte(unsigned char address);
	void			fdc_acorn_8271::writeByte(unsigned char address, unsigned char value);
};
