/* 74LS174 module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef LS174_HEADER
#define LS174_HEADER



/****** Class definition *********************************************************/
class ls174 {
public:
	/****** Constructor / Deconstructor ******************************************/
	ls174::ls174(void);
	ls174::~ls174(void);

	bool			DEBUG;

	/****** Signals and ports ****************************************************/
	
	/****** Public functions *****************************************************/
	void			ls174::Reset(void);
	void			ls174::Exec(int Cycles);
	unsigned char	ls174::ReadRegister(void);
	void			ls174::WriteRegister(unsigned char Value);

private:
	/****** Registers ************************************************************/
	unsigned char	reg;

	/****** Private functions ****************************************************/
};
#endif
