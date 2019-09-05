/* Motorola MC146818 RTC (Real Time Clock) module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef MC146818_HEADER
#define MC146818_HEADER



/****** Class definition *********************************************************/
class	mc146818 {
public:
	/****** Constructor / Deconstructor ******************************************/
	mc146818::mc146818(void);
	mc146818::~mc146818(void);

	bool			DEBUG;

	/****** Signals and ports ****************************************************/
	unsigned char	ad;
	bool			as;		// Address strobe
	bool			ds;		// Data strobe
	bool			rw;		// Read /Write
	bool			irq;
	bool			reset;

	/****** Public functions *****************************************************/
	void			mc146818::Reset(void);
	void			mc146818::Exec(int Cycles);
	unsigned char	mc146818::Read(void);
	void			mc146818::Write(unsigned char Value);
	void			mc146818::setAD(unsigned char Value);

private:
	/****** Registers ************************************************************/
	unsigned char	registers[64];			// All mc146818 registers

	/****** Internal Registers ***************************************************/
	unsigned char	SelectedRegister;	// Address written to RTC after /AS

	/****** Private functions ****************************************************/
	unsigned char	mc146818::binToBcd(unsigned char Value);
	unsigned char	mc146818::bcdToBin(unsigned char Value);
};
#endif
