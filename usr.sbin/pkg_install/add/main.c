#ifndef lint
static const char rcsid[] =
	"$Id: main.c,v 1.22 1999/01/26 22:31:23 billf Exp $";
#endif

/*
 *
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * This is the add module.
 *
 */

#include <err.h>
#include <sys/param.h>
#include <objformat.h>
#include "lib.h"
#include "add.h"

static char Options[] = "hvIRfnrp:SMt:";

char	*Prefix		= NULL;
Boolean	NoInstall	= FALSE;
Boolean	NoRecord	= FALSE;
Boolean Remote		= FALSE;

char	*Mode		= NULL;
char	*Owner		= NULL;
char	*Group		= NULL;
char	*PkgName	= NULL;
char	*Directory	= NULL;
char	FirstPen[FILENAME_MAX];
add_mode_t AddMode	= NORMAL;

#define MAX_PKGS	20
char	pkgnames[MAX_PKGS][MAXPATHLEN];
char	*pkgs[MAX_PKGS];

static char *getpackagesite(char *);
int getosreldate(void);

static void usage __P((void));

int
main(int argc, char **argv)
{
    int ch, err;
    char **start;
    char *cp;

    char *remotepkg = NULL, *ptr;
    static char binformat[1024];
    static char packageroot[MAXPATHLEN] = "ftp://ftp.FreeBSD.org/pub/FreeBSD/ports/";

    start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1) {
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'p':
	    Prefix = optarg;
	    break;

	case 'I':
	    NoInstall = TRUE;
	    break;

	case 'R':
	    NoRecord = TRUE;
	    break;

	case 'f':
	    Force = TRUE;
	    break;

	case 'n':
	    Fake = TRUE;
	    Verbose = TRUE;
	    break;

	case 'r':
	    Remote = TRUE;
	    break;

	case 't':
	    strcpy(FirstPen, optarg);
	    break;

	case 'S':
	    AddMode = SLAVE;
	    break;

	case 'M':
	    AddMode = MASTER;
	    break;

	case 'h':
	case '?':
	default:
	    usage();
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    if (argc > MAX_PKGS) {
	warnx("too many packages (max %d)", MAX_PKGS);
	return(1);
    }

    if (AddMode != SLAVE) {
	for (ch = 0; ch < MAX_PKGS; pkgs[ch++] = NULL) ;

	/* Get all the remaining package names, if any */
	for (ch = 0; *argv; ch++, argv++) {
    	    if (Remote) {
		if (getenv("PACKAGESITE") == NULL) {
		   getobjformat(binformat, sizeof(binformat), &argc, argv); 
		   strcat(packageroot, getpackagesite(binformat));
		}
		else
	    	   strcpy(packageroot, (getenv("PACKAGESITE")));
		remotepkg = strcat(packageroot, *argv);
		if (!((ptr = strrchr(remotepkg, '.')) && ptr[1] == 't' && 
			ptr[2] == 'g' && ptr[3] == 'z' && !ptr[4]))
		   strcat(remotepkg, ".tgz");
    	    }
	    if (!strcmp(*argv, "-"))	/* stdin? */
		pkgs[ch] = "-";
	    else if (isURL(*argv))	/* preserve URLs */
		pkgs[ch] = strcpy(pkgnames[ch], *argv);
	    else if ((Remote) && isURL(remotepkg))
		pkgs[ch] = strcpy(pkgnames[ch], remotepkg);
	    else {			/* expand all pathnames to fullnames */
		if (fexists(*argv)) /* refers to a file directly */
		    pkgs[ch] = realpath(*argv, pkgnames[ch]);
		else {		/* look for the file in the expected places */
		    if (!(cp = fileFindByPath(NULL, *argv)))
			warnx("can't find package '%s'", *argv);
		    else
			pkgs[ch] = strcpy(pkgnames[ch], cp);
		}
	    }
	}
    }
    /* If no packages, yelp */
    else if (!ch) {
	warnx("missing package name(s)");
	usage();
    }
    else if (ch > 1 && AddMode == MASTER) {
	warnx("only one package name may be specified with master mode");
	usage();
    }
    /* Make sure the sub-execs we invoke get found */
    setenv("PATH", "/sbin:/usr/sbin:/bin:/usr/bin", 1);

    /* Set a reasonable umask */
    umask(022);

    if ((err = pkg_perform(pkgs)) != 0) {
	if (Verbose)
	    warnx("%d package addition(s) failed", err);
	return err;
    }
    else
	return 0;
}

static char *
getpackagesite(char binform[1024])
{
    int reldate;

    reldate = getosreldate();

    if (reldate == 300005)
  	return "i386/packages-3.0/";
    else if (300004 > reldate && reldate >= 300000)
	return "i386/packages-3.0-aout/Latest/" ;
    else if (300004 < reldate) 
	return !strcmp(binform, "elf") ? "i386/packages-3-stable/Latest/" :
		"i386/packages-3.0-aout/Latest/";

    return("");

}

static void
usage()
{
    fprintf(stderr, "%s\n%s\n",
		"usage: pkg_add [-vInrfRMS] [-t template] [-p prefix]",
		"               pkg-name [pkg-name ...]");
    exit(1);
}
