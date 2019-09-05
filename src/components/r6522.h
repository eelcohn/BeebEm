/* Rockwell 6522 VIA module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef R6522_HEADER
#define R6522_HEADER



/****** Class definition *********************************************************/
class r6522 {
public:
	/****** Constructor / Deconstructor ******************************************/
	r6522::r6522(void);
	r6522::~r6522(void);

	bool			DEBUG;

	/****** Signals and ports ****************************************************/
	unsigned char	pa;
	unsigned char	pb;
	bool			ca1;
	bool			ca2;
	bool			cb1;
	bool			cb2;
	bool			res;
	bool			irq;

	/****** Public functions *****************************************************/
	void			r6522::Reset(void);
	void			r6522::Exec(int Cycles);
	unsigned char	r6522::ReadRegister(unsigned char Register);
	void			r6522::WriteRegister(unsigned char Register, unsigned char Value);
	void			r6522::setPA(unsigned char value);
	void			r6522::setPB(unsigned char value);
	void			r6522::setCA1(bool value);
	void			r6522::setCA2(bool value);
	void			r6522::setCB1(bool value);
	void			r6522::setCB2(bool value);

private:
	/****** Registers ************************************************************/
	unsigned char	orb;
	unsigned char	irb;
	unsigned char	ora;
	unsigned char	ira;
	unsigned char	ddrb;
	unsigned char	ddra;
	unsigned short	t1c;
	unsigned short	t1l;
	unsigned char	t2ll;
	unsigned short	t2c;
	unsigned char	sr;
	unsigned char	acr;
	unsigned char	pcr;
	unsigned char	ifr;
	unsigned char	ier;

	/****** Private functions ****************************************************/
	void r6522::updateInterrupt(void);
};
#endif
