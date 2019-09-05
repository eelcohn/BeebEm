/* Motorola MC6821 PIA (Pehiperal Interface Adapter) module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef MC6821_HEADER
#define MC6821_HEADER



/****** Class definition *********************************************************/
class	mc6821 {
public:
	/****** Constructor / Deconstructor ******************************************/
	mc6821::mc6821(void);
	mc6821::~mc6821(void);

	bool			DEBUG;

	/****** Signals and ports ****************************************************/
	unsigned char	pa;
	unsigned char	pb;
	bool			ca1;
	bool			ca2;
	bool			cb1;
	bool			cb2;
	bool			irqa;
	bool			irqb;

	/****** Public functions *****************************************************/
	void			mc6821::Reset(void);
	void			mc6821::Exec(int Cycles);
	unsigned char	mc6821::ReadRegister(unsigned int Register);
	void			mc6821::WriteRegister(unsigned int Register, unsigned char Value);
	void			mc6821::setPA(unsigned char Value);
	void			mc6821::setPB(unsigned char Value);
	void			mc6821::setCA1(bool value);
	void			mc6821::setCA2(bool value);
	void			mc6821::setCB1(bool value);
	void			mc6821::setCB2(bool value);

private:
	/****** Registers ************************************************************/
	unsigned char	data_a;
	unsigned char	data_b;
	unsigned char	ddra;
	unsigned char	ddrb;
	unsigned char	ctrl_a;
	unsigned char	ctrl_b;

	/****** Private functions ****************************************************/
	void			mc6821::updateIRQ(void);
};
#endif