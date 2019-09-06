/* Intel 8271 FDC (Floppy Disc Controller) object module for the BeebEm emulator
  Written by Eelco Huininga 2017
*/

#ifndef I8271_HEADER
#define I8271_HEADER



/****** Class definition *********************************************************/
class i8271 {
public:
	/****** Constructor / Deconstructor ******************************************/
	i8271::i8271(void);
	i8271::~i8271(void);

	bool			DEBUG;

	/****** Signals and ports ****************************************************/
								// Computer interface signals
	bool			intr;		// Interrupt (output)
	bool			drq;		// Data Request (output)
	bool			dack;		// Data Acknowledge (input)
	bool			rst;		// Reset (input)
								// Floppy disc drive interface signals
	bool			ready0;		// Drive 0 ready (input)
	bool			ready1;		// Drive 1 ready (input)
	bool			trk0;		// Track 0 (input)
	bool			count;		//
	bool			index;		// Index pulse (input)
	bool			wrprotect;	// Write Protected (input)
	bool			fault;		// (output)
	bool			select0;	// (output)
	bool			select1;	// (output)
	bool			wrenable;	// (output)
	bool			loadhead;	// Load head (output)
	bool			seekstep;	// Step 0=out 1=in (output)
	bool			direction;	// Direction (output)
	bool			lowcurrent;	// (output)
	bool			faultreset;	// (output)

	/****** Public functions *****************************************************/
	void			i8271::Reset(void);
	void			i8271::Exec(int Cycles);
	unsigned char	i8271::ReadRegister(unsigned char Register);
	void			i8271::WriteRegister(unsigned char Register, unsigned char Value);

private:
	/****** Registers ************************************************************/
	unsigned char	command;
	unsigned char	parameter;
	unsigned char	result;
	unsigned char	status;
	unsigned char	reset;

	/****** Special registers ****************************************************/
	unsigned char	scanSectorNumber;
	unsigned char	scanMSBOfCount;
	unsigned char	scanLSBOfCount;
	unsigned char	surface0CurrentTrack;
	unsigned char	surface1CurrentTrack;
	unsigned char	modeRegister;
	unsigned char	driveControlOutputPort;
	unsigned char	driveControlInputPort;
	unsigned char	surface0BadTrack1;
	unsigned char	surface0BadTrack2;
	unsigned char	surface1BadTrack1;
	unsigned char	surface1BadTrack2;

	/****** Internal variables ***************************************************/

};
#endif
