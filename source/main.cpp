	    /*Wii MP3 - A simple Mp3 player by Wentstorm & Lupo96 */
    /*This software is distribuited completly FREE under GPL license */

#include <gccore.h>
#include <wiiuse/wpad.h>

#include <fat.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdarg.h>
#include <asndlib.h>
#include <mp3player.h>

#include "oggplayer.h"

#define MB_SIZE		1048576.0
#define MAXPATHLEN	512

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

/*Current browsing device*/
char *device;

/*Previus directory array*/
char *curr_url;

/*Song's variables*/
FILE *music = 0;
long lSize;
char *mbuffer;
size_t mresult;
char *mp3_url;
char *ogg_url;

/*Function prototype */
void play_menu_mp3 ();
void play_menu_ogg ();
void print_banner ();
void main_menu ();

void
InitSD ()
{
  fatUnmount ("sd:/");
  __io_wiisd.shutdown ();
  fatMountSimple ("sd", &__io_wiisd);
}

void
InitUSB ()
{
  fatUnmount ("usb:/");
  fatMountSimple ("usb", &__io_usbstorage);
}

typedef struct
{
  char path[MAXPATHLEN];
  int isdir;
} FILE_LIST_ELEMENT;

typedef FILE_LIST_ELEMENT *FILE_LIST;

FILE_LIST filelist;
int nfiles;

void
ListFile (const char *dirn)
{
  DIR *dir;
  struct dirent *dirp;
  if ((dir = opendir (dirn)) == NULL)
    {
      return;
    }

  struct stat st;
  if (filelist != NULL)
    free (filelist);
  filelist = (FILE_LIST) malloc (0);
  nfiles = 0;
  char path[MAXPATHLEN];
  while ((dirp = readdir (dir)) != NULL)
    {
      if (strcmp (dirn, "/") == 0)
	{
	  sprintf (path, "%s%s", dirn, dirp->d_name);
	}
      else
	{
	  sprintf (path, "%s/%s", dirn, dirp->d_name);
	}

      stat (path, &st);
      if ((strcmp (dirp->d_name, ".") == 0
	   || strcmp (dirp->d_name, "..") == 0) && (st.st_mode & S_IFDIR))
	continue;
      filelist =
	(FILE_LIST) realloc (filelist,
			     (nfiles + 2) * sizeof (FILE_LIST_ELEMENT));
      sprintf (filelist[nfiles].path, "%s", path);
      if (st.st_mode & S_IFDIR)
	filelist[nfiles].isdir = 1;
      else
	filelist[nfiles].isdir = 0;
      nfiles++;
    }
  closedir (dir);
}

char *
FileBrowser (const char *dirn)
{
  int cursor = 0;
  char curdir[MAXPATHLEN];

  sprintf (curdir, "%s", dirn);
  ListFile (curdir);
  VIDEO_ClearFrameBuffer (rmode, xfb, COLOR_BLACK);
  print_banner ();
  for (int i = cursor; i < nfiles && i < 22 + cursor; i++)
    {
      if (i == cursor)
	{
	  printf ("\x1b[%d;7H>", 10 + i - cursor);
	}
      else
	{
	  printf ("\x1b[%d;7H ", 10 + i - cursor);
	}
      printf ("\x1b[%d;9H%s", 10 + i - cursor, filelist[i].path);
    }

  /*infinite loop */
  while (1)
    {
      WPAD_ScanPads ();
      u32 pressed = WPAD_ButtonsDown (0);

      if (pressed & WPAD_BUTTON_DOWN)
	{
	  if (cursor < nfiles - 1)
	    {
	      cursor++;
	      VIDEO_ClearFrameBuffer (rmode, xfb, COLOR_BLACK);
	      print_banner ();
	      for (int i = cursor; i < nfiles && i < 22 + cursor; i++)
		{
		  if (i == cursor)
		    {
		      printf ("\x1b[%d;7H>", 10 + i - cursor);
		    }
		  else
		    {
		      printf ("\x1b[%d;7H ", 10 + i - cursor);
		    }
		  printf ("\x1b[%d;9H%s", 10 + i - cursor, filelist[i].path);
		}
	    }
	}
      else if (pressed & WPAD_BUTTON_UP)
	{
	  if (cursor > 0)
	    {
	      cursor--;
	      VIDEO_ClearFrameBuffer (rmode, xfb, COLOR_BLACK);
	      print_banner ();
	      for (int i = cursor; i < nfiles && i < 22 + cursor; i++)
		{
		  if (i == cursor)
		    {
		      printf ("\x1b[%d;7H>", 10 + i - cursor);
		    }
		  else
		    {
		      printf ("\x1b[%d;7H ", 10 + i - cursor);
		    }
		  printf ("\x1b[%d;9H%s", 10 + i - cursor, filelist[i].path);
		}
	    }
	}
      else if (pressed & WPAD_BUTTON_HOME)
	{
	  printf ("\n\n\t     Exit...");
	  exit (0);
	}
      else if (pressed & WPAD_BUTTON_A)
	{
	  /*check if the selected item is a foder */
	  if (filelist[cursor].isdir)
	    {
	      FileBrowser (filelist[cursor].path);
	    }
	  else
	    {
	      if (strcasestr (filelist[cursor].path, ".mp3"))
		{
		  mp3_url = (filelist[cursor].path);
		  VIDEO_ClearFrameBuffer (rmode, xfb, COLOR_BLACK);
		  play_menu_mp3 ();
		}
	      else if (strcasestr (filelist[cursor].path, ".ogg"))
		{
		  curr_url = curdir;
		  ogg_url = (filelist[cursor].path);
		  VIDEO_ClearFrameBuffer (rmode, xfb, COLOR_BLACK);
		  play_menu_ogg ();
		}
	    }
	}
      else if (pressed & WPAD_BUTTON_B)
	{
	  if (strcmp (curdir, device) == 0)
	    {
	      VIDEO_ClearFrameBuffer (rmode, xfb, COLOR_BLACK);
	      main_menu ();
	    }
	  else
	    {
	      int i;
	      curr_url = curdir;
	      int len = strlen (curr_url);

	      VIDEO_ClearFrameBuffer (rmode, xfb, COLOR_BLACK);

	      for (i = len; curr_url[i] != '/'; i--);
	      if (i == 0)
		strcpy (curr_url, device);
	      else
		curr_url[i] = 0;
	      FileBrowser (curr_url);
	    }
	}
    }
  return NULL;
}

void
print_banner (void)
{
  printf ("\x1b[2;15H  __ __ __   __   __   _______   ______   ______ ");
  printf ("\x1b[3;15H |  |  |  | |__| |__| |   |   | |   __ | |__    |");
  printf ("\x1b[4;15H |  |  |  | |  | |  | |       | |    __|  |__   |");
  printf ("\x1b[5;15H |________| |__| |__| |__|_|__| |___|    |______|");

  printf ("\x1b[7;7H[>] Press [A] to open a file or directory");
  printf ("\x1b[8;7H[>] Press [B] to go back");
}

void
play_menu_mp3 (void)
{
  uint ctrl_seconds = 0;
  int seconds = 0;
  int minutes = 0;
  char *status = 0;
  int slash = 1;
  int volume = 120;

  /*Open selected file */
  music = fopen (mp3_url, "rb");

  if (!music)
    {
      printf ("Could not open the file!\n");
      FileBrowser (device);
    }

  fseek (music, 0, SEEK_END);
  lSize = ftell (music);
  f32 mbSize = lSize / MB_SIZE;
  rewind (music);

  /*Allocate buffer */
  mbuffer = (char *) malloc (sizeof (char) * lSize);
  mresult = fread (mbuffer, 1, lSize, music);

  /*Play buffer when is stored the file */
  MP3Player_PlayBuffer (mbuffer, lSize, NULL);
  MP3Player_Volume (volume);

  /*close file */
  fclose (music);

  while (slash == 1)
    {
      if (strcasestr (mp3_url, "/"))
	mp3_url++;
      else
	slash = 0;
    }

  printf ("\x1b[2;15H  __ __ __   __   __   _______   ______   ______ ");
  printf ("\x1b[3;15H |  |  |  | |__| |__| |   |   | |   __ | |__    |");
  printf ("\x1b[4;15H |  |  |  | |  | |  | |       | |    __|  |__   |");
  printf ("\x1b[5;15H |________| |__| |__| |__|_|__| |___|    |______|");

  printf ("\x1b[7;7H[>] Now Playing: %s", mp3_url);
  printf ("\x1b[10;7H[>] Size: %lu bytes, %.2f MB", lSize, mbSize);

  printf ("\x1b[14;7H[++] Usage:");
  printf ("\x1b[15;9H[>] Press [UP] to higher the volume");
  printf ("\x1b[16;9H[>] Press [DOWN] to lower the volume");

  printf ("\x1b[17;9H[>] Press [A] to pause/unpause");
  //printf ("\x1b[18;9H[>] Press [PLUS] to continue");

  printf ("\x1b[18;9H[>] Press [MINUS] to stop");
  printf ("\x1b[19;9H[>] Press [PLUS] to replay");

  printf ("\x1b[20;9H[>] Press [B] to return to the browser");
  printf ("\x1b[21;9H[>] Press [HOME] to exit");

  /*infinite loop */
  while (1)
    {
      printf ("\x1b[8;7H[>] State: < %s", status);
      printf ("\x1b[12;7H[>] Volume: %i/255", volume);

      if (ASND_GetTimerVoice (0) / 1000 >= ctrl_seconds)
	{
	  if (minutes == 0 && seconds < 60)
	    {
	      printf ("\x1b[11;7H[>] Playing since: %i \x1b[11;29Hseconds",
		      seconds);
	      seconds = seconds + 1;
	      ctrl_seconds = ctrl_seconds + 1;
	    }
	  else if (seconds >= 60)
	    {
	      seconds = 0;
	      minutes = minutes + 1;
	      printf
		("\x1b[11;7H[>] Playing since: %i \x1b[11;29Hminutes, %i \x1b[11;41Hseconds",
		 minutes, seconds);
	    }

	  if (minutes > 0 && seconds < 60)
	    {
	      printf
		("\x1b[11;7H[>] Playing since: %i \x1b[11;29Hminutes, %i \x1b[11;41Hseconds",
		 minutes, seconds);
	      seconds = seconds + 1;
	      ctrl_seconds = ctrl_seconds + 1;
	    }
	}

      WPAD_ScanPads ();
      u32 pressed = WPAD_ButtonsDown (0);

      /*Exit */
      if (pressed & WPAD_BUTTON_HOME)
	{
	  printf ("\x1b[24;7HExit...");
	  exit (0);
	}

      /*Back to the browser */
      else if (pressed & WPAD_BUTTON_B)
	{
	  ASND_Pause (0);
	  MP3Player_Stop ();
	  free (mbuffer);
	  FileBrowser (curr_url);
	}

      /*Higher the volume */
      else if (pressed & WPAD_BUTTON_UP)
	{
	  if (volume < 255)
	    {
	      volume = volume + 15;
	    }
	  else if (volume >= 255)
	    {
	      volume = 255;
	    }
	  printf ("\x1b[12;7H[>] Volume:        ");
	  MP3Player_Volume (volume);
	}

      /*Lower the volume */
      else if (pressed & WPAD_BUTTON_DOWN)
	{
	  if (volume > 0)
	    {
	      volume = volume - 15;
	    }
	  else if (volume <= 0)
	    {
	      volume = 0;
	    }
	  printf ("\x1b[12;7H[>] Volume:        ");
	  MP3Player_Volume (volume);
	}
      /*Stop */
      else if (pressed & WPAD_BUTTON_MINUS && !ASND_Is_Paused ())
	{
	  MP3Player_Stop ();
	}
      /*Replay */
      else if (pressed & WPAD_BUTTON_PLUS && !ASND_Is_Paused ())
	{
	  if (!MP3Player_IsPlaying ())
	    {
	      MP3Player_PlayBuffer (mbuffer, lSize, NULL);
	      MP3Player_Volume (volume);
	      ctrl_seconds = 0;
	      seconds = 0;
	      minutes = 0;
	    }
	  else
	    {
	      MP3Player_Stop ();
	      MP3Player_PlayBuffer (mbuffer, lSize, NULL);
	      MP3Player_Volume (volume);
	      ctrl_seconds = 0;
	      seconds = 0;
	      minutes = 0;
	    }
	}
      /*Pause */
      else if (pressed & WPAD_BUTTON_A && !ASND_Is_Paused ()
	       && MP3Player_IsPlaying ())
	{
	  ASND_Pause (1);
	}
      /*Unpause */
      else if (pressed & WPAD_BUTTON_A && ASND_Is_Paused ())
	{
	  ASND_Pause (0);
	  MP3Player_Volume (volume);
	}

      if (MP3Player_IsPlaying () && !ASND_Is_Paused ())
	{
	  status = ((char *) "Playng > ");
	}
      else if (!MP3Player_IsPlaying ())
	{
	  status = ((char *) "Stopped >");
	}
      else if (MP3Player_IsPlaying () && ASND_Is_Paused ())
	{
	  status = ((char *) "Paused > ");
	}
    }
}

void
play_menu_ogg (void)
{
  int ctrl_seconds = 0;
  int seconds = 0;
  int minutes = 0;
  char *status = 0;
  int stopped = 0;
  int paused = 0;
  int slash = 1;
  int volume = 120;

  /*Open selected file */
  music = fopen (ogg_url, "rb");

  if (!music)
    {
      printf ("Could not open the file!\n");
      FileBrowser ("/");
    }

  fseek (music, 0, SEEK_END);
  lSize = ftell (music);
  f32 mbSize = lSize / MB_SIZE;
  rewind (music);

  /*Allocate buffer */
  mbuffer = (char *) malloc (sizeof (char) * lSize);
  mresult = fread (mbuffer, 1, lSize, music);

  /*Play buffer when is stored the file */
  PlayOgg (mbuffer, lSize, 0, OGG_ONE_TIME);
  SetVolumeOgg (volume);
  status = ((char *) "Playing");

  /*close file */
  fclose (music);

  while (slash == 1)
    {
      if (strcasestr (ogg_url, "/"))
	ogg_url++;
      else
	slash = 0;
    }

  printf ("\x1b[2;15H  __ __ __   __   __   _______   ______   ______ ");
  printf ("\x1b[3;15H |  |  |  | |__| |__| |   |   | |   __ | |__    |");
  printf ("\x1b[4;15H |  |  |  | |  | |  | |       | |    __|  |__   |");
  printf ("\x1b[5;15H |________| |__| |__| |__|_|__| |___|    |______|");

  printf ("\x1b[7;7H[>] Now Playing: %s", ogg_url);
  printf ("\x1b[10;7H[>] Size: %lu bytes, %.2f MB", lSize, mbSize);

  printf ("\x1b[14;7H[++] Usage:");
  printf ("\x1b[15;9H[>] Press [UP] to higher the volume");
  printf ("\x1b[16;9H[>] Press [DOWN] to lower the volume");
  printf ("\x1b[17;9H[>] Press [A] to pause/unpause");
  printf ("\x1b[18;9H[>] Press [MINUS] to stop");
  printf ("\x1b[19;9H[>] Press [PLUS] to replay");
  printf ("\x1b[20;9H[>] Press [B] to return to the browser");
  printf ("\x1b[21;9H[>] Press [HOME] to exit");

  /*infinite loop */
  while (1)
    {
      printf ("\x1b[9;7H[>] State: < %s >", status);
      printf ("\x1b[12;7H[>] Volume: %i/255", volume);

      if (GetTimeOgg () / 1000 >= ctrl_seconds)
	{
	  if (minutes == 0 && seconds < 60)
	    {
	      printf
		("\x1b[11;7H[>] Playing since: %i \x1b[11;29Hseconds",
		 seconds);
	      seconds = seconds + 1;
	      ctrl_seconds = ctrl_seconds + 1;
	    }
	  else if (seconds >= 60)
	    {
	      seconds = 0;
	      minutes = minutes + 1;
	      printf
		("\x1b[11;7H[>] Playing since: %i \x1b[11;29Hminutes, %i \x1b[11;41Hseconds",
		 minutes, seconds);
	    }
	  if (minutes > 0 && seconds < 60)
	    {
	      printf
		("\x1b[11;7H[>] Playing since: %i \x1b[11;29Hminutes, %i \x1b[11;41Hseconds",
		 minutes, seconds);
	      seconds = seconds + 1;
	      ctrl_seconds = ctrl_seconds + 1;
	    }
	}

      // Call WPAD_ScanPads each loop, this reads the latest controller states
      WPAD_ScanPads ();

      // WPAD_ButtonsDown tells us which buttons were pressed in this loop
      // this is a "one shot" state which will not fire again until the button has been released
      u32 pressed = WPAD_ButtonsDown (0);

      // We return to the launcher application via exit
      if (pressed & WPAD_BUTTON_HOME)
	{
	  printf ("\x1b[23;7HExiting...");
	  exit (0);
	}
      else if (pressed & WPAD_BUTTON_B)
	{
	  /*Call filebrowser */
	  StopOgg ();
	  free (mbuffer);
	  FileBrowser (curr_url);
	}
      else if (pressed & WPAD_BUTTON_UP)
	{
	  if (volume < 255)
	    {
	      volume = volume + 15;
	    }
	  else if (volume >= 255)
	    {
	      volume = 255;
	    }
	  printf ("\x1b[12;7H[>] Volume:        ");
	  SetVolumeOgg (volume);
	}
      else if (pressed & WPAD_BUTTON_DOWN)
	{
	  if (volume > 0)
	    {
	      volume = volume - 15;
	    }
	  else if (volume <= 0)
	    {
	      volume = 0;
	    }
	  printf ("\x1b[12;7H[>] Volume:        ");
	  SetVolumeOgg (volume);
	}
      else if (pressed & WPAD_BUTTON_A)
	{
	  if (stopped == 0 && paused == 0)
	    {
	      PauseOgg (1);
	      paused = 1;
	      status = ((char *) "Paused ");
	    }
	  else if (stopped == 0 && paused == 1)
	    {
	      PauseOgg (0);
	      SetVolumeOgg (volume);
	      paused = 0;
	      status = ((char *) "Playing");
	    }
	}
      else if (pressed & WPAD_BUTTON_MINUS)
	{
	  StopOgg ();
	  ctrl_seconds = 0;
	  seconds = 0;
	  minutes = 0;
	  status = ((char *) "Stopped");
	  printf
	    ("\x1b[11;7H[>] Playing since: 0  seconds                                        ");
	  stopped = 1;
	}
      else if (pressed & WPAD_BUTTON_PLUS)
	{
	  StopOgg ();
	  ctrl_seconds = 0;
	  seconds = 0;
	  minutes = 0;
	  printf
	    ("\x1b[11;7H[>] Playing since: 0  seconds                                        ");
	  PlayOgg (mbuffer, lSize, 0, OGG_ONE_TIME);
	  SetVolumeOgg (volume);
	  status = ((char *) "Playing");
	  stopped = 0;
	}
      else if (StatusOgg () == OGG_STATUS_EOF)
	{
	  status = ((char *) "Stopped");
	  ctrl_seconds = 0;
	  seconds = 0;
	  minutes = 0;
	  stopped = 1;
	  printf
	    ("\x1b[11;7H[>] Playing since: 0  seconds                                        ");
	}
      /* Wait for the next frame */
      VIDEO_WaitVSync ();
    }
}

void
main_menu (void)
{
  printf ("\x1b[2;15H  __ __ __   __   __   _______   ______   ______ ");
  printf ("\x1b[3;15H |  |  |  | |__| |__| |   |   | |   __ | |__    |");
  printf ("\x1b[4;15H |  |  |  | |  | |  | |       | |    __|  |__   |");
  printf ("\x1b[5;15H |________| |__| |__| |__|_|__| |___|    |______|");
  printf ("\x1b[7;27H v2.0 By Wentstorm & Lupo96");


  printf ("\x1b[9;7H[++] Source selector:");
  printf ("\x1b[10;9H[>] Press [LEFT] and [RIGHT] to change browsing device");
  printf ("\x1b[11;9H[>] Press [A] to start browse it");
  printf ("\x1b[12;9H[>] Press [HOME] to exit.");

  int drive = 0;
  char *selected[2] = { (char *) "SD Card >  ", (char *) "USB Drive >" };

  while (1)
    {
      WPAD_ScanPads ();
      u32 pressed = WPAD_ButtonsDown (0);

      printf ("\x1b[9;7H[>] Browsing device: < %s", selected[drive]);

      if (pressed & WPAD_BUTTON_HOME)
	{
	  printf ("\x1b[18;7HExit...");
	  exit (0);
	}

      if ((pressed & WPAD_BUTTON_RIGHT) || (pressed & WPAD_BUTTON_LEFT))
	{
	  if (drive > 0)
	    {
	      drive--;
	    }
	  else
	    {
	      drive = 1;
	    }
	}

      if (pressed & WPAD_BUTTON_A)
	{
	  bool SDisInserted = __io_wiisd.isInserted ();
	  bool USBisInserted = __io_usbstorage.isInserted ();

	  switch (drive)
	    {
	    case (0):
	      InitSD ();
	      if (SDisInserted)
		{
		  device = ((char *) "sd:/");
		  FileBrowser ("sd:/");
		}
	    case (1):
	      InitUSB ();
	      if (USBisInserted)
		{
		  device = ((char *) "usb:/");
		  FileBrowser ("usb:/");
		}
	    }
	}
    }
}

int
main ()
{
  VIDEO_Init ();
  WPAD_Init ();
  rmode = VIDEO_GetPreferredMode (NULL);
  xfb = MEM_K0_TO_K1 (SYS_AllocateFramebuffer (rmode));
  console_init (xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
		rmode->fbWidth * VI_DISPLAY_PIX_SZ);
  VIDEO_Configure (rmode);
  VIDEO_SetNextFramebuffer (xfb);


//VIDEO_ClearFrameBuffer (rmode, xfb, COLOR_BLUE);
//VIDEO_ClearFrameBuffer (rmode, xfb[1], COLOR_BLUE);
//VIDEO_SetNextFramebuffer (xfb);

  VIDEO_SetBlack (FALSE);
  VIDEO_Flush ();
  VIDEO_WaitVSync ();

  /*Try to inizialize FAT lib */
  if (!fatInitDefault ())
    {
      printf ("\x1b[2;7H fatInit failure: terminating...\n");
      exit (0);
    }

  /*Init audio subsystem */
  ASND_Init ();
  MP3Player_Init ();

  main_menu ();
}
