/* Motorola 68k CPU object module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef TUBE_MC68K_HEADER
#define TUBE_MC68K_HEADER

static long cyclecount;



enum ARCHITECTURES	{CISCOS = 1, CASPER, CUMANA, TORCH};
enum CPUTYPES	{MC68008L=1, MC68008P, MC68000, MC68010, MC68020, MC68030, MC68040, MC68060, MC68070, MC68300, UNKNOWNCPU=255};
enum FPUTYPES	{NOFPU=0, MC68881, MC68882, INTERNALFPU, UNKNOWNFPU=255};
enum MMUTYPES	{NOMMU=0, MC68841, MC68851, INTERNALMMU, UNKNOWNMMU=255};
enum EXCEPTIONS	{RESET, RESET_PC, BUS_ERROR, ADDRESS_ERROR, ILLEGAL_INSTRUCTION, DIVIDE_BY_ZERO, CHK_INSTRUCTION, TRAPV_INSTRUCTION, PRIVILEGE_VIOLATION, TRACE, LINE1010_EMULATOR, LINE1111_EMULATOR, FORMAT_ERROR=14, UNINITIALIZED_INTERRUPT_VECTOR, SPURIOUS_INTERRUPT=24, INTERRUPT1, INTERRUPT2, INTERRUPT3, INTERRUPT4, INTERRUPT5, INTERRUPT6, INTERRUPT7, TRAP0, TRAP1, TRAP2, TRAP3, TRAP4, TRAP5, TRAP6, TRAP7, TRAP8, TRAP9, TRAP10, TRAP11, TRAP12, TRAP13, TRAP14, TRAP15};

/****** CPU stuff ************************************************************/

typedef union {				// SR: Status Register / Condition Code Register
	unsigned short reg;
	struct {
		bool c:1;			// Carry
		bool v:1;			// oVerflow
		bool z:1;			// Zero
		bool n:1;			// Negative
		bool x:1;			// eXtend
		bool undefined1:1;
		bool undefined2:1;
		bool undefined3:1;
		bool i0:1;			// Interrupt bit 0
		bool i1:1;			// Interrupt bit 1
		bool i2:1;			// Interrupt bit 2
		bool undefined4:1;
		bool m:1;			// Master/Interrupt mode
		bool s:1;			// Supervisor/User
		bool t0:1;			// Trace bit 0
		bool t1:1;			// Trace bit 1
	} flag;
} SR;

typedef struct {			// All CPU registers
	unsigned char	type;
	unsigned short	instruction_reg;
	unsigned int	pc;
	unsigned int	d[8];
	unsigned int	a[7];
	unsigned int	ssp;
	unsigned int	usp;
	SR				sr;
	unsigned int	vbr;				// Vector Base Register ( > 68010)
	unsigned int	cacr;				// Cache Control Regiter ( > 68020)
	unsigned int	caar;				// Cache Address Register ( > 68020)
	unsigned int	isp, msp;			// Interrupt Stack Pointer and Master Stack Pointer ( > 68020)
	unsigned int	sfc, dfc;			// Source Function Code and Destionation Source Code
	unsigned int	ac0, ac1;			// Access Control Registers 0 and 1 ( > 68030)
	unsigned short	acusr;				// ACU Status Register ( > 68030)
	unsigned int	urp, srp;			// User Root Pointer Regiser & Supervisor Root Pointer Register
	unsigned short	tc;					// Translation Control Register
	unsigned int	dtt0, dtt1;
	unsigned int	itt0, itt1;
	unsigned int	mmusr;
	bool			stop;
} CPU;

/****** FPU stuff ************************************************************/

typedef struct {			// Hack because MSVC doesn't support 80 bit floats anymore
	short	exponent;
	double	fraction;
} long_double;

typedef union {				// FPCR: Floating point control register
	unsigned int reg;
	struct {
		bool undefined0:1;
		bool undefined1:1;
		bool undefined2:1;
		bool undefined3:1;
		bool rnd1:1;		// Rounding mode
		bool rnd2:1;		// Rounding mode
		bool prec1:1;		// Rounding precision
		bool prec2:1;		// Rounding precision
		bool inex1:1;		// Inexact decimal input
		bool inex2:1;		// Inexact operation
		bool dz:1;			// Divide by zero
		bool unfl:1;		// Underflow
		bool ovfl:1;		// Overflow
		bool operr:1;		// Operand error
		bool snan:1;		// Signalling not a number
		bool bsun:1;		// Branch/set on unordered
		short undefined:16;
	} flag;
} FPCR;

typedef union {				// FPSR: Floating Point Status Register
	unsigned int reg;
	struct {
		bool undefied0:1;
		bool undefied1:1;
		bool undefied2:1;
		bool inex:1;		// Inexact
		bool dz:1;			// Divide by zero
		bool unfl:1;		// Underflow
		bool ovfl:1;		// Overflow
		bool idp:1;			// Invalid operation

		char something:8;

		char quotient:8;

		bool nan:1;			// Not a number
		bool i:1;			// Infinity
		bool z:1;			// Zero
		bool n:1;			// Negative
		bool undefied28:1;
		bool undefied29:1;
		bool undefied30:1;
		bool undefied31:1;
	} flag;
} FPSR;

typedef struct {						//  All FPU registers ( > 68040 or 68881/68882)
	unsigned char	type;
//	long_double		fp[8];				// Floating Point Data Registers
	FPCR			fpcr;				// FP Control Register
	unsigned int	fpsr;				// FP Status Register 
	unsigned int	fpiar;				// FP Instruction Address Register
} FPU;

/****** MMU stuff ************************************************************/

typedef struct {
	unsigned char	type;
} MMU;



/****** Class definition *********************************************************/

class	mc68k {
public:
	/****** Constructor / Deconstructor ******************************************/
	mc68k(int CPUType, int FPUType, int MMUType);
	~mc68k(void);
	int				Architecture;

	bool			DEBUG;

	/****** Processors ***********************************************************/
	CPU cpu;
	FPU fpu;
	MMU mmu;

	/****** Signals and ports ****************************************************/
	unsigned char	fc;					// FC0-FC2: CPU function code signals

	/****** Flags for signalling exceptions **************************************/
	signed short	pendingException;	// Exception 0-255, or -1 for normal operation
	unsigned char	interruptLevel;		// Represents the state of the IPL[0..2] interrupt lines on the CPU
	signed char		currentInterrupt;	// Represents the current interrupt being handled by the CPU (can this be replaced by FC0..FC2???)

	/****** Public functions *****************************************************/
	void			Reset(void);
	void			Exec(int Cycles);

private:
	/****** Private functions ****************************************************/
	unsigned char	readByte(unsigned int address);
	unsigned short	readWord(unsigned int address);
	unsigned int	readLong(unsigned int address);
	void			writeByte(unsigned int address, unsigned char value);
	void			writeWord(unsigned int address, unsigned short value);
	void			writeLong(unsigned int address, unsigned int value);
	unsigned int	getOperandValue(unsigned char Operand, unsigned char Size);
	void			setOperandValue(unsigned char Operand, unsigned char Size, unsigned int Value);
	void			rewindPC(unsigned char Operand);
	bool			evaluateConditionCode(unsigned char ConditionCode);
	unsigned int	signExtend(unsigned short value);
	void			dumpMemory(void);
	void			dumpRegisters(void);
	void			ABCDhandler(void);
	void			ADDhandler(void);
	void			ADDAhandler(void);
	void			ADDIhandler(void);
	void			ADDQhandler(void);
	void			ADDXhandler(void);
	void			ANDhandler(void);
	void			ANDIhandler(void);
	void			ASxLSx_handler(void);
	void			BCHGhandler(void);
	void			BCLRhandler(void);
	void			BSEThandler(void);
	void			BTSThandler(void);
	void			CHKhandler(void);
	void			CMPhandler(void);
	void			CMPAhandler(void);
	void			CMPISUBIhandler(void);
	void			CMPMhandler(void);
	void			DIVShandler(void);
	void			DIVUhandler(void);
	void			EORhandler(void);
	void			EORIhandler(void);
	void			EXGhandler(void);
	void			EXThandler(void);
	void			LEAhandler(void);
	void			LINKhandler(void);
	void			MOVEhandler(void);
	void			MOVEMhandler(void);
	void			MOVEPhandler(void);
	void			MULShandler(void);
	void			MULUhandler(void);
	void			NBCDhandler(void);
	void			NEGhandler(void);
	void			NOThandler(void);
	void			ORhandler(void);
	void			ORIhandler(void);
	void			ROxROXx_handler(void);
	void			SBCDhandler(void);
	void			SUBhandler(void);
	void			SUBQhandler(void);
	void			SUBXhandler(void);
	void			SWAPhandler(void);
	void			TAShandler(void);
	void			TSThandler(void);
	void			UNLINKhandler(void);

	/****** Internal variables ***************************************************/
	unsigned int	INTERNAL_ADDRESS_MASK;
	unsigned int	EXTERNAL_ADDRESS_MASK;
};
#endif
