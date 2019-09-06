/* Generic RAM module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef RAM_HEADER
#define RAM_HEADER



/****** Class definition *********************************************************/
class rammemory {
public:
	/****** Constructor / Deconstructor ******************************************/
					rammemory::rammemory(unsigned long size);
					rammemory::~rammemory(void);

	bool			DEBUG;

	/****** Signals and ports ****************************************************/
	
	/****** Public functions *****************************************************/
	void			rammemory::Reset(void);

	/****** Public variables *****************************************************/
	unsigned char	*memory;

private:
	unsigned long	size;
};
#endif
