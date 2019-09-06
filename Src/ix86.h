/* Intel x86 TUBE module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

extern int Enable_ix86;
extern int ix86Tube;								// Global variable indicating wether or not the Intel x86 Co-Pro is selected and active
static long cyclecount;



enum ARCHITECTURES {ACORN186 = 1, ACORN286, TORCH_GRADUATE};
enum CPUTYPES	{I8088=1, I8086, NECV20, NECV30, I80188, I80186, I80286, I80386, UNKNOWNCPU=255};
enum FPUTYPES	{NOFPU=0, I8087, I80187, I80287, I80387, INTERNALFPU=254, UNKNOWNFPU=255};
enum MMUTYPES	{NOMMU=0, INTERNALMMU=254, UNKNOWNMMU=255};
enum REGISTERS	{AX, BX, CX, DX, SI, DI, BP, SP, CS, DS, ES, SS, IP};

/****** CPU stuff ************************************************************/


static const bool parity[0x100] = { 
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1 
}; 

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

typedef union {
	unsigned char	reg;
	struct {
		bool w:1;
		bool s:1;
		bool bit2:1;
		bool bit3:1;
		bool bit4:1;
		bool bit5:1;
		bool bit6:1;
		bool bit7:1;
	};
} INSTRUCTION_REG;

typedef union {				// FLAGS Register
	unsigned int reg;
//	struct {
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
		bool iopl1:1;			// I/O Privilege flag (286+)
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
//	} flag;
} FLAGS;

typedef union {				// MSW (Machine Status Word) Register (286+) / CR0 (Control Register 0) (386+)
	unsigned int reg;
	struct {
		bool pe:1;			// Protection Enable (286+)
		bool mp:1;                      // Math Present (286+)
		bool em:1;			// Processor Extension Emulation (286+)
		bool ts:1;                      // Task Switched (286+)
		bool et:1;			// Extension Type (286+)
		bool ne:1;			// Numeric Error (386+)
		bool reserved06:1;
		bool reserved07:1;
		bool reserved08:1;
		bool reserved09:1;
		bool reserved10:1;
		bool reserved11:1;
		bool reserved12:1;
		bool reserved13:1;
		bool reserved14:1;
		bool reserved15:1;
	} msw;
} MSW;

typedef union {					// CR0 (Control Register 0) (386+)
	unsigned int reg;
	MSW	msw:16;				// First 16 bytes are MSW
	struct {
		bool wp:1;			// Write Protect (386+)
		bool reserved17:1;
		bool am:1;			// Alignment Mask (386+)
		bool reserved19:1;
		bool reserved20:1;
		bool reserved21:1;
		bool reserved22:1;
		bool reserved23:1;
		bool reserved24:1;
		bool reserved25:1;
		bool reserved26:1;
		bool reserved27:1;
		bool reserved28:1;
		bool nw:1;			// Not-write through (386+)
		bool cd:1;			// Cache disable (386+)
		bool pg:1;                      // Paging (386+)
	} cr0;
} CR0;

struct GDT_STR					// Structure for Global Descriptor Table
{
    unsigned short seg_length_0_15;
    unsigned short base0_15;
    unsigned char  base16_23;
    unsigned char  flags;
    unsigned char  seg_length_16_19:4;
    unsigned char  access:4;
    usigned  char  base24_31;
};

typedef struct {			// All CPU registers
	unsigned char	type;
//	unsigned short	instruction_reg;
	INSTRUCTION_REG	instruction_reg;
	REG16		ax;		// Main registers
	REG16		bx;
	REG16		cx;
	REG16		dx;
	unsigned short	si;		// Index registers
	unsigned short	di;
	unsigned short	bp;
	unsigned short	sp;
	unsigned short	ip;		// Instruction register (Program counter)
	unsigned short	cs;		// Segment registers
	unsigned short	ds;
	unsigned short	es;
	unsigned short	ss;
	unsigned short	fs;		// (386+)
	unsigned short	gs;		// (386+)

	MSW		msw;		// Machine Status Word (286+)
	unsigned long	gdt;		// Global Descriptor Table Register (286+)
	struct {
		unsigned short base;	// Interrupt Descriptor Table Register base (286+)
		unsigned long limit;	// Interrupt Descriptor Table Register (286+)
	} idtr;
	unsigned long	ldt;		// Local Descriptor Table Register (286+)

	unsigned long	cr0;		// Control Register 0 (386+)
	unsigned long	cr1;		// Control Register 1 (386+)
	unsigned long	cr2;		// Control Register 2 (386+)
	unsigned long	cr3;		// Control Register 3 (386+)

	unsigned long	dr0;		// Debug Register 0 (386+)
	unsigned long	dr1;		// Debug Register 1 (386+)
	unsigned long	dr2;		// Debug Register 2 (386+)
	unsigned long	dr3;		// Debug Register 3 (386+)
	unsigned long	dr4;		// Debug Register 4 (386+)
	unsigned long	dr5;		// Debug Register 5 (386+)
	unsigned long	dr6;		// Debug Register 6 (386+)
	unsigned long	dr7;		// Debug Register 7 (386+)

	FLAGS		flags;
	bool		halt;
	bool		wait;
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



static unsigned int		INTERNAL_ADDRESS_MASK;
static unsigned int		EXTERNAL_ADDRESS_MASK;



/****** Class definition *********************************************************/

class	ix86 {
public:
	/****** Constructor / Deconstructor ******************************************/
			ix86(int Architecture);
			~ix86(void);

	/****** Processors ***********************************************************/
	CPU		cpu;
	FPU		fpu;

	/****** Signals and ports ****************************************************/
	bool		intr;					// Interrupt signal
	bool		lock;					// Lock signal
	bool		nmi;					// Non-maskable interrupt (NMI) signal

	/****** Hardware related variables *******************************************/
	unsigned char	*romMemory;
	unsigned char	*ramMemory;
	unsigned int	RAM_SIZE;
	unsigned int	ROM_SIZE;
	unsigned int	TUBE_ULA_ADDR;
	unsigned int	ROM_ADDR;
	bool		BOOTFLAG;
	unsigned char	biosFile[128];

	unsigned char	interrupt_nr;		// Represents the A7-A3 and S2-S0 lines during an interrupt cycle; these represent the interrupt vector

	/****** Flags for signalling exceptions **************************************/
	signed short	pendingInterrupt;	// Interrupt 0-255, or -1 for normal operation
	unsigned char	interruptLevel;		// Represents the state of the IPL[0..2] interrupt lines on the CPU
	signed char		currentInterrupt;	// Represents the current interrupt being handled by the CPU (can this be replaced by FC0..FC2???)

	void			ix86::Reset(void);
	void			ix86::Exec(int Cycles);
	unsigned int	ix86::getAddress(unsigned char Segment_reg, unsigned char Offset_reg);
	unsigned char	ix86::readByte(unsigned int address);
	unsigned short	ix86::readWord(unsigned int address);
	unsigned int	ix86::readLong(unsigned int address);
	void			ix86::writeByte(unsigned int address, unsigned char value);
	void			ix86::writeWord(unsigned int address, unsigned short value);
	void			ix86::writeLong(unsigned int address, unsigned int value);
	unsigned int	getOperandValue(unsigned char Operand, unsigned char Size);
	void			setOperandValue(unsigned char Operand, unsigned char Size, unsigned int Value);
	bool			ix86::evaluateConditionCode(unsigned char ConditionCode);
	unsigned int	ix86::signExtend(unsigned short value);
	void			ix86::dumpTubeMemory(void);
	void			ix86::ADDhandler(void);
	void			ix86::ADDAhandler(void);
	void			ix86::ADDXhandler(void);
	void			ix86::ASxLSx_handler(void);
	void			ix86::ROxROXx_handler(void);
};
