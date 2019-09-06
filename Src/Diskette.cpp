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

#include "Diskette.h"

#include "beebemrc.h"				// Included for IDM_WPDISC0, IDM_WPDISC1
#include "beebsound.h"				// Included for PlaySoundSample(), SAMPLE_DRIVE_MOTOR etc.
#include "main.h"					// Included for mainWin->

 HMENU hMenu = (HMENU) SendMessage((HWND)0,MN_GETHMENU,0,0); // Don't know if this will work....

/*--------------------------------------------------------------------------*/
/* Initialise our diskette structures                                       */
void Diskette::InitDiscStore(void) {
	unsigned int dsc, sid, trk;

	for(dsc=0; dsc<2; dsc++) {
		disc[dsc].writeprotected=TRUE;
		for(sid=0; sid<2; sid++) {
			strcpy(disc[dsc].side[sid].filename, "");
			disc[dsc].side[sid].imagetype=NULL;
			for(trk=0; trk<80; trk++) {
				disc[dsc].side[sid].track[trk].format	= 0;
				disc[dsc].side[sid].track[trk].gap1		= NULL;
				disc[dsc].side[sid].track[trk].gap3		= NULL;
			}
		}
	}
}; /* Diskette::InitDiscStore */


/*--------------------------------------------------------------------------*/
/* Load a diskette image in a drive                                         */
void Diskette::LoadDiskette(char *filename, unsigned char drive, unsigned char head) {
	int trk, sec, sid, sectorsize, totalsectors;
	unsigned char discformat;
	SectorType *SecPtr;

	if (disc[drive].side[head].fp != NULL)
		fclose(disc[drive].side[head].fp);								// Close previous opened discs, if any

	disc[drive].side[head].fp=fopen(filename,"rb");

	if (!disc[drive].side[head].fp) {
#ifdef WIN32
		char errstr[200];
		sprintf(errstr, "Could not open disc file:\n  %s", filename);
		MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
#else
		cerr << "Could not open disc file " << filename << "\n";
#endif
		return;
	};

	mainWin->SetImageName(filename,drive,0);							// Should move the code from beebwinio.cpp to this file someday
	
	strcpy(disc[drive].side[head].filename, filename);
	fseek(disc[drive].side[head].fp, 0L, SEEK_SET);

	char *ext = strrchr(filename, '.');
	for ( ; *ext; ++ext) *ext = tolower(*ext);

//	for(int i = 0; ext[i]; i++)
//		ext[i] = tolower(ext[i]);

	switch (ext) {
		case 'ssd' :
			Diskette::FormatDiscStore(drive, head);

			switch (Diskette::GetDFSCatalogueSize(disc[drive].side[head].fp))
				{
				case 0x500 : // Double density 80 track disc
				case 0x280 : // Double density 40 track disc
					sectorsize		= 256; 
					discformat		= 2;	// MFM encoding
					totalsectors	= 16;
					break;
				case 0x320 : // Single density 80 track disc
				case 0x190 : // Single density 40 track disc
					sectorsize		= 256; 
					discformat		= 1;	// FM encoding
					totalsectors	= 10;
					break;
				default:
#ifdef WIN32
					MessageBox(GETHWND, "Could not determine disc size\n\nThe file is loaded as a single sided disc image\nCheck disc before writing to it.\n", WindowTitle, MB_OK|MB_ICONERROR);
#else
					cerr << "Could not determine disc size\n\nThe file is loaded as a single sided disc image\nCheck disc before writing to it.\n";
#endif
					sectorsize		= 256; 
					discformat		= 1;	// FM encoding
					totalsectors	= 10;
					break;
			}
			break;

			disc[drive].side[head].imagetype=1;
			for(trk=0; trk<80; trk++) {
				disc[drive].side[head].track[trk].format	= discformat;
				disc[drive].side[head].track[trk].gap1		= NULL;			// Not used in .SSD disc images
				disc[drive].side[head].track[trk].gap3		= NULL;			// Not used in .SSD disc images
				disc[drive].side[head].track[trk].totalsectors = totalsectors;
				SecPtr=disc[drive].side[head].track[trk].sector=(SectorType*)calloc(totalsectors ,sizeof(SectorType));
				for(sec=0; sec<totalsectors; sec++) {
					disc[drive].side[head].track[trk].sector[sec].sectorid		= NULL;	// Not used in .SSD disc images
					disc[drive].side[head].track[trk].sector[sec].sectoridcrc	= NULL;	// Not used in .SSD disc images
					disc[drive].side[head].track[trk].sector[sec].sectorlength	= sectorsize;
					disc[drive].side[head].track[trk].sector[sec].dam			= NULL;	// Not used in .SSD disc images
					disc[drive].side[head].track[trk].sector[sec].data			= ((trk*totalsectors*sectorsize)+(sec*sectorsize));
					disc[drive].side[head].track[trk].sector[sec].datacrc		= NULL;	// Not used in .SSD disc images
				}; /* Sector */
			}; /* Track */
			break;

		case 'dsd' :
			Diskette::FormatDiscStore(drive, 0);
			Diskette::FormatDiscStore(drive, 1);
			break;
		case 'adf' :
			break;
		case 'adl' :
			break;
		case 'imd' :
			break;
		case 'img' :
			break;
		case 'dos' :
			break;
		case 'raw' :
			break;
		case 'fsd' :
			break;
		default :
			fclose(disc[drive].side[head].fp);

#ifdef WIN32
			char errstr[200];
			sprintf(errstr, "Image type %s not supported\n", ext);
			MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
#else
			cerr << "Image type " << ext << " not supported\n";
#endif
			break;
	}
	if (drive==0)
		EnableMenuItem(hMenu, IDM_WPDISC0, MF_ENABLED );					// Enable the eject disc option
	else
		EnableMenuItem(hMenu, IDM_WPDISC1, MF_ENABLED );					// Enable the eject disc option
	if (DiscDriveSoundEnabled)
		PlaySoundSample(SAMPLE_LOAD_DISC, TRUE);
} /* Diskette::LoadDiskette */


/*--------------------------------------------------------------------------*/
/* Return the total number of sectors in the DFS catalogue                  */
int Diskette::GetDFSCatalogueSize(FILE *fp) {
	unsigned short TotalSectors;
	long int HeadStore;

	if (!fseek(fp, 0x106, SEEK_SET))
		return -1;
	TotalSectors=(fgetc(fp) & 7) << 8;
	TotalSectors|=fgetc(fp);

	fseek(fp,HeadStore,SEEK_SET);
	return TotalSectors;
}

/*--------------------------------------------------------------------------*/
/* Return the total number of sectors in the ADFS catalogue                 */
int Diskette::GetADFSCatalogueSize(FILE *fp) {
	unsigned short TotalSectors;
	long int HeadStore;

	HeadStore=ftell(fp);

	if (!fseek(fp, 0xfc, SEEK_SET))
		return -1;

	TotalSectors=fgetc(fp);
	TotalSectors|=fgetc(fp)<<8;
	TotalSectors|=fgetc(fp)<<16;

	fseek(fp,HeadStore,SEEK_SET);
	return TotalSectors;
}

/*--------------------------------------------------------------------------*/
/* Format the internal disk store                                           */
void Diskette::FormatDiscStore(unsigned char drive, unsigned char head) {
	unsigned int i, dsc, sid, trk;

/*	for(trk=0; trk<80; trk++) {
		for (i=0;i<60;i++)
			disc[dsc].side[head].track[trk].rawdata[i]=0x4e;			// Gap 1
		for (i=0;i<12;i++)
			disc[drive].side[head].track[trk].rawdata[i+60]=0x00;		// Gap 3
		for (i=0;i<3;i++)
			disc[drive].side[head].track[trk].rawdata[i+72]=0xA1;		// Start of ID field marker
		disc[drive].side[head].track[trk].rawdata[75]=0xFE;			// ID address mark
		disc[drive].side[head].track[trk].rawdata[76]=0x00;			// Track
		disc[drive].side[head].track[trk].rawdata[77]=0x00;			// Side
		disc[drive].side[head].track[trk].rawdata[78]=0x00;			// Sector
		disc[drive].side[head].track[trk].rawdata[79]=0x00;			// Sector length code
		disc[drive].side[head].track[trk].rawdata[80]=0x00;			// CRC
		disc[drive].side[head].track[trk].rawdata[81]=0x00;			// CRC
		for (i=0;i<22;i++)
			disc[dsc].side[head].track[trk].rawdata[i+82]=0x4e;		// Gap 2
		for (i=0;i<12;i++)
			disc[drive].side[head].track[trk].rawdata[i+104]=0x00;		// Gap 2
		for (i=0;i<3;i++)
			disc[drive].side[head].track[trk].rawdata[i+116]=0xA1;		// Start of data field marker
		disc[drive].side[head].track[trk].rawdata[119]=0xFB;			// Data address mark (0xFB for normal data, 0xF8 for deleted data)
		for (i=0;i<256;i++)
			disc[drive].side[head].track[trk].rawdata[i+120]=0x00;		// Data
		disc[drive].side[head].track[trk].rawdata[376]=0x00;			// CRC
		disc[drive].side[head].track[trk].rawdata[377]=0x00;			// CRC
		for (i=0;i<24;i++)
			disc[dsc].side[head].track[trk].rawdata[378+i]=0x4e;		// Gap 4
		for (i=0;i<668;i++)
			disc[dsc].side[head].track[trk].rawdata[402+i]=0x4e;		// Gap 5
	}
	*/
}; /* Diskette::FormatDiscStore */


/*--------------------------------------------------------------------------*/
/* Eject diskette from drive                                                */
void Diskette::EjectDisk(unsigned char drive, unsigned char head) {
	fclose(disc[drive].side[head].fp);
	disc[drive].side[head].fp=NULL;
	strcpy(disc[drive].side[head].filename, "");
	if (drive==0)
		EnableMenuItem(hMenu, IDM_WPDISC0, MF_GRAYED );						// Gray out the eject disc option
	else
		EnableMenuItem(hMenu, IDM_WPDISC1, MF_GRAYED );						// Gray out the eject disc option
	if (DiscDriveSoundEnabled)
		PlaySoundSample(SAMPLE_EJECT_DISC, TRUE);
} /* Diskette::EjectDisk */
