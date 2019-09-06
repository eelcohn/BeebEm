/* Cumana 68008 second processor module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "../components/mc68k.h"					// Included for mc68k object
#include "../components/mc6821.h"
#include "../components/wd177x.h"
#include "../components/mc146818.h"

//extern bool Enable_Cumana68k;
extern bool mc68kTube_Cumana;		// Global variable indicating wether or not the Cumana 68k Co-Pro is selected and active



class copro_cumana {
public:
	/****** Constructor / Deconstructor ******************************************/
	copro_cumana(void);
	~copro_cumana(void);

	bool			DEBUG;

	/****** Chip definitions *****************************************************/
	mc6821			*sasipia;
	mc6821			*rtcpia;
	wd177x			*fdc;
	mc146818		*rtc;
	mc68k			*cpu;

	/****** RAM and ROM memory ***************************************************/
	unsigned char	*ramMemory;
	unsigned int	RAM_SIZE;			// Size of RAM memory
	unsigned int	ROM_SIZE;			// Size of ROM memory
	unsigned int	RAM_ADDR;			// Start address of RAM memory
	unsigned int	ROM_ADDR;			// Start address of ROM memory

	void			Reset(void);
	void			Exec(int Cycles);
//	unsigned char	readByte(unsigned int address);
//	void			writeByte(unsigned int address, unsigned char value);
	void			InitMemory(void);
};
