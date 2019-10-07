/* 74LS374 module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef LS374_HEADER
#define LS374_HEADER



/****** Class definition *********************************************************/
class ls374 {
public:
	/****** Constructor / Deconstructor ******************************************/
	ls374::ls374(void);
	ls374::~ls374(void);

	bool			DEBUG;

	/****** Signals and ports ****************************************************/
	
	/****** Public functions *****************************************************/
	void			ls374::Reset(void);
	void			ls374::Exec(int Cycles);
	unsigned char	ls374::ReadRegister(void);
	void			ls374::WriteRegister(unsigned char Value);

private:
	/****** Registers ************************************************************/
	unsigned char	reg;

	/****** Private functions ****************************************************/
};
#endif
