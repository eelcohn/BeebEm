/* Casper 68000 second processor module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "../components/mc68k.h"	// Included for mc68k object
#include "../components/r6522.h"
#include "../components/rom.h"
#include "../components/ram.h"

//extern bool Enable_Casper68k;
extern bool mc68kTube_Casper;		// Global variable indicating wether or not the Casper 68k Co-Pro is selected and active



class copro_casper {
public:
	/****** Constructor / Deconstructor ******************************************/
					copro_casper::copro_casper(void);
					copro_casper::~copro_casper(void);

	bool			DEBUG;

	/****** Chip definitions *****************************************************/
	r6522			*host_via;
	r6522			*parasite_via;
	mc68k			*cpu;
	rommemory		*rom;
	rammemory		*ram;

	/****** RAM and ROM memory ***************************************************/
	unsigned int	RAM_SIZE;			// Size of RAM memory
	unsigned int	ROM_SIZE;			// Size of ROM memory
	unsigned int	RAM_ADDR;			// Start address of RAM memory
	unsigned int	ROM_ADDR;			// Start address of ROM memory
	char			biosFile[128];

	void			copro_casper::Reset(void);
	void			copro_casper::Exec(int Cycles);
	unsigned char	copro_casper::readByte(unsigned int address);
	void			copro_casper::writeByte(unsigned int address, unsigned char value);
	void			copro_casper::InitMemory(void);
};
