/* Torch Z80 communicator second processor module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "../devices/z80a.h"					// Included for mc68k object
#include "../devices/r6522.h"
#include "../devices/i8255.h"

#ifndef TRUE
#define TRUE		1
#endif

#ifndef FALSE
#define FALSE		0
#endif

//extern bool Enable_CoPro_Torch_Z80;
extern bool CoProTorchZ80Active;		// Global variable indicating wether or not the Casper 68k Co-Pro is selected and active



class copro_torch_z80 {
public:
	/****** Constructor / Deconstructor ******************************************/
	copro_torch_z80(void);
	~copro_torch_z80(void);

	bool			DEBUG;

	/****** Chip definitions *****************************************************/
	r6522			*via;
	i8255			*ppi;
	z80				*cpu;

	/****** RAM and ROM memory ***************************************************/
	unsigned char	*romMemory;
	unsigned char	*ramMemory;
	unsigned int	RAM_SIZE;			// Size of RAM memory
	unsigned int	ROM_SIZE;			// Size of ROM memory
	unsigned int	RAM_ADDR;			// Start address of RAM memory
	unsigned int	ROM_ADDR;			// Start address of ROM memory
	char			biosFile[128];

	void			copro_torch_z80::Reset(void);
	void			copro_torch_z80::Exec(int Cycles);
	unsigned char	copro_torch_z80::readByte(unsigned int address);
	void			copro_torch_z80::writeByte(unsigned int address, unsigned char value);
	void			copro_torch_z80::InitMemory(void);
};
