/* CiscOS 68k second processor object module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#include "../components/mc68k.h"		// Included for mc68k object
#include "../components/ula9c018.h"

//extern bool Enable_CiscOS68k;	// See tube.h for description
extern bool mc68kTube_CiscOS;	// Global variable indicating wether or not the CiscOS 68k Co-Pro is selected and active



class copro_ciscos {
public:
	/****** Constructor / Deconstructor ******************************************/
	copro_ciscos(void);
	~copro_ciscos(void);

	bool			DEBUG;

	/****** Chip definitions *****************************************************/
	ula9c018		*tube_ula;
	mc68k			*cpu;

	/****** Signals **************************************************************/
	bool			BOOTFLAG;

	/****** RAM and ROM memory ***************************************************/
	unsigned char	*romMemory;
	unsigned char	*ramMemory;
	unsigned int	RAM_SIZE;			// Size of RAM memory
	unsigned int	ROM_SIZE;			// Size of ROM memory
	unsigned int	RAM_ADDR;			// Start address of RAM memory
	unsigned int	ROM_ADDR;			// Start address of ROM memory
	unsigned int	TUBE_ULA_ADDR;
	char			biosFile[128];

	void			Reset(void);
	void			Exec(int Cycles);
	unsigned char	readByte(unsigned int address);
	void			writeByte(unsigned int address, unsigned char value);
	void			InitMemory(void);
};
