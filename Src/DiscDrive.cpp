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

#include "DiscDrive.h"

#include "beebsound.h"				// Included for PlaySoundSample(), SAMPLE_DRIVE_MOTOR etc.
#include "beebwin.h"				// Included for LEDs.Disc0 and LEDS.Disc1

/*--------------------------------------------------------------------------*/
/* Initialise our disc drive structures                                     */
void DiscDrive::InitDriveStore(void) {
	unsigned int drv;

	for(drv=0; drv<2; drv++) {
		drive[drv].track00		= TRUE;
		drive[drv].writeprotect	= TRUE;
		drive[drv].index		= TRUE;
		drive[drv].doorclosed	= FALSE;
		drive[drv].headloaded	= FALSE;
		drive[drv].headpos		= 0;
		drive[drv].available	= TRUE;			// Dual drive setup, so both drive are available
		drive[drv].tracks		= 80;			// 80 track drive
	}
} /* DiscDrive::InitDriveStore */


/*--------------------------------------------------------------------------*/
/* Load head                                                                */
void DiscDrive::LoadHead(unsigned char drv) {
	if (DiscDriveSoundEnabled)
		PlaySoundSample(SAMPLE_DRIVE_MOTOR, true);                              // Spin up the motor even if there's no disc in the drive
	if (drive[drv].doorclosed==TRUE) {
		if (drive[drv].headloaded==FALSE) {
			drive[drv].headloaded=TRUE;
			if (DiscDriveSoundEnabled)
				PlaySoundSample(SAMPLE_HEAD_LOAD, true);					// Only load the head if a disc is present in the drive
		}
	}
	if (drv==0)
		LEDs.Disc0=TRUE;
	if (drv==1)
		LEDs.Disc1=TRUE;
} /* Diskette::LoadHead */


/*--------------------------------------------------------------------------*/
/* Unload head                                                              */
void DiscDrive::UnloadHead(unsigned char drv) {
	if (DiscDriveSoundEnabled)
		StopSoundSample(SAMPLE_DRIVE_MOTOR);
	if (drive[drv].headloaded==TRUE) {
		drive[drv].headloaded=FALSE;
		if (DiscDriveSoundEnabled)
			PlaySoundSample(SAMPLE_HEAD_UNLOAD, true);
	}
	if (drv==0)
		LEDs.Disc0=FALSE;
	if (drv==1)
		LEDs.Disc1=FALSE;

} /* Diskette::UnloadHead */


/*--------------------------------------------------------------------------*/
/* Move head inwards (towards track 80) with one step                       */
void DiscDrive::StepIn(unsigned char drv) {
	DiscDrive::LoadHead(drv);
	if (drive[drv].headpos >= 79)
		if (DiscDriveSoundEnabled)
			PlaySoundSample(SAMPLE_DISCDRIVE_OVERSTEP, true);		// Make crunching sound because the drive tries to step past track 80
	else {
		drive[drv].headpos++;
		if(drive[drv].tracks==40)
			drive[drv].headpos++;									// Double step for 40 track drives
		if (DiscDriveSoundEnabled)
			PlaySoundSample(SAMPLE_HEAD_SEEK, true);				// Make stepping outward sound
		drive[drv].track00=FALSE;
	}
} /* DiscDrive::StepIn */


/*--------------------------------------------------------------------------*/
/* Move head outwards (towards track 00) with one step                      */
void DiscDrive::StepOut(unsigned char drv) {
	DiscDrive::LoadHead(drv);
	if (drive[drv].headpos == 0)
		if (DiscDriveSoundEnabled)
			PlaySoundSample(SAMPLE_DISCDRIVE_OVERSTEP, true);		// Make crunching sound because the drive tries to step past track 0
	else {
		drive[drv].headpos--;
		if(drive[drv].tracks==40)
			drive[drv].headpos--;									// Double step for 40 track drives
		if (DiscDriveSoundEnabled)
			PlaySoundSample(SAMPLE_HEAD_SEEK, true);				// Make stepping inward sound
		if (drive[drv].headpos == 0)
			drive[drv].track00=TRUE;
	}
} /* DiskDrive::StepOut */


/*--------------------------------------------------------------------------*/
/* Check if disc is write protected                                         */
bool DiscDrive::IsWriteProtected(unsigned char drv) {
	return drive[drv].writeprotect;
} /* DiscDrive:IsWriteProtected */


/*--------------------------------------------------------------------------*/
/* Check if head is at track 0                                              */
bool DiscDrive::IsTrack00(unsigned char drv) {
	return drive[drv].track00;
} /* DiscDrive::IsTrack00 */

/*--------------------------------------------------------------------------*/
/* Check for index pulse                                                    */
bool DiscDrive::IndexPulse(unsigned char drv) {
	return drive[drv].index;
} /* DiscDrive::IndexPulse */
