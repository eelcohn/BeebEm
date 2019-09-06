/* Acorn ULA9C018 TUBE ULA object module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef ULA9C018_HEADER
#define ULA9C018_HEADER



class ula9c018 {
public:
	/****** Constructor / Deconstructor ******************************************/
	ula9c018(void);
	~ula9c018(void);

	/****** Public functions *****************************************************/
	void			ula9c018::Reset(void);
	void			ula9c018::Exec(int Cycles);
	unsigned char	ula9c018::ReadHostRegister(unsigned char Register);
	unsigned char	ula9c018::ReadParasiteRegister(unsigned char Register);
	void			ula9c018::WriteHostRegister(unsigned char Register, unsigned char Value);
	void			ula9c018::WriteParasiteRegister(unsigned char Register, unsigned char Value);

	bool			DEBUG;

	/****** Signals and ports ****************************************************/
	bool			host_irq;		// Output
	bool			host_rst;		// Input
	bool			parasite_int;	// Output
	bool			parasite_irq;	// Output
	bool			parasite_rst;	// Output
	bool			drq;			// Output
	bool			dack;			// Input

private:
	/****** Registers ************************************************************/
	unsigned char	control;		// Control register (S, T, P, V, M, J, I and Q bits)
	unsigned char	hp_r1status;
	unsigned char	hp_r2status;
	unsigned char	hp_r3status;
	unsigned char	hp_r4status;
	unsigned char	hp_r1data;
	unsigned char	hp_r2data;
	unsigned char	hp_r3data[2];	// 2-byte FIFO
	unsigned char	hp_r4data;
	unsigned char	ph_r1status;
	unsigned char	ph_r2status;
	unsigned char	ph_r3status;
	unsigned char	ph_r4status;
	unsigned char	ph_r1data[24];	// 24-byte FIFO
	unsigned char	ph_r2data;
	unsigned char	ph_r3data[2];	// 2-byte FIFO
	unsigned char	ph_r4data;

	/****** Internal variables ***************************************************/
	unsigned char	hp_r3data_ptr;
	unsigned char	ph_r1data_ptr;
	unsigned char	ph_r3data_ptr;

	/****** Private functions ****************************************************/
	void			ula9c018::clearRegisters(void);
};
#endif
