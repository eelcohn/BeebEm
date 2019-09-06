/* Technomatic Multiform Z80 co-processor module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "chips/z80a.h"					// Included for z80 object
#include "chips/ls374.h"				// Included for 74ls374 object (using a seperate object definition instead of just a local variable, so we can log reads and writes)

#ifndef TRUE
#define TRUE		1
#endif

#ifndef FALSE
#define FALSE		0
#endif

//extern bool Enable_PEDLZ80;
extern bool CoProTechnomaticZ80Enable;		// Global variable indicating wether or not the Casper 68k Co-Pro is selected and active



class copro_multiform_z80 {
public:
	/****** Constructor / Deconstructor ******************************************/
	copro_multiform_z80(void);
	~copro_multiform_z80(void);

	bool			DEBUG;

	/****** Chip definitions *****************************************************/
	z80				*cpu;

	/****** Co-processor registers ***********************************************/
	ls374	*host_register1;
	ls374	*host_register2;
	ls374	*parasite_register1;
	ls374	*parasite_register2;

	/****** RAM and ROM memory ***************************************************/
	unsigned char	*romMemory;
	unsigned char	*ramMemory;
	unsigned int	RAM_SIZE;			// Size of RAM memory
	unsigned int	ROM_SIZE;			// Size of ROM memory
	unsigned int	RAM_ADDR;			// Start address of RAM memory
	unsigned int	ROM_ADDR;			// Start address of ROM memory
	char			biosFile[128];

	void			copro_multiform_z80::Reset(void);
	void			copro_multiform_z80::Exec(int Cycles);
	unsigned char	copro_multiform_z80::readByte(unsigned int address);
	void			copro_multiform_z80::writeByte(unsigned int address, unsigned char value);
	void			copro_multiform_z80::InitMemory(void);
};
