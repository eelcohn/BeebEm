/* Torch Z80 communicator second processor module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "../components/z80a.h"					// Included for mc68k object
#include "../components/r6522.h"
#include "../components/i8255.h"

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

	void			Reset(void);
	void			Exec(int Cycles);
	unsigned char	readByte(unsigned int address);
	void			writeByte(unsigned int address, unsigned char value);
	void			InitMemory(void);
};
