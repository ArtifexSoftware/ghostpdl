
/* Copyright (C) 2001-2008 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

// $Id$
//
//
// This is the setup program for Win32 GPL Ghostscript
//
// The starting point is a self extracting zip archive
// with the following contents:
//   setupgs.exe
//   uninstgs.exe
//   filelist.txt      (contains list of program files)
//   gs#.##\*          (files listed in filelist.txt)
// This is the same as the zip file created by Aladdin Enterprises,
// with the addition of setupgs.exe, uninstgs.exe, and filelist.txt
//
// The first line of the file filelist.txt
// contains the uninstall name to be used.  
// The second line contains name of the main directory where 
// uninstall log files are to be placed.  
// Subsequent lines contain files to be copied (but not directories).
// For example, filelist.txt might contain:
//   GPL Ghostscript 8.55
//   gs8.55
//   gs8.55\bin\gsdll32.dll
//   gs8.55\lib\gs_init.ps
//
// The default install directory is c:\gs.
// The default Start Menu Folder is Ghostscript.
// These are set in the resources.
// The setup program will create the following uninstall log files
//   c:\gs\gs#.##\uninstal.txt
// The uninstall program (accessed through control panel) will not 
// remove directories nor will it remove itself.
//
// If the install directory is the same as the current file 
// location, no files will be copied, but the existence of each file 
// will be checked.  This allows the archive to be unzipped, then
// configured in its current location.  Running the uninstall will not 
// remove uninstgs.exe, setupgs.exe, or filelist.txt.


#define STRICT
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <shlobj.h>
#include <stdio.h>
#include <direct.h>
#include <ctype.h>

#pragma comment(lib,"user32.lib")

#ifdef MAX_PATH
#define MAXSTR MAX_PATH
#else
#define MAXSTR 256
#endif

CHAR g_szAppName[MAXSTR];

// Prototypes
BOOL init();
BOOL make_filelist(int argc, char *argv[]);


//////////////////////////////////////////////////////////////////////
// Entry point
//////////////////////////////////////////////////////////////////////

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
  strcpy(g_szAppName, "make_filelist");
	if (!init()) {
		MessageBox(HWND_DESKTOP, "Initialisation failed", 
			g_szAppName, MB_OK);
		return 1;
	}
	
	return 0;
}




//////////////////////////////////////////////////////////////////////
// Initialisation and Main dialog box
//////////////////////////////////////////////////////////////////////

void
message_box(const char *str)
{
	MessageBox(HWND_DESKTOP, str, g_szAppName, MB_OK);
}


BOOL
init()
{
	DWORD dwVersion = GetVersion();
	// get source directory

	
	if (LOBYTE(LOWORD(dwVersion)) < 4) {
        MessageBox(HWND_DESKTOP, 
			"This install program needs Windows 4.0 or later",
			g_szAppName, MB_OK);
		return FALSE;
	}
	
	
#define MAXCMDTOKENS 128

	int argc;
	LPSTR argv[MAXCMDTOKENS];
	LPSTR p;
	char *args;
	char *d, *e;
     
	p = GetCommandLine();

	argc = 0;
	args = (char *)malloc(lstrlen(p)+1);
	if (args == (char *)NULL)
		return 1;
       
	// Parse command line handling quotes.
	d = args;
	while (*p) {
		// for each argument

		if (argc >= MAXCMDTOKENS - 1)
			break;

		e = d;
		while ((*p) && (*p != ' ')) {
			if (*p == '\042') {
				// Remove quotes, skipping over embedded spaces.
				// Doesn't handle embedded quotes.
				p++;
				while ((*p) && (*p != '\042'))
					*d++ =*p++;
			}
			else 
				*d++ = *p;
			if (*p)
				p++;
		}
		*d++ = '\0';
		argv[argc++] = e;

		while ((*p) && (*p == ' '))
			p++;	// Skip over trailing spaces
	}
	argv[argc] = NULL;

	if (argc > 2) {
		// Probably creating filelist.txt
		return make_filelist(argc, argv);
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////
// Create file list
//////////////////////////////////////////////////////////////////////

FILE *fList;

typedef int (*PFN_dodir)(const char *name);

/* Called once for each directory */
int
dodir(const char *filename)
{
    return 0;
}

/* Called once for each file */
int
dofile(const char *filename)
{
    if (fList != (FILE *)NULL) {
		fputs(filename, fList);
		fputs("\n", fList);
    }
	
    return 0;
}


/* Walk through directory 'path', calling dodir() for given directory
 * and dofile() for each file.
 * If recurse=1, recurse into subdirectories, calling dodir() for
 * each directory.
 */
int 
dirwalk(char *path, int recurse, PFN_dodir dodir, PFN_dodir dofile)
{    
	WIN32_FIND_DATA find_data;
	HANDLE find_handle;
	char pattern[MAXSTR];	/* orig pattern + modified pattern */
	char base[MAXSTR];
	char name[MAXSTR];
	BOOL bMore = TRUE;
	char *p;
	
	
	if (path) {
		strcpy(pattern, path);
		if (strlen(pattern) != 0)  {
			p = pattern + strlen(pattern) -1;
			if (*p == '\\')
				*p = '\0';		// truncate trailing backslash
		}
		
		strcpy(base, pattern);
		if (strchr(base, '*') != NULL) {
			// wildcard already included
			// truncate it from the base path
			if ( (p = strrchr(base, '\\')) != NULL )
				*(++p) = '\0';
		}
		else if (isalpha(pattern[0]) && 
			pattern[1]==':' && pattern[2]=='\0')  {
			strcat(pattern, "\\*");		// search entire disk
			strcat(base, "\\");
		}
		else {
			// wildcard NOT included
			// check to see if path is a directory
			find_handle = FindFirstFile(pattern, &find_data);
			if (find_handle != INVALID_HANDLE_VALUE) {
				FindClose(find_handle);
				if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					strcat(pattern, "\\*");		// yes, search files 
					strcat(base, "\\");
				}
				else {
					dofile(path);				// no, return just this file
					return 0;
				}
			}
			else
				return 1;	// path invalid
		}
	}
	else {
		base[0] = '\0';
		strcpy(pattern, "*");
	}
	
	find_handle = FindFirstFile(pattern,  &find_data);
	if (find_handle == INVALID_HANDLE_VALUE)
		return 1;
	
	while (bMore) {
		strcpy(name, base);
		strcat(name, find_data.cFileName);
		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if ( strcmp(find_data.cFileName, ".") && 
				strcmp(find_data.cFileName, "..") ) {
				dodir(name);
				if (recurse)
					dirwalk(name, recurse, dodir, dofile);
			}
		}
		else {
			dofile(name);
		}
		bMore = FindNextFile(find_handle, &find_data);
	}
	FindClose(find_handle);
	
	return 0;
}



// This is used when creating a file list.

BOOL make_filelist(int argc, char *argv[])
{
    char *title = NULL;
    char *dir = NULL;
    char *list = NULL;
    int i;
	
    for (i=1; i<argc; i++) {
		if (strcmp(argv[i], "-title") == 0) {
			i++;
			title = argv[i];
		}
		else if (strcmp(argv[i], "-dir") == 0) {
			i++;
			dir = argv[i];
		}
		else if (strcmp(argv[i], "-list") == 0) {
			i++;
			list = argv[i];
		}
		else {
		    if ((title == NULL) || (strlen(title) == 0) ||
			(dir == NULL) || (strlen(dir) == 0) ||
			(list == NULL) || (strlen(list) == 0)) {
			message_box("Usage: make_filelist -title \042GPL Ghostscript #.##\042 -dir \042gs#.##\042 -list \042filelist.txt\042 spec1 spec2 specn\n");
			return FALSE;
		    }
		    if (fList == (FILE *)NULL) {
			    if ( (fList = fopen(list, "w")) == (FILE *)NULL ) {
					message_box("Can't write list file\n");
					return FALSE;
			    }
			    fputs(title, fList);
			    fputs("\n", fList);
			    fputs(dir, fList);
			    fputs("\n", fList);
		    }
		    if (argv[i][0] == '@') {
			// Use @filename with list of files/directories
			// to avoid DOS command line limit
			FILE *f;
			char buf[MAXSTR];
			int j;
			if ( (f = fopen(&(argv[i][1]), "r")) != (FILE *)NULL) {
			    while (fgets(buf, sizeof(buf), f)) {
				// remove trailing newline and spaces
				while ( ((j = strlen(buf)-1) >= 0) &&
				    ((buf[j] == '\n') || (buf[j] == ' ')) )
				    buf[j] = '\0';
			        dirwalk(buf, TRUE, &dodir, &dofile);
			    }
			    fclose(f);
			}
			else {
				wsprintf(buf, "Can't open @ file \042%s\042",
				    &argv[i][1]);
				message_box(buf);
			}
		    }
		    else
		        dirwalk(argv[i], TRUE, &dodir, &dofile);
		}
    }
	
    if (fList != (FILE *)NULL) {
        fclose(fList);
	fList = NULL;
    }
    return TRUE;
}

