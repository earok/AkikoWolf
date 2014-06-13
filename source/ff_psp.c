#include "include/ff.h"

#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dosasl.h>
#include <clib/dos_protos.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <dos.h>
/*#include <unistd.h>
#ifdef _PSP
#include <sys/dirent.h>
#else
#include <dirent.h>
#endif
#ifdef USE_DMALLOC
#include<dmalloc.h>
#endif
   */
static char **dirfname;
static int dirpos;
static int dirsize;

static int wildcard(const char *name, const char *match)
{
	int i;
	int max;
	max=strlen(name);
	if(strlen(match)!=max) {
		return 0;
	}
	for(i=0;i<max;i++) {
		char a,b;
		a=name[i];
		b=match[i];
		if(a>='a' && a<='z') a^=32;
		if(b>='a' && b<='z') b^=32;
		if(b=='?' || a==b) {
			// continue
		} else {
			return 0;
		}
	}
	return 1;
}
char* strdup(const char* p)
{
    char* np = (char*)malloc(strlen(p)+1);
    return np ? strcpy(np, p) : np;
}

int findfirst(const char *pathname, struct FileInfoBlock *info, int attrib) {
char *match;
int matches=0;
match=strdup(pathname);
//printf("Looking for '%s'\n",pathname);
if (dfind(info,match,attrib)==0)
{
matches++;
//printf("Name = %s\n", info->fib_FileName);
/*while (!dnext(info))
{
matches++;
//printf("Name = %s\n", info->fib_FileName);
}*/
//printf("Got %d matches\n",matches);
	return 0;
	}
	else
	{ 
	//printf("Got %d matches\n",matches);
	return 1;
	}

}

int findnext(struct FileInfoBlock *info) {
   if (dnext(info)==0)
   {
   //printf("Name = %s\n", info->fib_FileName);
   	return 0;
   	}
   	else
   	return 1;
 /* if (dirpos<dirsize) {
    strcpy(ffblk->ff_name,dirfname[dirpos++]);
    return 0;
  }
  return 1;*/

}

void resetinactivity(void) {
	//User::ResetInactivityTime();
}
