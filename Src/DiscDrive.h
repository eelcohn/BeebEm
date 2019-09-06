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

#ifndef SAMPLE_DISCDRIVE_OVERSTEP			// Should be in beebsound.h library, just here for testing purposes
#define SAMPLE_DISCDRIVE_OVERSTEP	SAMPLE_HEAD_STEP
#endif



struct Discdrive {
												// Shugart Interface related stuff
	bool			track00;					// ==TRUE: Head is at track 00, ==FALSE: Head is not at track 00
	bool			writeprotect;				// ==TRUE: Disc is write protected, ==FALSE: Disc is not write protected
	bool			index;						// ==TRUE: Index hole pulse
	bool			doorclosed;					// ==FALSE: drive door not closed, ==TRUE: drive door is closed
	bool			headloaded;					// ==FALSE: head is not loaded, ==TRUE: head is loaded
	unsigned char	headpos;					// Current head position

												// Drive properties
	bool			available;					// ==TRUE: Drive is installed and available, ==FALSE: Drive is not available/installed
	unsigned char	tracks;						// ==40: 40 tracks unit, ==80: 80 tracks unit
};


extern struct Discdrive drive[2];


class DiscDrive {
public:
	static void DiscDrive::InitDriveStore(void);
	static void DiscDrive::LoadHead(unsigned char drive);
	static void DiscDrive::UnloadHead(unsigned char drive);
	static void DiscDrive::StepOut(unsigned char drv);
	static void DiscDrive::StepIn(unsigned char drv);
	static bool DiscDrive::IsWriteProtected(unsigned char drv);
	static bool DiscDrive::IsTrack00(unsigned char drv);
	static bool DiscDrive::IndexPulse(unsigned char drv);
};