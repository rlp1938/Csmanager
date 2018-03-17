/*    csmanager.c
 *
 * Copyright 2018 Robert L (Bob) Parker rlp1938@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
**/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <getopt.h>
#include <ctype.h>
#include <limits.h>
#include <linux/limits.h>
#include <libgen.h>
#include <errno.h>
// typdefs/structs here.
typedef struct oper_t {
	char *dirname;		// source dir to be synced.
	char *filname;		// file listing source dirs to sync.
	char *cloud_target;	// eg Dropbox or Nextcloud;
	char *dotdirs_dir;	// the dir to send dot dir contents to.
	char **excludes;	// list of dirs to exclude eg $HOME/Dropbox etc.
	int do_master_dir;	// a dir, not a file listing dirs.
	size_t len2target;	// byte count to (for example) $HOME/Nextcloud
} oper_t;
#include "str.h"
#include "dirs.h"
#include "files.h"
#include "gopt.h"
static oper_t
*init_operations(char *srcdir, options_t *opts);
static char
*check_args(char **argv);
static char
**gen_dirslist(const char *dirname, int dotsornot, char **excl_list);
static char
**excl_list(const char *prname);
static char
**getfromfile(oper_t *ops);
static void
processlist(char **synclist, oper_t *ops, int dotsornot);
static char
*build_path(char *s1, char *s2, char *s3);

int main(int argc, char **argv)
{
	options_t opts = process_options(argc, argv);	// options
	char *srcdir = check_args(argv);
	oper_t *operations = init_operations(srcdir, &opts);
	char **synclist;
	if (operations->filname) { // work from list of dirs given.
		synclist = getfromfile(operations);
		processlist(synclist, operations, 0);
	} else { // work from source dir.
		char **exlist = excl_list("csmanager");
		printf("%s\n", "====================");
		synclist = gen_dirslist(operations->dirname, 0, exlist);
		processlist(synclist, operations, 0);
		printf("%s\n", "====================");
		synclist = gen_dirslist(operations->dirname, 1, exlist);
		processlist(synclist, operations, 1);
		printf("%s\n", "====================");
	}


	return 0;
}//main()

char
*check_args(char **argv)
{ /* Check validity of non-option arg */
	char buf[PATH_MAX] = {0};
	char *p;
	if (!argv[optind]) { // default to $HOME
		p = realpath(getenv("HOME"), NULL);
		strcpy(buf, p);
	} else if ((p = realpath(argv[optind], NULL))) {
		if (!p) { // must exist
			perror(p);
			exit(EXIT_FAILURE);
		}
		if (!exists_dir(p)) {
			fprintf(stderr, "%s is not a dir.\n", argv[optind]);
			exit(EXIT_FAILURE);
		}
	}
	return p;
} // check_args()

char
**gen_dirslist(const char *dirname, int dotsornot, char **excl_list)
{/* get the dir names under dirname selecting or avoiding dot dirs,
  * then turn the data into an array of C strings.
*/
	mdata *md = init_mdata();
	const size_t meminc = 1024 * 1024;	// 1 meg is ok for this job.
	DIR *thedir = dopendir(dirname);
	struct dirent *de;
	while ((de = readdir(thedir))) {
		if(strcmp("..", de->d_name) == 0) continue;
		if(strcmp(".", de->d_name) == 0) continue;
		if(de->d_type != DT_DIR) continue;
		if (dotsornot) {
			if (de->d_name[0] != '.') continue;
		} else {
			if (de->d_name[0] == '.') continue;
		}
		char buf[PATH_MAX];
		strcpy(buf, dirname);
		strjoin(buf, '/', de->d_name, PATH_MAX);
		if(instrlist(buf, excl_list)) continue;
		meminsert(buf, md, meminc);
	}
	doclosedir(thedir);
	char **result = memblocktoarray(md, 0);
	free_mdata(md);
	return result;
} // gen_dirslist()

char
**excl_list(const char *prname)
{/* Return list of dirs to exclude from processing. If the excludes file
  * does not exist, create it with some reasonable default values.
*/
	char fpath[PATH_MAX] = {0};
	char *home = getenv("HOME");
	sprintf(fpath, "%s/.config/%s/excl.lst", home, prname);
	if (!exists_file(fpath))
	{	// create it
		char *cp = strstr(fpath, "excl.lst");
		*cp = 0;	// does the dir exist? If not create it.
		if (!exists_dir(fpath)) newdir(fpath, 0);
		*cp = 'e';	// restore the filename to fpath.
		FILE *fpo = dofopen(fpath, "w");
		fprintf(fpo, "%s/%s\n", home, "Dropbox");
		fprintf(fpo, "%s/%s\n", home, "Nextcloud");
		dofclose(fpo);
		sync();
	}
	return getfile_str(fpath);
} // excl_list()

oper_t
*init_operations(char *srcdir, options_t *opts)
{ /* setup the variables to be operated on thru the run. */
	oper_t *operations = xmalloc(sizeof (oper_t));
	memset(operations, 0, sizeof(oper_t));
	// need to change into srcdir, by default maybe $HOME
	int res = chdir(srcdir);
	if (res == -1) {
		perror(srcdir);
		exit(EXIT_FAILURE);
	}
	operations->dirname = srcdir;
	// Need to deal with target before anything else.
	size_t len2target;
	char wrkbuf[PATH_MAX];
	char *stop;
	if (opts->cloud_target) { // it's on the heap.
		operations->cloud_target =
				build_path(srcdir, opts->cloud_target, NULL);
		strcpy(wrkbuf, operations->cloud_target);
		stop = strstr(wrkbuf, opts->cloud_target);
		free(opts->cloud_target);
	} else { // use the default.
		operations->cloud_target =
						build_path(srcdir, "Nextcloud", NULL);
		strcpy(wrkbuf, operations->cloud_target);
		stop = strstr(wrkbuf, "Nextcloud");
	}
	*stop = '\0';
	len2target = strlen(wrkbuf) - 1;	// backup to '/' before target.
	operations->len2target = len2target;
	// Now the dotfiles dir.
	if (opts->dot_files_dir) {
		operations->dotdirs_dir =
		build_path(operations->cloud_target, opts->dot_files_dir, NULL);
		free(opts->dot_files_dir);	// on the heap from options proc.
	} else {
		operations->dotdirs_dir =
				build_path(operations->cloud_target, "Dotty", NULL);
	}
	// deal with the source.
	if (opts->dirs_from) {
		operations->filname = build_path(srcdir, opts->dirs_from, NULL);
		if (!exists_file(operations->filname)) {
			fprintf(stderr, "No such file: %s\n", operations->filname);
			exit(EXIT_FAILURE);
		}
	}
	return operations;
} // init_operations()

char
**getfromfile(oper_t *ops)
{ /* Read the given file and generate the list of dirs from it. */
	mdata *mydat = readfile(ops->filname, 1, 0);
	size_t count = memlinestostr(mydat);
	char **list = xmalloc((count+1) * sizeof(char *));
	list[count] = (char *)NULL;
	char *cp = mydat->fro;
	size_t i;
	for (i = 0; i < count; i++) {
		if (cp[0] == '/') { // absolute path specified.
			list[i] = xstrdup(cp);
		} else {
			char line[PATH_MAX];
		/* dirs named in file must be relative to named source dir. */
			strcpy(line, ops->dirname);
			strjoin(line, '/', cp, PATH_MAX);
			list[i] = xstrdup(line);
		}
		if (!exists_dir(list[i])) {
			fprintf(stderr, "No such dir: %s\n", list[i]);
			exit(EXIT_FAILURE);
		}
		cp += strlen(cp) + 1;
	}
	free(mydat->fro);
	return list;
} // getfromfile()

void
processlist(char **synclist, oper_t *ops, int dotsornot)
{ /* From the list of absolute paths in synclist, sync to the cloud
   * target.
*/
	size_t i;
	size_t hlen = ops->len2target;
	for (i = 0; synclist[i]; i++) {
		char buf[PATH_MAX];
		char command[PATH_MAX];
		strcpy(buf, synclist[i]);
		buf[hlen] = 0;
		strcpy(buf, ops->cloud_target);
		if (dotsornot) {
			strcpy(buf, ops->dotdirs_dir);
			if (!exists_dir(buf)) {
				int res = mkdir(buf, 0775);
				if (res) {
					perror(buf);
					exit(EXIT_FAILURE);
				}
			}
		}
		// For every $HOME/somedir, create $HOME/Nextcloud/somedir
		char wrk[PATH_MAX];
		strcpy(wrk, synclist[i]);
		char *cp = &wrk[hlen+1];
		strjoin(buf, '/', cp, PATH_MAX);
		if (!exists_dir(buf)) {
			int res = mkdir(buf, 0775);
			if (res) {
				perror(buf);
				exit(EXIT_FAILURE);
			}
		}
		strcpy(command, "/usr/local/bin/synclink -v");
		strjoin(command, ' ', synclist[i], PATH_MAX);
		strjoin(command, ' ', buf, PATH_MAX);
		printf("%s\n", command);
		xsystem(command, 0);
	}
} // processlist()

char
*build_path(char *s1, char *s2, char *s3)
{ /* Assemble a path of names separated by '/', s3 may be NULL. */
	char buf[PATH_MAX];
	strcpy(buf, s1);
	strjoin(buf, '/', s2, PATH_MAX);
	if (s3) {
		strjoin(buf, '/', s3, PATH_MAX);
	}
	return xstrdup(buf);
} // build_path()
