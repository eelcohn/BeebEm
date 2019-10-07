/* Generic ROM module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

#ifndef ROM_HEADER
#define ROM_HEADER



/****** Class definition *********************************************************/
class rommemory {
public:
	/****** Constructor / Deconstructor ******************************************/
					rommemory::rommemory(const char *bios_file);
					rommemory::~rommemory(void);

	bool			DEBUG;

	/****** Signals and ports ****************************************************/
	
	/****** Public functions *****************************************************/
	void			rommemory::Reset(void);

	/****** Public variables *****************************************************/
	unsigned char	*memory;
};
#endif
