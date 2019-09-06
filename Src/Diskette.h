/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
2016-Feb-10 Eelco Huininga - First release of FDC independant floppy disc module

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public 
License along with this program; if not, write to the Free 
Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA  02110-1301, USA.
****************************************************************/

#ifndef TRUE
#define TRUE		1
#endif

#ifndef FALSE
#define FALSE		0
#endif

#ifndef SAMPLE_LOAD_DISC					// Should be in beebsound.h library, just here for testing purposes
#define SAMPLE_LOAD_DISC 7
#endif

#ifndef SAMPLE_EJECT_DISC					// Should be in beebsound.h library, just here for testing purposes
#define SAMPLE_EJECT_DISC 8
#endif



#include <stdio.h>							// Included for FILE *
#include <stdlib.h>							// Included for _MAX_PATH



typedef struct {
	long int		sectorid;				// FILE *Pointer to start of sector id
	long int		sectoridcrc;			// FILE *Pointer to sector id crc, or ==NULL if disc image does not support sector id crc's
	unsigned short	sectorlength;			// Actual length of sector
	long int		dam;					// FILE *Pointer to Data Address Marker (&FB for normal data, &F8 for deleted data), or ==NULL  if disc image does not support raw disc data
	long int		data;					// FILE *Pointer to start of sector data
	long int		datacrc;				// FILE *Pointer to start of data crc, or ==NULL  if disc image does not support data crc's
} SectorType;

struct Disk {
	bool			writeprotected;			// =FALSE: not write protected, =TRUE: write protected
	struct Sides {
		char			filename[_MAX_PATH];	// Path to loaded disc image
		FILE			*fp;				// File pointer to loaded disc image
		unsigned int	imagetype;			// =0: unformatted, =1: raw, =2: ssd, =3:dsd, =4:adf etc etc, defined by some enum somewhere)
		struct Tracks {
			unsigned char	format;			// =0: Unformatted, 1=FM, =2: MFM - Although seldomly used, on real hardware you can format different tracks using different encoding
			long int		gap1;			// FILE *Pointer to start of gap1 data
			long int		gap3;			// Pointer to start of gap3 data
			unsigned char	totalsectors;	// Total no. of sectors in this track, shouldn't be nessecary and can be removed when disc1770.cpp cleanup is complete
			SectorType		*sector;		// Will be declared later using calloc();
		} track[80];						// Always 80 tracks. 40 track images have the deleted=true flag on all uneven tracks
	} side[2];
};

extern struct Disk disc[2];									// =0: Drive 0, =1: Drive 1



class Diskette {
public:
	static void Diskette::InitDiscStore(void);
	static void Diskette::LoadDiskette(char *filename, char unsigned drive, unsigned char head);
	static int Diskette::GetDFSCatalogueSize(FILE *fp);
	static int Diskette::GetADFSCatalogueSize(FILE *fp);
	static void Diskette::FormatDiscStore(unsigned char drive, unsigned char head);
	static void Diskette::EjectDisk(unsigned char drive, unsigned char head);
};