/* Western Digital wd177x/wd179x/wd279x FDC (Floppy Disc Controller) object module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef WD177X_HEADER
#define WD177X_HEADER

enum DFDCTYPES	{WD1770=1, WD1771, WD1772, WD1773, WD1791, WD1793, WD1795, WD1797, WD2791, WD2793, WD2795, WD2797};

const unsigned char StepRate[12][4] = {{6,12,20,30},	// WD1770 step rate in ms
									{6,6,10,20},	// WD1771 step rate in ms
									{2,3,5,6},		// WD1772 step rate in ms
									{6,12,20,30},	// WD1773 step rate in ms
									{3,6,10,15},	// WD1791 @ 2MHz (step rates double @ 1MHz)
									{3,6,10,15},	// WD1793 @ 2MHz (step rates double @ 1MHz)
									{3,6,10,15},	// WD1795 @ 2MHz (step rates double @ 1MHz)
									{3,6,10,15},	// WD1797 @ 2MHz (step rates double @ 1MHz)
									{3,6,10,15},	// WD2791 (used in Opus DDOS)
									{0,0,0,0},		// WD2793
									{0,0,0,0},		// WD2795
									{3,6,10,15}};	// WD2797 @ 2MHz (step rates double @ 1MHz) (used in Cumana 68000 co-pro)



/****** Class definition *********************************************************/
class wd177x {
public:
	/****** Constructor / Deconstructor ******************************************/
	wd177x::wd177x(int FDCType);
	wd177x::~wd177x(void);

	bool			DEBUG;

	/****** Signals and ports ****************************************************/
							// Computer interface signals
	bool			intrq;	// Interrupt Request (output)
	bool			drq;	// Data Request (output)
	bool			dden;	// Double Density Enable (input)
	bool			enp;	// Enable write precompensation (input)
	bool			mr;		// Master Reset (input)
							// Floppy disc drive interface signals
	bool			index;	// Index pulse (input)
	bool			tr00;	// Track 00 (input)
	bool			wp;		// Write Protected (input)
	bool			step;	// Step 0=out 1=in (output)
	bool			dir;	// Direction (output)
	bool			load;	// Load head/Motor On (output)

	/****** Public functions *****************************************************/
	void			wd177x::Reset(void);
	void			wd177x::Exec(int Cycles);
	unsigned char	wd177x::ReadRegister(unsigned char Register);
	void			wd177x::WriteRegister(unsigned char Register, unsigned char Value);

private:
	/****** Registers ************************************************************/
	unsigned char	command;
	unsigned char	status;
	unsigned char	track;
	unsigned char	sector;
	unsigned char	data;

	/****** Internal variables ***************************************************/
	bool			ImmediateInterrupt;		// An immediate interrupt command has been issued
	unsigned char	SpinUpSequenceCounter;
	unsigned char	SpinDownCounter;		// Counts back from 10 to 0
	unsigned char	commandPhase;			// Phase of the current command being processed

};
#endif
