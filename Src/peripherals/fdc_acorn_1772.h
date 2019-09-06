/* Acorn WD1772 FDC module for the BeebEm emulator
  Written by Eelco Huininga 2017
*/

#include "../devices/wd177x.h"
#include "../devices/ls174.h"				// Included for 74ls174 object (using a seperate object definition instead of just a local variable, so we can log reads and writes)

extern bool fdc_acorn_1772_enabled;		// Global variable indicating wether or not the Acorn 1772 FDC is selected and active



class fdc_acorn_1772 {
public:
	/****** Constructor / Deconstructor ******************************************/
	fdc_acorn_1772::fdc_acorn_1772(void);
	fdc_acorn_1772::~fdc_acorn_1772(void);

	bool			DEBUG;

	/****** Chip definitions *****************************************************/
	wd177x			*fdc;

	/****** Co-processor registers ***********************************************/
	ls174			*drive_control_register;

	void			fdc_acorn_1772::Reset(void);
	void			fdc_acorn_1772::Exec(int Cycles);
	unsigned char	fdc_acorn_1772::readByte(unsigned char address);
	void			fdc_acorn_1772::writeByte(unsigned char address, unsigned char value);
};
