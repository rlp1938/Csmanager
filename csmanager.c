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
typedef struct ops_t {
	char *dirname;		// the name of a dir or a file listing dirs.
	char *cloud_target;	// eg Dropbox or Nextcloud;
	int do_master_dir;	// a dir, not a file listing dirs.
	int do_dot_dirs;	// 0 is avoid dot dirs, 1 is dot dirs only.
	time_t dd_updated;	// last time we processed dot dirs.
	char *dotfiles_dir;	// the dir to send dot files tarballs to.
	char *timestamp;	// path to the timestamp for this process.
	char **files_list;	// list of dirs to process.
	char **excludes;	// list of dirs to exclude eg $HOME/Dropbox etc.
} ops_t;
#include "str.h"
#include "dirs.h"
#include "files.h"
#include "gopt.h"

static int
nc_recursedir(ops_t *operation, const char *dirname);
static char
*timestamp_name(void);
static void
process_dotdirs(ops_t *operation);
static void
check_args(ops_t *ops, char **argv, int optind, options_t *opts);
static char
**make_dirslist(ops_t *operation);
static char
**gen_dirslist(const char *dirname, int dotsornot, char **excl_list);
static char
**gen_fileslist(const char *dirname, char **excl_list);
static char
**gen_listfromfile(const char *filename);
static char
**excl_list(const char *prname);
static void
proc_normaldirs(ops_t *operation);
static char
*target_from_option(char *tname);
static char
*target_from_file(void);


int main(int argc, char **argv)
{
	options_t opts = process_options(argc, argv);	// options
	ops_t operation = {0};	// preparation
	check_args(&operation, argv, optind, &opts);
	operation.timestamp = timestamp_name();
	operation.excludes = excl_list("csmanager");
	operation.files_list = make_dirslist(&operation); // data gathering.
	if (operation.do_dot_dirs) {	// processing
		process_dotdirs(&operation);
	} else { /* possibly replace a sub-dir name with full path. */
		if (opts.cloud_target) operation.cloud_target =
								target_from_option(opts.cloud_target);
		else operation.cloud_target = target_from_file();
		proc_normaldirs(&operation);
	}
	// and now deal with sundry files in $HOME or chosen dir.
	gen_fileslist(operation.dirname, NULL);
	return 0;
}//main()

void check_args(ops_t *operation, char **argv, int optind,
			options_t *opts)
{ /* Check presence of and validity of non-option args */
	char buf[PATH_MAX] = {0};
	if (opts->dirs_from && argv[optind]) {
		sprintf(buf, "List file: %s and master dir %s not allowed\n",
				opts->dirs_from, argv[optind]);
	} else if (!argv[optind]) {
		strcpy(buf, "No directory to process provided.\n");
	} else if (!exists_dir(argv[optind])) {
		sprintf(buf, "%s does not exist or is not a dir.\n",
				argv[optind]);
	} else if (opts->dirs_from && opts->do_dot_files) {
		sprintf(buf, "May not process dot dirs in list of dirs: %s\n",
				opts->dirs_from);
	} else if (opts->dirs_from && !exists_file(opts->dirs_from)) {
				sprintf(buf, "%s does not exist or is not a file.\n",
						opts->dirs_from);
	}

	if (strlen(buf)) {
		fprintf(stderr, "%s", buf);
		exit(EXIT_FAILURE);
	}
	// Can complete ops_t struct
	if (argv[optind]) {
		operation->do_master_dir = 1;
		operation->dirname = realpath(argv[optind], NULL);
		if (!operation->dirname) {
			perror(argv[optind]);
			exit(EXIT_FAILURE);
		}
	} else if (opts->dirs_from) {
		operation->dirname = xstrdup(opts->dirs_from);
	}
	operation->do_dot_dirs = opts->do_dot_files;
	if (opts->dot_files_dir) {
		// dot file location is initialised needed or not.
		operation->dotfiles_dir = realpath(opts->dot_files_dir, NULL);
		if (!operation->dotfiles_dir) {
			perror(argv[optind]);
			exit(EXIT_FAILURE);
		}
	} else { // dot dir tarballs go to standard place.
		sprintf(buf, "%s/dotty", getenv("HOME"));
		operation->dotfiles_dir = xstrdup(buf);
	}
	// set up the cloud target name
	if (opts->cloud_target)
	{ /* if user has optioned a target it won't be NULL */
		operation->cloud_target = target_from_option(opts->cloud_target);
	} else
	{ /* no optional target so get it from file. */
		operation->cloud_target = target_from_file();
	}
} // check_args()

char
**make_dirslist(ops_t *operation)
{/* return a list of dirs using method depending on type of data given.
**/
	if (operation->do_master_dir) {
		return gen_dirslist(operation->dirname, operation->do_dot_dirs,
							operation->excludes);
	} else {
		return gen_listfromfile(operation->dirname);
	}
} // make_dirslist()

char
**gen_dirslist(const char *dirname, int dotsornot, char **excl_list)
{/* get the dir names under dirname selecting or avoiding dot dirs,
  * then turn the data into an array of C strings.
*/
	mdata *md = init_mdata();
	const size_t meminc = 1024 * 1024;	// This instance 1 meg good.
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
		if(inlist(buf, excl_list)) continue;
		meminsert(buf, md, meminc);
	}
	doclosedir(thedir);
	char **result = memblocktoarray(md, 0);
	free_mdata(md);
	return result;
} // gen_dirslist()

char
**gen_listfromfile(const char *filename)
{/* get the dir names contained in the named file,
  * then turn the data into an array of C strings.
*/
	mdata *md = readfile(filename, 1, 0);	// terminated with '\n'
	memlinestostr(md);	// C strings
	char **result = memblocktoarray(md, 0);
	free_mdata(md);
	return result;
} // gen_listfromfile()

void
process_dotdirs(ops_t *operation)
{/* set up to recurse dot dirs */
	if (!exists_file(operation->timestamp)) {
		char command[PATH_MAX];
		sprintf(command, "touch -d 1970-01-01 %s",
					operation->timestamp);
		(void)xsystem(command, 1);
	}
	operation->dd_updated = getfile_mtime(operation->timestamp);
	if (!exists_dir(operation->dotfiles_dir))
		newdir(operation->dotfiles_dir, 0);
	size_t hl = home_len();
	size_t i;
	for (i = 0; operation->files_list[i] ; i++) {
		// ->files_list is NULL terminated.
		int res = nc_recursedir(operation, operation->files_list[i]);
		if (res) {
			char toname[NAME_MAX];
			strcpy(toname, operation->dotfiles_dir);	// tarballs dir.
			strjoin(toname, '/', operation->files_list[i] + hl+2,
						NAME_MAX);
			strjoin(toname, '.', "tgz", NAME_MAX);
			/* tarball name is the name of the dot dir less '.'. */
			char comm[PATH_MAX];
			sprintf(comm, "tar -czf %s %s", toname,
						operation->files_list[i]);
			(void)xsystem(comm, 0);
		}
	}
	char command[PATH_MAX];	// time stamp update
	sprintf(command, "touch %s", operation->timestamp);
	(void)xsystem(command, 1);
} // process_dotdirs()

char
*timestamp_name(void)
{
	char buf[PATH_MAX];
	strcpy(buf, getenv("HOME"));
	strjoin(buf, '/', "dottim", PATH_MAX);
	return xstrdup(buf);
} // timestamp_name()

int
nc_recursedir(ops_t *operation, const char *dirname)
{ /* Recurse the given dirname checking dir and file objects for
   * age against the time stamp file. Return 1 if a newer object is
   * found, 0 otherwise.
*/
	time_t stamp = getfile_mtime(dirname);
	if (stamp > operation->dd_updated) return 1;
	DIR *dp = dopendir(dirname);
	struct dirent *de;
	while ((de = readdir(dp))) {
		if (strcmp(de->d_name, ".") == 0 ) continue;
		if (strcmp(de->d_name, "..") == 0) continue;
		if (!(de->d_type == DT_REG || de->d_type == DT_DIR)) continue;
		char joinbuf[PATH_MAX];
		strcpy(joinbuf, dirname);
		strjoin(joinbuf, '/', de->d_name, PATH_MAX);
		stamp = getfile_mtime(joinbuf);
		if (stamp > operation->dd_updated){
			doclosedir(dp);
			return 1;
		}
		if (de->d_type == DT_DIR) {
			int result = nc_recursedir(operation, joinbuf);
			if (result) {
				doclosedir(dp);
				return 1;
			}
		}
	} // while()
	doclosedir(dp);
	return 0;
} // nc_recursedir()

char
**excl_list(const char *prname)
{/* Return list of dirs to exclude from processing. If the excludes file
  * does not exist, create it with some reasonable default values.
*/
	char fpath[PATH_MAX] = {0};
	sprintf(fpath, "%s/.config/%s/excl.lst", getenv("HOME"), prname);
	if (!exists_file(fpath))
	{	// create it
		char *cp = strstr(fpath, "excl.lst");
		*cp = 0;	// does the dir exist? If not create it.
		if (!exists_dir(fpath)) newdir(fpath, 0);
		*cp = 'e';	// restore the filename to fpath.
		FILE *fpo = dofopen(fpath, "w");
		fprintf(fpo, "%s/%s\n", getenv("HOME"), "Dropbox");
		fprintf(fpo, "%s/%s\n", getenv("HOME"), "Nextcloud");
		dofclose(fpo);
		sync();
	}
	return getfile_str(fpath);
} // excl_list()

static void
proc_normaldirs(ops_t *operation)
{/* We have an array of C strings of the form $HOME/dirname. Required to
  * hard link all the files and sub dirs under $HOME/dirname into
  * $HOME/cloud_target/dirname, creating dirs in target as needed.
*/
	size_t hl = home_len();
	size_t i;
	for (i = 0; operation->files_list[i]; i++)
	{	// it's a NULL terminated list
		char *frdir = operation->files_list[i];
		char todir[PATH_MAX];
		strcpy(todir, operation->cloud_target);
		strjoin(todir, '/', &frdir[hl+1], PATH_MAX);
		if(!exists_dir(todir)) newdir(todir, 1);
		char command[PATH_MAX];
		sprintf(command, "synclink '%s' '%s'", frdir, todir);
		int res = xsystem(command, 0);
		if (res)
		{
			fprintf(stderr, "Non zero system result: %d\n", res);
		}
	}
} // proc_normaldirs()

char
*target_from_option(char *tname)
{	/* set up the cloud directory name from tname */
	char joinbuf[PATH_MAX];
	char *cp = get_home();
	strcpy(joinbuf, cp);
	free(cp);
	strjoin(joinbuf, '/', tname, PATH_MAX);
	return xstrdup(joinbuf);
} // target_from_option()

char
*target_from_file(void)
{	/* set up the cloud directory name from config file */
	char *cfgn = getcfgfn("csmanager", "target");
	char *tmp;
	if (!exists_file(cfgn))
	{
		str2file(cfgn, "Nextcloud");
		tmp = xstrdup("Nextcloud");
	} else
	{
		mdata *md = readfile(cfgn, 0, 1);
		memlinestostr(md);
		tmp = xstrdup(md->fro);
		free_mdata(md);
	}
	free(cfgn);
	char buf[PATH_MAX];
	sprintf(buf, "%s/%s", getenv("HOME"), tmp);
	free(tmp);
	return xstrdup(buf);
} // target_from_file()

char
**gen_fileslist(const char *dirname, char **excl_list)
{/* get the file names under dirname, then turn the data into an array
  * of C strings.
*/
	mdata *md = init_mdata();
	const size_t meminc = PATH_MAX;	// This instance 4k meg good.
	DIR *thedir = dopendir(dirname);
	struct dirent *de;
	while ((de = readdir(thedir))) {
		if(strcmp("..", de->d_name) == 0) continue;
		if(strcmp(".", de->d_name) == 0) continue;
		if(de->d_type != DT_REG) continue;
		char buf[PATH_MAX];
		strcpy(buf, dirname);
		strjoin(buf, '/', de->d_name, PATH_MAX);
		if(inlist(buf, excl_list)) continue;
		meminsert(buf, md, meminc);
	}
	doclosedir(thedir);
	char **result = memblocktoarray(md, 0);
	free_mdata(md);
	return result;
} // gen_fileslist()
