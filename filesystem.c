/*
===========================================================================
    Copyright (C) 2010-2013  Ninja and TheKelm of the IceOps-Team
    Copyright (C) 1999-2005 Id Software, Inc.

    This file is part of CoD4X17a-Server source code.

    CoD4X17a-Server source code is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    CoD4X17a-Server source code is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>
===========================================================================
*/


/*****************************************************************************
 * name:		files.c
 *
 * desc:		handle based filesystem for Quake III Arena 
 *
 * $Archive: /MissionPack/code/qcommon/files.c $
 *
 *****************************************************************************/

/*
=============================================================================

QUAKE3 FILESYSTEM

All of Quake's data access is through a hierarchical file system, but the contents of 
the file system can be transparently merged from several sources.

A "qpath" is a reference to game file data.  MAX_ZPATH is 256 characters, which must include
a terminating zero. "..", "\\", and ":" are explicitly illegal in qpaths to prevent any
references outside the quake directory system.

The "base path" is the path to the directory holding all the game directories and usually
the executable.  It defaults to ".", but can be overridden with a "+set fs_basepath c:\quake3"
command line to allow code debugging in a different directory.  Basepath cannot
be modified at all after startup.  Any files that are created (demos, screenshots,
etc) will be created reletive to the base path, so base path should usually be writable.

The "cd path" is the path to an alternate hierarchy that will be searched if a file
is not located in the base path.  A user can do a partial install that copies some
data to a base path created on their hard drive and leave the rest on the cd.  Files
are never writen to the cd path.  It defaults to a value set by the installer, like
"e:\quake3", but it can be overridden with "+set ds_cdpath g:\quake3".

If a user runs the game directly from a CD, the base path would be on the CD.  This
should still function correctly, but all file writes will fail (harmlessly).

The "home path" is the path used for all write access. On win32 systems we have "base path"
== "home path", but on *nix systems the base installation is usually readonly, and
"home path" points to ~/.q3a or similar

The user can also install custom mods and content in "home path", so it should be searched
along with "home path" and "cd path" for game content.


The "base game" is the directory under the paths where data comes from by default, and
can be either "baseq3" or "demoq3".

The "current game" may be the same as the base game, or it may be the name of another
directory under the paths that should be searched for files before looking in the base game.
This is the basis for addons.

Clients automatically set the game directory after receiving a gamestate from a server,
so only servers need to worry about +set fs_game.

No other directories outside of the base game and current game will ever be referenced by
filesystem functions.

To save disk space and speed loading, directory trees can be collapsed into zip files.
The files use a ".pk3" extension to prevent users from unzipping them accidentally, but
otherwise the are simply normal uncompressed zip files.  A game directory can have multiple
zip files of the form "pak0.pk3", "pak1.pk3", etc.  Zip files are searched in decending order
from the highest number to the lowest, and will always take precedence over the filesystem.
This allows a pk3 distributed as a patch to override all existing data.

Because we will have updated executables freely available online, there is no point to
trying to restrict demo / oem versions of the game with code changes.  Demo / oem versions
should be exactly the same executables as release versions, but with different data that
automatically restricts where game media can come from to prevent add-ons from working.

After the paths are initialized, quake will look for the product.txt file.  If not
found and verified, the game will run in restricted mode.  In restricted mode, only 
files contained in demoq3/pak0.pk3 will be available for loading, and only if the zip header is
verified to not have been modified.  A single exception is made for q3config.cfg.  Files
can still be written out in restricted mode, so screenshots and demos are allowed.
Restricted mode can be tested by setting "+set fs_restrict 1" on the command line, even
if there is a valid product.txt under the basepath or cdpath.

If not running in restricted mode, and a file is not found in any local filesystem,
an attempt will be made to download it and save it under the base path.

If the "fs_copyfiles" cvar is set to 1, then every time a file is sourced from the cd
path, it will be copied over to the base path.  This is a development aid to help build
test releases and to copy working sets over slow network links.

File search order: when FS_FOpenFileRead gets called it will go through the fs_searchpaths
structure and stop on the first successful hit. fs_searchpaths is built with successive
calls to FS_AddGameDirectory

Additionaly, we search in several subdirectories:
current game is the current mode
base game is a variable to allow mods based on other mods
(such as baseq3 + missionpack content combination in a mod for instance)
BASEGAME is the hardcoded base game ("baseq3")

e.g. the qpath "sound/newstuff/test.wav" would be searched for in the following places:

home path + current game's zip files
home path + current game's directory
base path + current game's zip files
base path + current game's directory
cd path + current game's zip files
cd path + current game's directory

home path + base game's zip file
home path + base game's directory
base path + base game's zip file
base path + base game's directory
cd path + base game's zip file
cd path + base game's directory

home path + BASEGAME's zip file
home path + BASEGAME's directory
base path + BASEGAME's zip file
base path + BASEGAME's directory
cd path + BASEGAME's zip file
cd path + BASEGAME's directory

server download, to be written to home path + current game's directory


The filesystem can be safely shutdown and reinitialized with different
basedir / cddir / game combinations, but all other subsystems that rely on it
(sound, video) must also be forced to restart.

Because the same files are loaded by both the clip model (CM_) and renderer (TR_)
subsystems, a simple single-file caching scheme is used.  The CM_ subsystems will
load the file with a request to cache.  Only one file will be kept cached at a time,
so any models that are going to be referenced by both subsystems should alternate
between the CM_ load function and the ref load function.

TODO: A qpath that starts with a leading slash will always refer to the base game, even if another
game is currently active.  This allows character models, skins, and sounds to be downloaded
to a common directory no matter which game is active.

How to prevent downloading zip files?
Pass pk3 file names in systeminfo, and download before FS_Restart()?

Aborting a download disconnects the client from the server.

How to mark files as downloadable?  Commercial add-ons won't be downloadable.

Non-commercial downloads will want to download the entire zip file.
the game would have to be reset to actually read the zip in

Auto-update information

Path separators

Casing

  separate server gamedir and client gamedir, so if the user starts
  a local game after having connected to a network game, it won't stick
  with the network game.

  allow menu options for game selection?

Read / write config to floppy option.

Different version coexistance?

When building a pak file, make sure a q3config.cfg isn't present in it,
or configs will never get loaded from disk!

  todo:

  downloading (outside fs?)
  game directory passing and restarting

=============================================================================

*/

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "filesystem.h"
#include "qcommon_io.h"
#include "cvar.h"
#include "q_platform.h"

#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <unistd.h>

/*
typedef int (__cdecl *tFS_ReadFile)(const char* qpath, void **buffer);
tFS_ReadFile FS_ReadFile = (tFS_ReadFile)(0x818bb8c);

typedef void (__cdecl *tFS_WriteFile)(const char* qpath, const void *buffer, int size);
tFS_WriteFile FS_WriteFile = (tFS_WriteFile)(0x818a58c);

typedef void (__cdecl *tFS_FreeFile)(void *buffer);
tFS_FreeFile FS_FreeFile = (tFS_FreeFile)(0x8187430);

typedef void (__cdecl *tFS_SV_Rename)(const char* from,const char* to);
tFS_SV_Rename FS_SV_Rename = (tFS_SV_Rename)(0x81287da);

typedef int (__cdecl *tFS_Write)(void const* data,int length, fileHandle_t);
tFS_Write FS_Write = (tFS_Write)(0x8186ec4);

typedef int (__cdecl *tFS_Read)(void const* data,int length, fileHandle_t);
tFS_Read FS_Read = (tFS_Read)(0x8186f64);

typedef int (__cdecl *tFS_HandleForFile)(fileHandle_t);
tFS_HandleForFile FS_HandleForFile = (tFS_HandleForFile)(0x818690e);

typedef int (__cdecl *tFS_FOpenFileByMode)(const char *qpath, fileHandle_t *f, fsMode_t mode);
tFS_FOpenFileByMode FS_FOpenFileByMode = (tFS_FOpenFileByMode)(0x818ba98);
*/


/*
==============
FS_Initialized
==============
*/

qboolean FS_Initialized() {
	return (fs_searchpaths != NULL);
}



static fileHandle_t	FS_HandleForFile(void) {
	int		i;

	for ( i = 1 ; i < MAX_FILE_HANDLES ; i++ ) {
		if ( fsh[i].handleFiles.file.o == NULL ) {
			return i;
		}
	}
	Com_Error( ERR_DROP, "FS_HandleForFile: none free" );
	return 0;
}

static FILE	*FS_FileForHandle( fileHandle_t f ) {
	if ( f < 0 || f > MAX_FILE_HANDLES ) {
		Com_Error( ERR_DROP, "FS_FileForHandle: out of range %i\n", f);
	}
	if ( !fsh[f].handleFiles.file.o ) {
		Com_Error( ERR_DROP, "FS_FileForHandle: NULL" );
	}
	
	return fsh[f].handleFiles.file.o;
}

void	FS_ForceFlush( fileHandle_t f ) {
	FILE *file;

	file = FS_FileForHandle(f);
	setvbuf( file, NULL, _IONBF, 0 );
}



/*
==================
Sys_Mkdir
==================
*/
qboolean Sys_Mkdir( const char *path )
{
	int result = mkdir( path, 0750 );

	if( result != 0 )
		return errno == EEXIST;

	return qtrue;
}



/*
================
FS_filelength

If this is called on a non-unique FILE (from a pak file),
it will return the size of the pak file, not the expected
size of the file.
================
*/
int FS_filelength( fileHandle_t f ) {
	int		pos;
	int		end;
	FILE*	h;

	h = FS_FileForHandle(f);
	pos = ftell (h);
	fseek (h, 0, SEEK_END);
	end = ftell (h);
	fseek (h, pos, SEEK_SET);

	return end;
}

/*
====================
FS_ReplaceSeparators

Fix things up differently for win/unix/mac
====================
*/
static void FS_ReplaceSeparators( char *path ) {
	char	*s;

	for ( s = path ; *s ; s++ ) {
		if ( *s == '/' || *s == '\\' ) {
			*s = PATH_SEP;
		}
	}
}

/*
===================
FS_BuildOSPath

Qpath may have either forward or backwards slashes
===================
*/
char *FS_BuildOSPath( const char *base, const char *game, const char *qpath ) {

	char	temp[MAX_OSPATH];
	static char ospath[2][MAX_OSPATH];
	static int toggle;

	toggle ^= 1;	// flip-flop to allow two returns without clash

	if(!game || !game[0]){
	    game = fs_gamedir;
	}
	Com_sprintf( temp, sizeof(temp), "/%s/%s", game, qpath );
	FS_ReplaceSeparators( temp );	
	Com_sprintf( ospath[toggle], sizeof( ospath[0] ), "%s%s", base, temp );

	return ospath[toggle];
}


/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
qboolean FS_CreatePath (char *OSPath) {
	char	*ofs;

	// make absolutely sure that it can't back up the path
	// FIXME: is c: allowed???
	if ( strstr( OSPath, ".." ) || strstr( OSPath, "::" ) ) {
		Com_Printf( "WARNING: refusing to create relative path \"%s\"\n", OSPath );
		return qtrue;
	}

	for (ofs = OSPath+1 ; *ofs ; ofs++) {
		if (*ofs == PATH_SEP) {	
			// create the directory
			*ofs = 0;
			Sys_Mkdir (OSPath);
			*ofs = PATH_SEP;
		}
	}
	return qfalse;
}


/*
===========
FS_HomeRemove

===========
*/
qboolean FS_HomeRemove( const char *path ) {
	const char *osPath = FS_BuildOSPath( fs_homepath->string, "", path );
	return remove( osPath ) == 0;
}

/*
===========
FS_SV_HomeRemove

===========
*/
qboolean FS_SV_HomeRemove( const char *path ) {
	char *osPath = FS_BuildOSPath( fs_homepath->string, path, "" );
	osPath[strlen(osPath)-1] = '\0';
	return remove( osPath ) == 0;
}


/*
===========
FS_Remove

===========
*/
static void FS_Remove( const char *osPath ) {
	remove( osPath );
}

/*
================
FS_FileExists

Tests if the file exists in the current gamedir, this DOES NOT
search the paths.  This is to determine if opening a file to write
(which always goes into the current gamedir) will cause any overwrites.
NOTE TTimo: this goes with FS_FOpenFileWrite for opening the file afterwards
================
*/
qboolean FS_FileExists( const char *file )
{
	FILE *f;
	char *testpath;

	testpath = FS_BuildOSPath( fs_homepath->string, "", file );

	f = fopen( testpath, "rb" );
	if (f) {
		fclose( f );
		return qtrue;
	}
	return qfalse;
}


/*
================
FS_FileExists

Tests if the file exists in the current gamedir, this DOES NOT
search the paths.  This is to determine if opening a file to write
(which always goes into the current gamedir) will cause any overwrites.
NOTE TTimo: this goes with FS_FOpenFileWrite for opening the file afterwards
================
*/
char* FS_SV_GetFilepath( const char *file )
{
	FILE *f;
	char *testpath;

	testpath = FS_BuildOSPath( fs_homepath->string, file, "" );
        testpath[strlen(testpath)-1] = '\0';

	f = fopen( testpath, "rb" );
	if (f) {
		fclose( f );
		return testpath;
	}

        testpath = FS_BuildOSPath( fs_basepath->string, file, "" );
        testpath[strlen(testpath)-1] = '\0';

	f = fopen( testpath, "rb" );
	if (f) {
		fclose( f );
		return testpath;
	}

	return NULL;
}


/*
===========
FS_Rename

===========
*/
void FS_Rename( const char *from, const char *to ) {
	char			*from_ospath, *to_ospath;

	from_ospath = FS_BuildOSPath( fs_homepath->string, "", from );
	to_ospath = FS_BuildOSPath( fs_homepath->string, "", to );

	if ( fs_debug->boolean ) {
		Com_Printf( "FS_Rename: %s --> %s\n", from_ospath, to_ospath );
	}

	if (rename( from_ospath, to_ospath )) {
		// Failed, try copying it and deleting the original
		FS_CopyFile ( from_ospath, to_ospath );
		FS_Remove ( from_ospath );
	}
}


/*
===========
FS_SV_Rename

===========
*/
void FS_SV_Rename( const char *from, const char *to ) {
	char			*from_ospath, *to_ospath;

	from_ospath = FS_BuildOSPath( fs_homepath->string, from, "" );
	to_ospath = FS_BuildOSPath( fs_homepath->string, to, "" );
	from_ospath[strlen(from_ospath)-1] = '\0';
	to_ospath[strlen(to_ospath)-1] = '\0';

	if ( fs_debug->boolean ) {
		Com_Printf( "FS_Rename: %s --> %s\n", from_ospath, to_ospath );
	}

	if (rename( from_ospath, to_ospath )) {
		// Failed, try copying it and deleting the original
		FS_CopyFile ( from_ospath, to_ospath );
		FS_Remove ( from_ospath );
	}
}



/*
==============
FS_FCloseFile

If the FILE pointer is an open pak file, leave it open.

For some reason, other dll's can't just cal fclose()
on files returned by FS_FOpenFile...
==============
*/
qboolean FS_FCloseFile( fileHandle_t f ) {

	if(fsh[f].zipFile)
	{
		unzCloseCurrentFile(fsh[f].handleFiles.file.z);
		if(fsh[f].handleFiles.unique)
		{
			unzClose(fsh[f].handleFiles.file.z);
		}
		Com_Memset( &fsh[f], 0, sizeof( fsh[f] ) );
		return qtrue;
	}

	if (fsh[f].handleFiles.file.o) {
	// we didn't find it as a pak, so close it as a unique file
	    fclose (fsh[f].handleFiles.file.o);
	    Com_Memset( &fsh[f], 0, sizeof( fsh[f] ) );
	    return qtrue;
	}

	Com_Memset( &fsh[f], 0, sizeof( fsh[f] ) );
	return qfalse;
}


/*
===========
FS_FilenameCompare

Ignore case and seprator char distinctions
===========
*/
qboolean FS_FilenameCompare( const char *s1, const char *s2 ) {
	int		c1, c2;
	
	do {
		c1 = *s1++;
		c2 = *s2++;

		if (c1 >= 'a' && c1 <= 'z') {
			c1 -= ('a' - 'A');
		}
		if (c2 >= 'a' && c2 <= 'z') {
			c2 -= ('a' - 'A');
		}

		if ( c1 == '\\' || c1 == ':' ) {
			c1 = '/';
		}
		if ( c2 == '\\' || c2 == ':' ) {
			c2 = '/';
		}
		
		if (c1 != c2) {
			return -1;		// strings not equal
		}
	} while (c1);
	
	return 0;		// strings are equal
}

/*
===========
FS_ShiftedStrStr
===========
*/
char *FS_ShiftedStrStr(const char *string, const char *substring, int shift) {
	char buf[MAX_STRING_TOKENS];
	int i;

	for (i = 0; substring[i]; i++) {
		buf[i] = substring[i] + shift;
	}
	buf[i] = '\0';
	return strstr(string, buf);
}




/*
================
FS_fplength
================
*/

long FS_fplength(FILE *h)
{
	long		pos;
	long		end;

	pos = ftell(h);
	fseek(h, 0, SEEK_END);
	end = ftell(h);
	fseek(h, pos, SEEK_SET);

	return end;
}


/*
===========
FS_IsExt

Return qtrue if ext matches file extension filename
===========
*/

qboolean FS_IsExt(const char *filename, const char *ext, int namelen)
{
	int extlen;

	extlen = strlen(ext);

	if(extlen > namelen)
		return qfalse;

	filename += namelen - extlen;

	return !Q_stricmp(filename, ext);
}




/*
=================================================================================

DIRECTORY SCANNING FUNCTIONS

=================================================================================
*/

#define	MAX_FOUND_FILES	0x1000



/*
===========
FS_ConvertPath
===========
*/
void FS_ConvertPath( char *s ) {
	while (*s) {
		if ( *s == '\\' || *s == ':' ) {
			*s = '/';
		}
		s++;
	}
}

/*
===========
FS_PathCmp

Ignore case and seprator char distinctions
===========
*/
int FS_PathCmp( const char *s1, const char *s2 ) {
	int		c1, c2;
	
	do {
		c1 = *s1++;
		c2 = *s2++;

		if (c1 >= 'a' && c1 <= 'z') {
			c1 -= ('a' - 'A');
		}
		if (c2 >= 'a' && c2 <= 'z') {
			c2 -= ('a' - 'A');
		}

		if ( c1 == '\\' || c1 == ':' ) {
			c1 = '/';
		}
		if ( c2 == '\\' || c2 == ':' ) {
			c2 = '/';
		}
		
		if (c1 < c2) {
			return -1;		// strings not equal
		}
		if (c1 > c2) {
			return 1;
		}
	} while (c1);
	
	return 0;		// strings are equal
}



/*
========================================================================================

Handle based file calls for virtual machines

========================================================================================
*/
/*
int		FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	int		r;
	qboolean	sync;

	sync = qfalse;

	switch( mode ) {
	case FS_READ:
		r = FS_FOpenFileRead( qpath, f );
		break;
	case FS_READ_LOCK:
		r = FS_FOpenFileRead( qpath, f );
		if(fcntl(fileno(fsh[*f].handleFiles.file.o),F_SETLKW,file_lock(F_WRLCK, SEEK_SET)) == -1){
		    FS_FCloseFile(*f);
		    r = -1;
		}else{
		    fsh[*f].locked = qtrue;
		}
		break;
	case FS_WRITE:
		*f = FS_FOpenFileWrite( qpath );
		r = 0;
		if (*f == 0) {
			r = -1;
		}
		break;
	case FS_WRITE_LOCK:
		*f = FS_FOpenFileWrite( qpath );
		r = 0;
		if (*f == 0) {
			r = -1;
		}else if(fcntl(fileno(fsh[*f].handleFiles.file.o),F_SETLKW,file_lock(F_WRLCK, SEEK_SET)) == -1){
		    FS_FCloseFile(*f);
		    r = -1;
		}else{
		    fsh[*f].locked = qtrue;
		}
		break;
	case FS_APPEND_SYNC:
		sync = qtrue;
	case FS_APPEND:
		*f = FS_FOpenFileAppend( qpath );
		r = 0;
		if (*f == 0) {
			r = -1;
		}
		break;
	case FS_APPEND_LOCK:
		*f = FS_FOpenFileAppend( qpath );
		r = 0;
		if (*f == 0) {
			r = -1;
		}else if(fcntl(fileno(fsh[*f].handleFiles.file.o),F_SETLKW,file_lock(F_WRLCK, SEEK_SET)) == -1){
		    FS_FCloseFile(*f);
		    r = -1;
		}else{
		    fsh[*f].locked = qtrue;
		}
		break;
	default:
		Com_Error( ERR_FATAL, "FSH_FOpenFile: bad mode" );
		return -1;
	}

	if (!f) {
		return r;
	}

	if ( *f ) {
		fsh[*f].baseOffset = ftell(fsh[*f].handleFiles.file.o);
		fseek(fsh[*f].handleFiles.file.o,0,SEEK_END);
		fsh[*f].fileSize = ftell(fsh[*f].handleFiles.file.o);
		fseek(fsh[*f].handleFiles.file.o,0,SEEK_SET);
		fsh[*f].streamed = qfalse;

	}
	fsh[*f].handleSync = sync;

	return r;
}
*/
int	FS_FTell( fileHandle_t f ) {
	int pos;
		pos = ftell(fsh[f].handleFiles.file.o);
	return pos;
}

void	FS_Flush( fileHandle_t f ) {
	fflush(fsh[f].handleFiles.file.o);
}


/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile( void *buffer ) {

	if ( !buffer ) {
		Com_Error( ERR_FATAL, "FS_FreeFile( NULL )" );
	}
	fs_loadStack --;

	free( buffer );
}



/*
=================
FS_ReadLine
Custom function that only reads single lines
Properly handles line reads
=================
*/

int FS_ReadLine( void *buffer, int len, fileHandle_t f ) {
	char		*read;
	char		*buf;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	if ( !f ) {
		return 0;
	}

	buf = buffer;
        *buf = 0;
	read = fgets (buf, len, fsh[f].handleFiles.file.o);
	if (read == NULL) {	//Error

		if(feof(fsh[f].handleFiles.file.o)) return 0;
		Com_Error (ERR_FATAL, "FS_ReadLine: couldn't read");
		return -1;
	}
	return 1;
}



/*
===========
FS_SV_FOpenFileWrite

===========
*/
fileHandle_t FS_SV_FOpenFileWrite( const char *filename ) {
	char *ospath;
	fileHandle_t	f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	ospath = FS_BuildOSPath( fs_homepath->string, filename, "" );
	ospath[strlen(ospath)-1] = '\0';

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	if ( fs_debug->boolean ) {
		Com_Printf( "FS_SV_FOpenFileWrite: %s\n", ospath );
	}

	if( FS_CreatePath( ospath ) ) {
		return 0;
	}

	fsh[f].handleFiles.file.o = fopen( ospath, "wb" );

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}

/*
===========
FS_SV_FOpenFileRead
search for a file somewhere below the home path, base path or cd path
we search in that order, matching FS_SV_FOpenFileRead order
===========
*/
int FS_SV_FOpenFileRead( const char *filename, fileHandle_t *fp ) {
	char *ospath;
	fileHandle_t	f = 0;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	// search homepath
	ospath = FS_BuildOSPath( fs_homepath->string, filename, "" );
	// remove trailing slash
	ospath[strlen(ospath)-1] = '\0';

	if ( fs_debug->boolean ) {
		Com_Printf( "FS_SV_FOpenFileRead (fs_homepath): %s\n", ospath );
	}

	fsh[f].handleFiles.file.o = fopen( ospath, "rb" );
	fsh[f].handleSync = qfalse;

        if (!fsh[f].handleFiles.file.o){
        // NOTE TTimo on non *nix systems, fs_homepath == fs_basepath, might want to avoid
            if (Q_stricmp(fs_homepath->string,fs_basepath->string)){
              // search basepath
                ospath = FS_BuildOSPath( fs_basepath->string, filename, "" );
                ospath[strlen(ospath)-1] = '\0';
                if ( fs_debug->boolean ){
                    Com_Printf( "FS_SV_FOpenFileRead (fs_basepath): %s\n", ospath );
                }

                fsh[f].handleFiles.file.o = fopen( ospath, "rb" );
                fsh[f].handleSync = qfalse;

            }
        }

        if ( !fsh[f].handleFiles.file.o ){
            f = 0;
        }

	*fp = f;
	if (f) {
		return FS_filelength(f);
	}
	return 0;
}


/*
===========
FS_SV_FOpenFileAppend

===========
*/
fileHandle_t FS_SV_FOpenFileAppend( const char *filename ) {
	char			*ospath;
	fileHandle_t	f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	ospath = FS_BuildOSPath( fs_homepath->string, filename, "" );
	ospath[strlen(ospath)-1] = '\0';

	if ( fs_debug->boolean ) {
		Com_Printf( "FS_SV_FOpenFileAppend (fs_homepath): %s\n", ospath );
	}

	if( FS_CreatePath( ospath ) ) {
		return 0;
	}

	fsh[f].handleFiles.file.o = fopen( ospath, "ab" );
	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}


/*
=================
FS_Read

Properly handles partial reads
=================
*/


int FS_Read( void *buffer, int len, fileHandle_t f ) {
	int		block, remaining;
	int		read;
	byte	*buf;
	int		tries;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !f ) {
		return 0;
	}

	buf = (byte *)buffer;

	if (fsh[f].zipFile == qfalse) {
		remaining = len;
		tries = 0;
		while (remaining) {
			block = remaining;
			read = fread (buf, 1, block, fsh[f].handleFiles.file.o);
			if (read == 0)
			{
				// we might have been trying to read from a CD, which
				// sometimes returns a 0 read on windows
				if (!tries) {
					tries = 1;
				} else {
					return len-remaining;	//Com_Error (ERR_FATAL, "FS_Read: 0 bytes read");
				}
			}

			if (read == -1) {
				Com_Error (ERR_FATAL, "FS_Read: -1 bytes read");
			}

			remaining -= read;
			buf += read;
		}
		return len;
	} else {
		return unzReadCurrentFile(fsh[f].handleFiles.file.z, buffer, len);
	}
}




int FS_Read2( void *buffer, int len, fileHandle_t f ) {
	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !f ) {
		return 0;
	}
	if (fsh[f].streamed) {
		int r;
		fsh[f].streamed = qfalse;
		r = FS_Read( buffer, len, f );
		fsh[f].streamed = qtrue;
		return r;
	} else {
		return FS_Read( buffer, len, f);
	}
}


/*
=================
FS_Write

Properly handles partial writes
=================
*/
int FS_Write( const void *buffer, int len, fileHandle_t h ) {
	int		block, remaining;
	int		written;
	byte	*buf;
	int		tries;
	FILE	*f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !h ) {
		return 0;
	}

	f = FS_FileForHandle(h);
	buf = (byte *)buffer;

	remaining = len;
	tries = 0;
	while (remaining) {
		block = remaining;
		written = fwrite (buf, 1, block, f);
		if (written == 0) {
			if (!tries) {
				tries = 1;
			} else {
				Com_Printf( "FS_Write: 0 bytes written\n" );
				return 0;
			}
		}

		if (written == -1) {
			Com_Printf( "FS_Write: -1 bytes written\n" );
			return 0;
		}

		remaining -= written;
		buf += written;
	}
	if ( fsh[h].handleSync ) {
		fflush( f );
	}

	return len;
}




/*
============
FS_ReadFile

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_ReadFile( const char *qpath, void **buffer ) {
	fileHandle_t	h;
	byte*			buf;
	int			len;

	if ( !qpath || !qpath[0] ) {
		Com_Error( ERR_FATAL, "FS_ReadFile with empty name\n" );
	}

	buf = NULL;	// quiet compiler warning

	// look for it in the filesystem or pack files
	len = FS_FOpenFileRead( qpath, &h );
	if ( h == 0 ) {
		if ( buffer ) {
			*buffer = NULL;
		}
		return -1;
	}
	
	if ( !buffer ) {
		FS_FCloseFile( h);
		return len;
	}

	fs_loadStack ++;

	buf = malloc(len+1);
	*buffer = buf;

	FS_Read (buf, len, h);

	// guarantee that it will have a trailing 0 for string operations
	buf[len] = 0;
	FS_FCloseFile( h );
	return len;
}


/*
============
FS_WriteFile

Filename are reletive to the quake search path
============
*/
void FS_WriteFile( const char *qpath, const void *buffer, int size ) {
	fileHandle_t f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !qpath || !buffer ) {
		Com_Error( ERR_FATAL, "FS_WriteFile: NULL parameter" );
	}

	f = FS_FOpenFileWrite( qpath );
	if ( !f ) {
		Com_Printf( "Failed to open %s\n", qpath );
		return;
	}

	FS_Write( buffer, size, f );

	FS_FCloseFile( f );
}

/*
============
FS_SV_WriteFile

Filename are reletive to the quake search path
============
*/
void FS_SV_WriteFile( const char *qpath, const void *buffer, int size ) {
	fileHandle_t f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !qpath || !buffer ) {
		Com_Error( ERR_FATAL, "FS_WriteFile: NULL parameter" );
	}

	f = FS_SV_FOpenFileWrite( qpath );
	if ( !f ) {
		Com_Printf( "Failed to open %s\n", qpath );
		return;
	}

	FS_Write( buffer, size, f );

	FS_FCloseFile( f );
}

#define MAXPRINTMSG 1024

void QDECL FS_Printf( fileHandle_t h, const char *fmt, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	FS_Write(msg, strlen(msg), h);
}





#define PK3_SEEK_BUFFER_SIZE 65536

/*
=================
FS_Seek

=================
*/
int FS_Seek( fileHandle_t f, long offset, int origin ) {
	int		_origin;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
		return -1;
	}

	if (fsh[f].streamed) {
		fsh[f].streamed = qfalse;
	 	FS_Seek( f, offset, origin );
		fsh[f].streamed = qtrue;
	}

	if (fsh[f].zipFile == qtrue) {
		//FIXME: this is incomplete and really, really
		//crappy (but better than what was here before)
		byte	buffer[PK3_SEEK_BUFFER_SIZE];
		int		remainder = offset;

		if( offset < 0 || origin == FS_SEEK_END ) {
			Com_Error( ERR_FATAL, "Negative offsets and FS_SEEK_END not implemented "
					"for FS_Seek on pk3 file contents" );
			return -1;
		}

		switch( origin ) {
			case FS_SEEK_SET:
				unzSetOffset(fsh[f].handleFiles.file.z, fsh[f].zipFilePos);
				unzOpenCurrentFile(fsh[f].handleFiles.file.z);
				//fallthrough

			case FS_SEEK_CUR:
				while( remainder > PK3_SEEK_BUFFER_SIZE ) {
					FS_Read( buffer, PK3_SEEK_BUFFER_SIZE, f );
					remainder -= PK3_SEEK_BUFFER_SIZE;
				}
				FS_Read( buffer, remainder, f );
				return offset;
				break;

			default:
				Com_Error( ERR_FATAL, "Bad origin in FS_Seek" );
				return -1;
				break;
		}
	} else {
		FILE *file;
		file = FS_FileForHandle(f);
		switch( origin ) {
		case FS_SEEK_CUR:
			_origin = SEEK_CUR;
			break;
		case FS_SEEK_END:
			_origin = SEEK_END;
			break;
		case FS_SEEK_SET:
			_origin = SEEK_SET;
			break;
		default:
			_origin = SEEK_CUR;
			Com_Error( ERR_FATAL, "Bad origin in FS_Seek" );
			break;
		}
		return fseek( file, offset, _origin );
	}
}


__cdecl const char* FS_GetBasepath(){

    if(fs_basepath && *fs_basepath->string){
        return fs_basepath->string;
    }else{
        return "";
    }
}


/*
=================
FS_SV_HomeCopyFile

Copy a fully specified file from one place to another
=================
*/
void FS_SV_HomeCopyFile( char *from, char *to ) {
	FILE	*f;
	int		len;
	byte	*buf;
	char			*from_ospath, *to_ospath;

	from_ospath = FS_BuildOSPath( fs_homepath->string, from, "" );
	to_ospath = FS_BuildOSPath( fs_homepath->string, to, "" );
	from_ospath[strlen(from_ospath)-1] = '\0';
	to_ospath[strlen(to_ospath)-1] = '\0';

	if ( fs_debug->boolean ) {
		Com_Printf( "FS_SVHomeCopyFile: %s --> %s\n", from_ospath, to_ospath );
	}

	f = fopen( from_ospath, "rb" );
	if ( !f ) {
		return;
	}
	fseek (f, 0, SEEK_END);
	len = ftell (f);
	fseek (f, 0, SEEK_SET);

	// we are using direct malloc instead of Z_Malloc here, so it
	// probably won't work on a mac... Its only for developers anyway...
	buf = malloc( len );
	if (fread( buf, 1, len, f ) != len)
		Com_Error( ERR_FATAL, "Short read in FS_Copyfiles()\n" );
	fclose( f );

	if( FS_CreatePath( to_ospath ) ) {
		return;
	}

	f = fopen( to_ospath, "wb" );
	if ( !f ) {
		return;
	}
	if (fwrite( buf, 1, len, f ) != len)
		Com_Error( ERR_FATAL, "Short write in FS_Copyfiles()\n" );
	fclose( f );
	free( buf );
}


// CVE-2006-2082
// compared requested pak against the names as we built them in FS_ReferencedPakNames
qboolean FS_VerifyPak( const char *pak ) {
	char teststring[ BIG_INFO_STRING ];
	searchpath_t    *search;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if ( search->pack ) {
			Q_strncpyz( teststring, search->pack->pakGamename, sizeof( teststring ) );
			Q_strcat( teststring, sizeof( teststring ), "/" );
			Q_strcat( teststring, sizeof( teststring ), search->pack->pakBasename );
			Q_strcat( teststring, sizeof( teststring ), ".iwd" );
			if ( !Q_stricmp( teststring, pak ) ) {
				return qtrue;
			}

		}

	}
	Com_sprintf(teststring, sizeof( teststring ), "%s/mod.ff", fs_game->string);
	if ( !Q_stricmp( teststring, pak ) ){
		return qtrue;
	}

	if ( !Q_stricmpn( "usermaps/", pak, 9) ){

		if(strstr(pak, "..") || strstr(pak, ";"))
			return qfalse;
		else
			return qtrue;

	}

	return qfalse;
}

/*
============
FS_ReadFileNoInit

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/

/*
int FS_ReadFileNoInit( const char *qpath, void **buffer ) {

	byte*			buf;
	int			len, pos;
	FILE*			fh;
	char			ospath[MAX_OSPATH];
	int			block, remaining;
	int			read;

	if ( !qpath || !qpath[0] ) {
		Com_Error( ERR_FATAL, "FS_ReadFile with empty name\n" );
	}

	buf = NULL;	// quiet compiler warning
	// look for it in the filesystem or pack files

	fs_basepath = Cvar_RegisterString("fs_basepath", Sys_DefaultInstallPath(), CVAR_INIT, "Base game path");
	fs_homepath = Cvar_RegisterString("fs_homepath", Sys_DefaultInstallPath(), CVAR_INIT, "Base game path");


	Com_sprintf( ospath, sizeof( ospath ), "%s/%s", fs_homepath->string, qpath);

	fh = fopen( ospath, "rb" );

	if(!fh){
		Com_sprintf( ospath, sizeof( ospath ), "%s/%s", fs_basepath->string, qpath);
		fh = fopen( ospath, "rb" );
	}

	if(!fh){
		if ( buffer ) {
			*buffer = NULL;
		}
		return -1;
	}

	if ( !buffer ) {
		fclose( fh );
		return -1;
	}

	pos = ftell (fh);
	fseek (fh, 0, SEEK_END);
	len = ftell (fh);
	fseek (fh, pos, SEEK_SET);

	buf = malloc(len+1);
	*buffer = buf;

	remaining = len;
	while(remaining){
		block = remaining;
		read = fread (buf, 1, block, fh);
		if (read == 0) {

			buf[len] = 0;
			fclose( fh );
			return len-remaining;	//Com_Error (ERR_FATAL, "FS_Read: 0 bytes read");
		}

		if (read == -1) {
			Com_Error (ERR_FATAL, "FS_Read: -1 bytes read");
		}

		remaining -= read;
		buf += read;
	}
	// guarantee that it will have a trailing 0 for string operations
	buf[len] = 0;
	fclose( fh );
	return len;
}
*/


/*
================
FS_Startup
================
*/
/*
static void FS_Startup( const char *gameName ) {
	const char *homePath;
	cvar_t  *fs;

	Com_Printf( "----- FS_Startup -----\n" );

	fs_debug = Cvar_RegisterInt( "fs_debug", 0, 0, 2, CVAR_ARCHIVE, "Enable file system debugging informations" );
	fs_copyfiles = Cvar_RegisterBool( "fs_copyfiles", qfalse, CVAR_INIT, "Copy all used files to another location" );
	fs_cdpath = Cvar_RegisterString( "fs_cdpath", Sys_DefaultCDPath(), CVAR_INIT, "Copy all files to another location" );
	fs_basepath = Cvar_RegisterString( "fs_basepath", Sys_DefaultInstallPath(), CVAR_INIT, "Base game path" );
	fs_basegame = Cvar_RegisterString( "fs_basegame", "", CVAR_INIT, "Base game name" );
	fs_gamedirvar = Cvar_RegisterString( "fs_game", "", CVAR_INIT | CVAR_SYSTEMINFO, "Game data directory. Must be \"\" or a subdirectory of 'mods/'." );
	fs_ignoreLocalized = Cvar_RegisterBool( "fs_ignoreLocalized", qfalse, CVAR_INIT, "Ignore localized files" );



	homePath = Sys_DefaultHomePath();
	if ( !homePath || !homePath[0] ) {
		homePath = fs_basepath->string;
	}
	fs_homepath = Cvar_RegisterString( "fs_homepath", homePath, CVAR_INIT, "Game home path" );

	fs_restrict = Cvar_RegisterBool( "fs_restrict", qfalse, CVAR_INIT, "Enable demo filesystem" );

	fs_usedevdir = Cvar_RegisterBool( "fs_usedevdir", qfalse, CVAR_INIT, "Use development directories" );














	// add search path elements in reverse priority order
	if ( fs_cdpath->string[0] ) {
		FS_AddGameDirectory( fs_cdpath->string, gameName );
	}
	if ( fs_basepath->string[0] ) {
		FS_AddGameDirectory( fs_basepath->string, gameName );
	}
	// fs_homepath is somewhat particular to *nix systems, only add if relevant
	// NOTE: same filtering below for mods and basegame
	if ( fs_basepath->string[0] && Q_stricmp( fs_homepath->string,fs_basepath->string ) ) {
		FS_AddGameDirectory( fs_homepath->string, gameName );
	}

	// check for additional base game so mods can be based upon other mods
	if ( fs_basegame->string[0] && !Q_stricmp( gameName, BASEGAME ) && Q_stricmp( fs_basegame->string, gameName ) ) {
		if ( fs_cdpath->string[0] ) {
			FS_AddGameDirectory( fs_cdpath->string, fs_basegame->string );
		}
		if ( fs_basepath->string[0] ) {
			FS_AddGameDirectory( fs_basepath->string, fs_basegame->string );
		}
		if ( fs_homepath->string[0] && Q_stricmp( fs_homepath->string,fs_basepath->string ) ) {
			FS_AddGameDirectory( fs_homepath->string, fs_basegame->string );
		}
	}

	// check for additional game folder for mods
	if ( fs_gamedirvar->string[0] && !Q_stricmp( gameName, BASEGAME ) && Q_stricmp( fs_gamedirvar->string, gameName ) ) {
		if ( fs_cdpath->string[0] ) {
			FS_AddGameDirectory( fs_cdpath->string, fs_gamedirvar->string );
		}
		if ( fs_basepath->string[0] ) {
			FS_AddGameDirectory( fs_basepath->string, fs_gamedirvar->string );
		}
		if ( fs_homepath->string[0] && Q_stricmp( fs_homepath->string,fs_basepath->string ) ) {
			FS_AddGameDirectory( fs_homepath->string, fs_gamedirvar->string );
		}
	}

	Com_ReadCDKey( BASEGAME );
	fs = Cvar_Get( "fs_game", "", CVAR_INIT | CVAR_SYSTEMINFO );
	if ( fs && fs->string[0] != 0 ) {
		Com_AppendCDKey( fs->string );
	}

	// add our commands
	Cmd_AddCommand( "path", FS_Path_f );
	Cmd_AddCommand( "dir", FS_Dir_f );
	Cmd_AddCommand( "fdir", FS_NewDir_f );
	Cmd_AddCommand( "touchFile", FS_TouchFile_f );

	// show_bug.cgi?id=506
	// reorder the pure pk3 files according to server order
	FS_ReorderPurePaks();

	// print the current search paths
	FS_Path_f();

	fs_gamedirvar->modified = qfalse; // We just loaded, it's not modified

	Com_Printf( "----------------------\n" );

#ifdef FS_MISSING
	if ( missingFiles == NULL ) {
		missingFiles = fopen( "\\missing.txt", "ab" );
	}
#endif
	Com_Printf( "%d files in pk3 files\n", fs_packFiles );
}
*/
