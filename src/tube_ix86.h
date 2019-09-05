/* Intel x86 TUBE module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef TRUE
#define TRUE		1
#endif

#ifndef FALSE
#define FALSE		0
#endif

extern int Enable_ix86;
extern int ix86Tube;								// Global variable indicating wether or not the Intel x86 Co-Pro is selected and active
static long cyclecount;



enum ARCHITECTURES {ACORN186 = 1, ACORN286, TORCH_GRADUATE};
enum CPUTYPES	{I8088=1, I8086, I80186, I80188, I80286, I80386, UNKNOWNCPU=255};
enum FPUTYPES	{NOFPU=0, I8087, I80187, I80287, I80387, INTERNALFPU, UNKNOWNFPU=255};
enum MMUTYPES	{NOMMU=0, INTERNALMMU, UNKNOWNMMU=255};

/****** CPU stuff ************************************************************/

typedef union {
	unsigned short	x:16;
	unsigned char	l:8;
	unsigned char	h:8;
} REG16;

typedef union {
	unsigned int	e:32;
	unsigned short	x:16;
	unsigned char	l:8;
	unsigned char	h:8;
} REG32;

typedef union {				// FLAGS Register
	unsigned int reg;
	struct {
		bool cf:1;			// Carry
		bool reserved1:1;
		bool pf:1;			// Parity
		bool reserved3:1;
		bool af:1;			// Adjust flag
		bool reserved5:1;
		bool zf:1;			// Zero
		bool sf:1;			// Sign flag
		bool tf:1;			// Trap flag
		bool if:1;			// Interrupt enable flag
		bool df:1;			// Direction flag
		bool of:1;			// Overflow flag
		bool iopl1:1;		// I/O Privilege flag (286+)
		bool iopl2:1;
		bool nt:1;			// Nested task (286+)
		bool reserved15:1;
		bool rf:1;			// Resume flag (386+)
		bool vm:1;			// Virtual 8086 mode (386+)
		bool ac:1;			// Alignment check (386+)
		bool vif:1;			// Virtual interrupt flag (Pentium+)
		bool vip:1;			// Virtual interrupt pending (Pentium+)
		bool id:1;			// Able to use CPUID instruction (Pentium+)
		bool reserved22:1;
		bool reserved23:1;
		bool reserved24:1;
		bool reserved25:1;
		bool reserved26:1;
		bool reserved27:1;
		bool reserved28:1;
		bool reserved29:1;
		bool reserved30:1;
		bool reserved31:1;
	} flag;
} FLAGS;

typedef struct {			// All CPU registers
	unsigned char	type;
	unsigned short	instruction_reg;
	REG16			AX;		// Main registers
	REG16			BX;
	REG16			CX;
	REG16			DX;
	unsigned short	SI;		// Index registers
	unsigned short	DI;
	unsigned short	BP;
	unsigned short	SP;
	unsigned short	IP;		// Instruction register (Program counter)
	unsigned short	CS;		// Segment registers
	unsigned short	DS;
	unsigned short	ES;
	unsigned short	SS;
	FLAGS			flags,
} CPU;

/****** FPU stuff ************************************************************/

typedef struct {			// Hack because MSVC doesn't support 80 bit floats anymore
	short	exponent;
	double	fraction;
} long_double;

typedef struct {						//  All FPU registers ( 8087 / 80187 / 80287 / 80387)
	unsigned char	type;
//	long_double		st[8];				// Floating Point Data Registers
} FPU;

/****** MMU stuff ************************************************************/

typedef struct {
	unsigned char	type;
} MMU;


static unsigned int		INTERNAL_ADDRESS_MASK;
static unsigned int		EXTERNAL_ADDRESS_MASK;



/****** Class definition *********************************************************/

class	tube_ix86 {
public:
	/****** Constructor / Deconstructor ******************************************/
	tube_ix86(int Architecture);
	~tube_ix86(void);

	/****** Processors ***********************************************************/
	CPU cpu;
	FPU fpu;
	MMU mmu;

	/****** Hardware related variables *******************************************/
	unsigned char	*romMemory;
	unsigned char	*ramMemory;
	unsigned int	RAM_SIZE;
	unsigned int	ROM_SIZE;
	unsigned int	TUBE_ULA_ADDR;
	unsigned int	ROM_ADDR;
	bool			BOOTFLAG;
	unsigned char	biosFile[128];

	/****** Flags for signalling exceptions **************************************/
	signed short	pendingException;	// Exception 0-255, or -1 for normal operation
	unsigned char	interruptLevel;		// Represents the state of the IPL[0..2] interrupt lines on the CPU
	signed char		currentInterrupt;	// Represents the current interrupt being handled by the CPU (can this be replaced by FC0..FC2???)

	void			tube_mc68k::Reset(void);
	void			tube_mc68k::Exec(int Cycles);
	unsigned char	tube_mc68k::readByte(unsigned int address);
	unsigned short	tube_mc68k::readWord(unsigned int address);
	unsigned int	tube_mc68k::readLong(unsigned int address);
	void			tube_mc68k::writeByte(unsigned int address, unsigned char value);
	void			tube_mc68k::writeWord(unsigned int address, unsigned short value);
	void			tube_mc68k::writeLong(unsigned int address, unsigned int value);
	unsigned int	getOperandValue(unsigned char Operand, unsigned char Size);
	void			setOperandValue(unsigned char Operand, unsigned char Size, unsigned int Value);
	bool			tube_mc68k::evaluateConditionCode(unsigned char ConditionCode);
	void			tube_mc68k::setStatusRegister (unsigned short ANDmask, bool C, bool V, bool Z, bool N, bool X);
	unsigned int	tube_mc68k::signExtend(unsigned short value);
	void			tube_mc68k::dumpTubeMemory(void);
	void			tube_mc68k::ADDhandler(void);
	void			tube_mc68k::ADDAhandler(void);
	void			tube_mc68k::ADDXhandler(void);
	void			tube_mc68k::ASxLSx_handler(void);
	void			tube_mc68k::ROxROXx_handler(void);
};
