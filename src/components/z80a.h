/* Zilog Z80 CPU object module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef Z80_HEADER
#define Z80_HEADER

//static long cyclecount;



enum Z80_ARCHITECTURES	{ACORN_Z80 = 1, TORCH_Z80, PEDL_Z80};
enum Z80_CPUTYPES		{Z80=1, Z80_UNKNOWNCPU=255};

/****** CPU stuff ************************************************************/

typedef union {				// SR: Status Register / Condition Code Register
	unsigned short reg;
	struct {
		bool c:1;			// Carry
		bool n:1;			// Negative
		bool p:1;			// Parity
		bool undefined3:1;
		bool h:1;			// Adjust
		bool undefined5:1;
		bool z:1;			// Zero
		bool s:1;			// Sign
	} flag;
} FLAGS;

typedef struct {			// All CPU registers
	unsigned char	type;
	unsigned char	instruction_reg;
	union {
		unsigned short		af;
		struct {
			unsigned char	a;
			FLAGS			f;
		};
	};
	union {
		unsigned short		bc;
		struct {
			unsigned char	b;
			unsigned char	c;
		};
	};
	union {
		unsigned short		de;
		struct {
			unsigned char	d;
			unsigned char	e;
		};
	};
	union {
		unsigned short		hl;
		struct {
			unsigned char	h;
			unsigned char	l;
		};
	};
	unsigned char	i;
	unsigned char	r;
	unsigned short	ix;
	unsigned short	iy;
	unsigned short	sp;
	unsigned short	pc;
	bool			iff1;			// Interrupt flag 1
	bool			iff2;			// Interrupt flag 2
	bool			halt;			// 
} CPU_Z80;



/****** Class definition *********************************************************/

class	z80 {
public:
	/****** Constructor / Deconstructor ******************************************/
	z80::z80(int CPUType);
	z80::~z80(void);
	int				Architecture;

	bool			DEBUG;

	/****** Processors ***********************************************************/
	CPU_Z80	cpu;

	/****** Signals and ports ****************************************************/

	/****** Flags for signalling exceptions **************************************/

	/****** Public functions *****************************************************/
	void			z80::Reset(void);
	void			z80::Exec(int Cycles);

private:
	/****** Private functions ****************************************************/
	unsigned char	z80::readByte(unsigned int address);
	unsigned short	z80::readWord(unsigned int address);
	unsigned int	z80::readLong(unsigned int address);
	void			z80::writeByte(unsigned int address, unsigned char value);
	void			z80::writeWord(unsigned int address, unsigned short value);
	void			z80::writeLong(unsigned int address, unsigned int value);
	bool			z80::evaluateConditionCode(unsigned char ConditionCode);
	void			z80::dumpMemory(void);
	void			z80::dumpRegisters(void);

	/****** Internal variables ***************************************************/
};
#endif
