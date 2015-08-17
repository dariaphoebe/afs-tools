#define true 1
#define false 0

/* This program was originally written by Dave Nichols, and was modifed by Mike Kazar to expunge a hierarchy, instead of copy it. */

#include <stdio.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef _POSIX_VERSION
#include <dirent.h>
#define direct dirent
#else
#include <sys/dir.h>
#endif
#if !defined(NeXT) && !(defined(sun) && defined(__SVR4)) && !defined(darwin) && !defined(__APPLE__)
#include <a.out.h>
#endif

int Zap(char *file, int level, int isDir);

#if (!defined(__linux__) || !defined(__GLIBC__) || (__GLIBC__ < 2)) && !defined(__APPLE__)
extern int  errno, sys_nerr;
extern char *sys_errlist[];
#endif

char *prefix, *dircmd, *regcmd, *filecmd, *linkcmd, *mntcmd;
int doit, showit, viceII;

int
main(int argc, char *argv[])
{
    char *root;
    char start[MAXPATHLEN];
    int i;
    int code = 0;
    doit = 1;
    viceII=0;
    showit = 1;
    root = prefix = dircmd = regcmd = filecmd = mntcmd = linkcmd = "";
    for (i=1;i<argc;i++){
	if (!strcmp(argv[i], "-d")) dircmd = argv[++i];
	else if (!strcmp(argv[i], "-f")) filecmd = argv[++i];
	else if (!strcmp(argv[i], "-l")) linkcmd = argv[++i];
	else if (!strcmp(argv[i], "-b")) regcmd = argv[++i];
	else if (!strcmp(argv[i], "-m")) mntcmd = argv[++i], viceII = 1;
	else if (!strcmp(argv[i], "-2")) viceII = 1;
	else if (!strcmp(argv[i], "-p")) prefix = argv[++i];
	else if (!strcmp(argv[i], "-n")) doit = 0, showit = 1;
	else if (!strcmp(argv[i], "-q")) doit = 1, showit = 0;
	else if (argv[i][0] == '-'){
	    printf("Bogus switch %s, try\n",argv[i]);
	    printf("-f 'command to run on each file'\n");
	    printf("-d 'command to run on each dir'\n");
	    printf("-m 'command to run on each mountpoint'\n");
	    printf("-2 use vice II dir id hack, and apply filecmd to links\n");
	    printf("-l 'command to run on each link'\n");
	    printf("-b 'command to run on each file or link'\n");
	    printf("-p prefix string\n");
	    printf("-n just show the command\n");
	    printf("-q don't show, but do the command\n");
	    printf("%%d is the dir name, %%f the file name, caps -> no prefix string\n");
	    exit(1);
	}
	else root = argv[i];
    }
    if (regcmd[0] != 0)
	linkcmd = filecmd = regcmd;
    if (root[0] == 0)
	getcwd(start, MAXPATHLEN);
    else
	strcpy(start, root);
    code = Zap(start, 0, 1);
    exit(code); /* explicitly trying to return status */
}

char *
strrep(char *src, char *patt, char *with) {
    char *retstr, *instr, *tmp;   
    int pattlen, withlen, headlen, count;    

    if (!src)
        return NULL;
    if (!patt || !(pattlen = strlen(patt)))
        return NULL;
    if (!(instr = strstr(src, patt))) 
        return NULL;
    if (!with)
        with = "";
    withlen = strlen(with);

    for (count = 0; (tmp = strstr(instr, patt)); ++count)
        instr = tmp + pattlen;

    tmp = retstr = malloc(strlen(src) + (withlen - pattlen) * count + 1);

    if (!retstr)
        return NULL;

    while (count--) {
        instr = strstr(src, patt);
        headlen = instr - src;
        tmp = strncpy(tmp, src, headlen) + headlen;
        tmp = strcpy(tmp, with) + withlen;
        src += headlen + pattlen;
    }
    strcpy(tmp, src);
    return retstr;
}

int
quotestrcpy(char *dst, char *src, int n)
{
    char *tmp = strrep(src, "\'", "\'\\\'\'");
    if (tmp)  {
	return snprintf(dst, n, "%c%s%c", '\'', tmp, '\'');
	free(tmp);
    } else {
	return snprintf(dst, n, "%c%s%c", '\'', src, '\'');
    }
}

int
docmd(char *cmdstring, char *dname, char *fname, char *npdname, char *npfname)
{
    char cmdbuffer[4096];
    char tc, *np;
    int n, code = 0;

    if (*cmdstring == '\0')
	return(0);  /*Didn't used to have a return param*/
    n = strlen(cmdstring);
    np = cmdbuffer;
    while ((tc = *cmdstring++)) {
	if (tc != '%')
	    *np++ = tc;
	else {
	    tc = *cmdstring++;
	    if (tc == '%')
		*np++ = tc;
	    else
		if (tc == 'f')
		    np += quotestrcpy(np, fname, 4096-(np-cmdbuffer));
		else if (tc == 'd')
		    np += quotestrcpy(np, dname, 4096-(np-cmdbuffer));
		else if (tc == 'D')
		    np += quotestrcpy(np, npdname, 4096-(np-cmdbuffer));
		else if (tc == 'F')
		    np += quotestrcpy(np, npfname, 4096-(np-cmdbuffer));
		else {
		    printf("Bogus %%char '%c'.\n", tc);
		    exit(1);
		}
	}
    }
    *np++ = 0;	/* Terminating null. */
    if (showit) printf("%s\n", cmdbuffer);
    if (doit){
	code = system(cmdbuffer);
	return(code);
    }
    else
	return(0);
}

int
Zap(char *file, int level, int isDir)
{
    int i, ismntpt = 0;
    int n, seenalpha, code = 0;
    char dname[MAXPATHLEN], *npfname, *npdname;
    struct stat s;

    strcpy(dname, file);
    n = strlen(dname);
    seenalpha = 0;
    for(i=n-1;i>=0;i--){
	if (dname[i] != '/')
	    seenalpha = 1;
	else
	    if (seenalpha && dname[i] == '/') {
		dname[i] = 0;
		break;
	    }
    }

    n = strlen(prefix);
    npfname = file+n;
    npdname = dname+n;

    if (viceII) {
	if (isDir)
	    s.st_mode = S_IFDIR;
	else {
	    if (lstat(file, &s) < 0){
		printf("Can't find %s\n",file);
		return(2)/*Didn't used to return a param*/;
	    }
	    if ((s.st_mode & S_IFMT) == S_IFDIR)
		ismntpt = 1;
	    s.st_mode = S_IFREG;
	}
    } else if (lstat(file, &s) < 0){
	printf("Can't find %s\n",file);
	return(2)/*Didn't used to return a param*/;
    }

    if (ismntpt) {
	/* Mountpoint. */
	code = docmd(mntcmd, dname, file, npdname, npfname);
    } else if ((s.st_mode & S_IFMT) == S_IFREG){
	/* Regular file. */
	code = docmd(filecmd, dname, file, npdname, npfname);
    } else if ((s.st_mode & S_IFMT) == S_IFLNK){
	/* Link. */
	code = docmd(linkcmd, dname, file, npdname, npfname);
    } else if ((s.st_mode & S_IFMT) == S_IFDIR){
	/* Zap Directory */
	DIR * dir;
	struct direct *d;
	char  f[MAXPATHLEN];
	char *p;
	
	code = docmd(dircmd, dname, file, npdname, npfname);
	
	/* Now recurse. */
	strcpy(f, file);
	p = f + strlen(f);
	if (p == f || p[-1] != '/') *p++ = '/';
	
	if ((dir = opendir(file)) == NULL){
	    printf("Couldn't open %s\n", file);
	    return(2)/*Didn't used to return a param*/;
	}
	while ((d = readdir(dir)) != NULL){
	    if (strcmp(d->d_name, ".") == 0
		|| strcmp(d->d_name, "..") == 0)
		continue;
	    strcpy(p, d->d_name);
	    Zap(f, level + 1, d->d_ino&1);
	}
	closedir(dir);
    }
    return(code);
}
