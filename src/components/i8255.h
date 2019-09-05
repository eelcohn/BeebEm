/* Intel 8255 PPI module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef I8255_HEADER
#define I8255_HEADER



/****** Class definition *********************************************************/
class i8255 {
public:
	/****** Constructor / Deconstructor ******************************************/
	i8255::i8255(void);
	i8255::~i8255(void);

	bool			DEBUG;

	/****** Signals and ports ****************************************************/
	unsigned char	pa;
	unsigned char	pb;
	unsigned char	pc;
	bool			reset;

	/****** Public functions *****************************************************/
	void			i8255::Reset(void);
	void			i8255::Exec(int Cycles);
	unsigned char	i8255::ReadRegister(unsigned char Register);
	void			i8255::WriteRegister(unsigned char Register, unsigned char Value);
	void			i8255::setPA(unsigned char value);
	void			i8255::setPB(unsigned char value);
	void			i8255::setPC(unsigned char value);

private:
	/****** Registers ************************************************************/
	unsigned char	port_a;
	unsigned char	port_b;
	unsigned char	port_c;
	unsigned char	control;

	/****** Private functions ****************************************************/
};
#endif
