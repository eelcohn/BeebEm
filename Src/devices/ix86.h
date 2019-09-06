/* Intel x86 TUBE module for the BeebEm emulator
  Written by Eelco Huininga 2016-2017
*/

extern int Enable_ix86;
extern int ix86Tube;								// Global variable indicating wether or not the Intel x86 Co-Pro is selected and active



enum ARCHITECTURES {ACORN186 = 1, ACORN286, TORCH_GRADUATE};
enum CPUTYPES	{I8088=1, I8086, NECV20, NECV30, I80188, I80186, I80286, I80386, UNKNOWNCPU=255};
enum FPUTYPES	{NOFPU=0, I8087, I80187, I80287, I80387, INTERNALFPU=254, UNKNOWNFPU=255};
enum MMUTYPES	{NOMMU=0, INTERNALMMU=254, UNKNOWNMMU=255};
enum REGISTERS	{AX, BX, CX, DX, SI, DI, BP, SP, CS, DS, ES, SS, IP};

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

typedef union {
	unsigned char	reg;
//	struct {
		bool w:1;
		bool s:1;
		bool bit2:1;
		bool bit3:1;
		bool bit4:1;
		bool bit5:1;
		bool bit6:1;
		bool bit7:1;
//	};
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
		bool in:1;			// Interrupt enable flag
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

typedef union {						// MSW (Machine Status Word) Register (286+) / CR0 (Control Register 0) (386+)
	unsigned int reg;
//	struct {
		bool pe:1;					// Protection Enable (286+)
		bool mp:1;					// Math Present (286+)
		bool em:1;					// Processor Extension Emulation (286+)
		bool ts:1;					// Task Switched (286+)
		bool et:1;					// Extension Type (286+)
		bool ne:1;					// Numeric Error (386+)
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
//	} msw;
} MSW;

typedef union {						// CR0 (Control Register 0) (386+)
	unsigned short msw;
//	struct {
		bool pe:1;					// Protection Enable (286+)
		bool mp:1;					// Math Present (286+)
		bool em:1;					// Processor Extension Emulation (286+)
		bool ts:1;					// Task Switched (286+)
		bool et:1;					// Extension Type (286+)
		bool ne:1;					// Numeric Error (386+)
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
//	} msw;
//	struct {
	unsigned short cr0;
		bool wp:1;					// Write Protect (386+)
		bool reserved17:1;
		bool am:1;					// Alignment Mask (386+)
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
		bool nw:1;					// Not-write through (386+)
		bool cd:1;					// Cache disable (386+)
		bool pg:1;					// Paging (386+)
//	} cr0;
} CR0;

struct GDT_STR {					// Structure for Global Descriptor Table
    unsigned short seg_length_0_15;
    unsigned short base0_15;
    unsigned char  base16_23;
    unsigned char  flags;
    unsigned char  seg_length_16_19:4;
    unsigned char  access:4;
    unsigned char  base24_31;
};

typedef struct {					// All CPU registers
	unsigned char		type;
	FLAGS				flags;
//	unsigned short		instruction_reg;
	INSTRUCTION_REG		instruction_reg;
	REG16				ax;			// Main registers
	REG16				bx;
	REG16				cx;
	REG16				dx;
	unsigned short		si;			// Index registers
	unsigned short		di;
	unsigned short		bp;
	unsigned short		sp;
	unsigned short		ip;			// Instruction register (Program counter)
	unsigned short		cs;			// Segment registers
	unsigned short		ds;
	unsigned short		es;
	unsigned short		ss;
	unsigned short		fs;			// (386+)
	unsigned short		gs;			// (386+)

	unsigned short		relocreg;	// Relocation register (186/188)
	unsigned short		umcs;		//  (186/188)
	unsigned short		mmcs;		//  (186/188)
	unsigned short		mpcs;		//  (186/188)
	unsigned short		lmcs;		//  (186/188)
	unsigned short		pacs;		//  (186/188)

	MSW					msw;		// Machine Status Word (286+)
	struct {
		unsigned short	base;		// Interrupt Descriptor Table Register base (286+)
		unsigned long	limit;		// Interrupt Descriptor Table Register limit (286+)
	} idtr;
	struct {
		unsigned short	base;		// Local Descriptor Table Register base (286+)
		unsigned long	limit;		// Local Descriptor Table Register limit (286+)
	} ldtr;
	struct {
		unsigned short	base;		// Global Descriptor Table Register base (286+)
		unsigned long	limit;		// Global Descriptor Table Register limit (286+)
	} gdtr;
	unsigned short		tr;			// Task Register (286+)
	unsigned long long	csd;		// CS Segment Descriptor
	unsigned long long	dsd;		// DS Segment Descriptor
	unsigned long long	esd;		// ES Segment Descriptor
	unsigned long long	ssd;		// SS Segment Descriptor
	unsigned long long	ldt;		// Local Descriptor Table (286+)
	unsigned long long	idt;		// Interrupt Descriptor Table (286+)
	unsigned long long	tss;		// Task State Segment (286+)

	unsigned long		cr0;		// Control Register 0 (386+)
	unsigned long		cr1;		// Control Register 1 (386+)
	unsigned long		cr2;		// Control Register 2 (386+)
	unsigned long		cr3;		// Control Register 3 (386+)

	unsigned long		dr0;		// Debug Register 0 (386+)
	unsigned long		dr1;		// Debug Register 1 (386+)
	unsigned long		dr2;		// Debug Register 2 (386+)
	unsigned long		dr3;		// Debug Register 3 (386+)
	unsigned long		dr4;		// Debug Register 4 (386+)
	unsigned long		dr5;		// Debug Register 5 (386+)
	unsigned long		dr6;		// Debug Register 6 (386+)
	unsigned long		dr7;		// Debug Register 7 (386+)

	bool				halt;
	bool				wait;
} CPU;

/****** FPU stuff ************************************************************/

typedef struct {					// Hack because MSVC doesn't support 80 bit floats anymore
	short	exponent;
	double	fraction;
} long_double;

typedef struct {					//  All FPU registers ( 8087 / 80187 / 80287 / 80387)
	unsigned char	type;
//	long_double		st[8];			// Floating Point Data Registers
} FPU;



static unsigned int		INTERNAL_ADDRESS_MASK;
static unsigned int		EXTERNAL_ADDRESS_MASK;



/****** Class definition *********************************************************/

class	ix86 {
public:
	/****** Constructor / Deconstructor ******************************************/
					ix86(int cpuType, int fpuType, unsigned int clockSpeed, unsigned char *readMemory, unsigned char *writeMemory, unsigned char *readIO, unsigned char *writeIO);
					~ix86(void);

	bool			DEBUG;

	/****** Processors ***********************************************************/
	CPU				cpu;
	FPU				fpu;

	/****** Signals and ports ****************************************************/
	bool			intr;			// Interrupt signal
	bool			lock;			// Lock signal
	bool			nmi;			// Non-maskable interrupt (NMI) signal

	/****** Hardware related variables *******************************************/
	unsigned char	*romMemory;
	unsigned char	*ramMemory;
	unsigned int	RAM_SIZE;
	unsigned int	ROM_SIZE;
	unsigned int	TUBE_ULA_ADDR;
	unsigned int	ROM_ADDR;
	bool			BOOTFLAG;
	unsigned char	biosFile[128];

	/****** Public functions *****************************************************/
	void			Reset(void);
	void			Exec(int Cycles);
	unsigned int	signExtend(unsigned short value);

private:
	/****** Various variables ****************************************************/
	unsigned int	clockSpeed;
	long cyclecount;


	/****** Flags for signalling interrupts **************************************/
	signed short	pendingInterrupt;	// Interrupt 0-255, or -1 for normal operation
	signed short	pendingInterruptBeforeTrace;	// Interrupt 0-255, or -1 for normal operation
	unsigned char	interrupt_nr;		// Represents the A7-A3 and S2-S0 lines during an interrupt cycle; these represent the interrupt vector
//	unsigned char	interruptLevel;		// Represents the state of the IPL[0..2] interrupt lines on the CPU
	signed char	currentInterrupt;		// Represents the current interrupt being handled by the CPU (can this be replaced by FC0..FC2???)

	/****** Private functions ****************************************************/
	unsigned char	*readMemory;
	unsigned char	*writeMemory;
	unsigned char	*readIO;
	unsigned char	*writeIO;
	unsigned int	getAddress(unsigned char Segment_reg, unsigned char Offset_reg);
	unsigned char	readMemByte(unsigned int address);
	unsigned short	readMemWord(unsigned int address);
	unsigned int	readMemLong(unsigned int address);
	void			writeMemByte(unsigned int address, unsigned char value);
	void			writeMemWord(unsigned int address, unsigned short value);
	void			writeMemLong(unsigned int address, unsigned int value);
	unsigned char	readIOByte(unsigned int address);
	unsigned short	readIOWord(unsigned int address);
	void			writeIOByte(unsigned int address, unsigned char value);
	void			writeIOWord(unsigned int address, unsigned short value);
	bool			evaluateConditionCode(unsigned char ConditionCode);
	unsigned int	getOperandValue(bool w, unsigned char mod, unsigned char rm);
	void			setOperandValue(bool w, unsigned char mod, unsigned char rm, unsigned short value);
	unsigned int	getRegisterValue (bool w, unsigned char reg);
	void			setRegisterValue (bool w, unsigned char reg, unsigned short value);
	void			dumpRegisters(void);
	void			dumpTubeMemory(void);
	void			setZeroFlag(unsigned int value);
	void			setSignFlag(bool w, unsigned int value);
	void			setParityFlag(bool w, unsigned int value);
	void			setOverflowFlag(bool w, unsigned int result, unsigned int value1, unsigned int value2);
	void			setAdjustFlag(unsigned int result, unsigned int value1, unsigned int value2);
	void			setCarryFlag(bool w, unsigned int value);
};
