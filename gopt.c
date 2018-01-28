/*      gopt.c
 *
 *  Copyright 2017 Robert L (Bob) Parker rlp1938@gmail.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
*/

#include "files.h"
#include "dirs.h"
#include "str.h"
#include "gopt.h"


options_t process_options(int argc, char **argv)
{
	synopsis = thesynopsis();
	helptext = thehelp();
	optstring = ":hd:f:c:";

	/* declare and set defaults for local variables. */

	/* set up defaults for opt vars. */
	options_t opts = {0};	// assumes defaults all 0/NULL
	// initialise non-zero defaults below.


	int c;

	while(1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
		{"help",			0,	0,	'h'}, /* help text */
		{"dirs-from",		1,	0,	'd'}, /* a file, list of dirs */
		{"dot-files-dir",	1,	0,	'f'}, /* hidden dirs synced here */
		{"cloud-target",	1,	0,	'c'}, /* name of cloud dir */
		{0,	0,	0,	0}
		};

		c = getopt_long(argc, argv, optstring,
                        long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 0:
			switch (option_index) {
			} // switch()
		break;
		case 'h':
		dohelp(0);
		break;
		case 'd':
			opts.dirs_from = xstrdup(optarg);
			break;
		case 'f':
			opts.dot_files_dir = xstrdup(optarg);
			break;
		case 'c':
			opts.cloud_target = xstrdup(optarg);	// --cloud-target
			break;
		case ':':
			fprintf(stderr, "Option %s requires an argument\n",
					argv[this_option_optind]);
			dohelp(1);
		break;
		case '?':
			fprintf(stderr, "Unknown option: %s\n",
					 argv[this_option_optind]);
			dohelp(1);
		break;
		} // switch()
	} // while()
	return opts;
} // process_options()

void dohelp(int forced)
{
  if(strlen(synopsis)) fputs(synopsis, stderr);
  fputs(helptext, stderr);
  exit(forced);
} // dohelp()

char *thesynopsis(void)
{ /* Moved this text off the top of the page. */
	char *ret =
  "\tSYNOPSIS\n"
  "\t\tcsmanager [option] dirname\n\n"
  "\tDESCRIPTION\n"
  "\tcsmanager assists with managing directories to be syncronised with"
  " a\n\tnextcloud server. The server is to be set up to sync from\n\t"
  "$HOME/nextcloud/. The dirs to be synced by default, are all named "
  "dirs\n\tin $HOME that are not prefixed with '.' ie hidden dirs.\n"
  "\tThe program creates required dirs under nextcloud/ and then\n\t"
  "executes synclink to make hard links to of all files from the "
  "named\n\tdirs."
  "\n\n";
	return ret;
} // thesynopsis()

char *thehelp(void)
{
	char *ret =
  "\tOPTIONS\n"
  "\t-h, --help\n"
  "\tOutputs this help message and then quits.\n\n"
  "\t-d, --dirs-from dir_names_file\n"
  "\tSyncs a specific list of dirs rather than every non hidden "
  "dir in\n\t$HOME. This is especially useful for users taking "
  "advantage of\n\tfree sites made available to users with small "
  "storage needs.\n\n"
  "\t-D, --dot-files \n"
  "\tWhen selected this option causes the program to check the dot "
  "dirs\n\tunder $HOME and any such dirs that have files newer than "
  "when this was\n\tlast run will be tarballed and copied into the "
  "dot-files-dir.\n\n"
  "\t-l, --dot-files-dir dir_name\n"
  "\tBy default dot files tarballs are copied into $HOME/dotty. "
  "You can name\n\tanother dir for this purpose. However, this is "
  "not a persistent change.\n\tThe tarballs storing dot dirs data "
  "are eg named like this:\n\t$HOME/.config/ becomes "
  "$HOME/dotty/config.tgz and so on.\n\n"
  "\tFILES\n"
  "\tThere is a file $HOME/dottim the modification time of which is "
  "set to\n\tthe time of completion of the last dot-files run. Initially "
  "this file\n\tis dated '1970-01-01' so that all dot dirs are tarballed"
  " on the first\n\ttime the option is selected.\n"
  ;
	return ret;
} // thehelp()
