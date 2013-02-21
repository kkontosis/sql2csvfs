/*  sql2textfs, a FUSE filesystem for mounting database tables as text files 
 *  Copyright (C) 2013, Kimon Kontosis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation; either version 3.0, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  version 3.0 along with this program (see LICENSE); if not, see 
 *  <http://www.gnu.org/licenses/>.
 *
*/

/*
 * Reference Note: 
 * The fuse implementation was inspired by the online tutorial:
 *
 * Writing a FUSE Filesystem: a Tutorial
 * Joseph J. Pfeiffer, Jr., Ph.D.
 * Emeritus Professor
 * Department of Computer Science
 * New Mexico State University
 * pfeiffer@cs.nmsu.edu
*/

#include "sql2textfs.hpp"
#include "bindir.hpp"

// -------------------------------------------------------------------------------------------------

int fs_usage(const char* programname)
{
	printf("sql2textfs version 1.0\n");
	printf("Copyright (c) 2013 Kimon Kontosis, licenced under the GNU GPL v3.0 or above\n");
	printf("usage: %s [options]... [mount-options]... <conn-string> <path>\n", 
		programname);
	printf("where:\n");
	printf("\t<conn-string> is the database connection string, and\n");
	printf("\t<path> is the mounting point\n");
	printf("\nthe database connection string consists of pairs of the type:\n");
	printf("\tkey=value\n");
	printf("separated by semicolon (;) following the database type name and a colon (:)\n");
	printf("for example:\n");
	printf("\tmysql:user=username;password=hackme\n");
	printf("see also the documentation of `cppcms` for connection string information\n");
	printf("\nvalid options are:\n");
	printf("\t--help\t\t\tshows help\n");
	printf("\t--root\t\t\tallow running as root\n");
	printf("\t--log <file>\t\tuse log file <file>\n");
	printf("\t--verbose\t\tenable verbose logging\n");
	printf("\t--disable-reload\tno reloading files on the fly\n");
	printf(" \nvalid mount-options are:\n");
	printf(" \t-o opt\twhere opt is a valid mount option\n");
	printf("see also: `man mount' for a full list of the mount options\n");
	printf("\n");
	return 0;
}

// -------------------------------------------------------------------------------------------------

char tmpn[512];
const char* logname = "/dev/null";
int verbose = 0;
int reload = 1;

int main(int argc, char *argv[])
{
	std::ios::sync_with_stdio();

	int i;
	int fuse_stat;
	int argstart = 0;
	int nextarg = 1;
	fs_state *fs_data;

	int enable_root = 0;

	/* parse custom options */
	while(nextarg-- && argstart+1 < argc) {
		if(!strcmp(argv[argstart+1], "--root")) { enable_root = 1; argstart++; nextarg=1; }
		else if(!strcmp(argv[argstart+1], "--verbose")) { verbose = 1; argstart++; nextarg=1; }
		else if(!strcmp(argv[argstart+1], "--disable-reload")) { reload = 0; argstart++; nextarg=1; }
		else if(!strcmp(argv[argstart+1], "--help")) argc=1;
		else if(!strcmp(argv[argstart+1], "--log") && argstart+2 < argc) 
			{ logname=argv[argstart+2]; argstart+=2; nextarg=1; }
	}

	/* do not run as root */
	if (((getuid() == 0) || (geteuid() == 0))&&(!enable_root)) {
		fprintf(stderr, "Running sql2textmount as root opens unnacceptable security holes\n");
		fprintf(stderr, "To force running as root use the option: --root\n");
		return 1;
	}
  
	/* find configuration file path */

	std::string bindir = get_bin_dir(argv[0]);
	const char* default_cfg = "/etc/sql2text/config.ret";
	std::string cfg_file = default_cfg;
	if(!tool::exist(default_cfg)) {
		cfg_file = bindir + default_cfg;
		if(!tool::exist(cfg_file)) {
			fprintf(stderr, "cannot find configuration file %s\n", default_cfg);
			return 1;
		}
	}

	std::string cfg_dir = get_file_dir(cfg_file.c_str());

	// save current directory
	char cur_dir[PATH_MAX];
	char *curdir=getcwd(cur_dir, PATH_MAX);
	if(chdir(cfg_dir.c_str()))
		{ std::cerr << "error: cannot change directory to " << cfg_dir << std::endl; return 1; }

	retranse::node* nc = retranse::compile("config.ret");

	// restore current directory
	if(chdir(curdir))
		{ std::cerr << "error: cannot change directory to " << curdir << std::endl; return 1; }

	if(!nc) {
		std::cerr << "error in configuration file" << cfg_file << std::endl;
		return 1;
	}

	fs_data = new fs_state();
	fs_data->verbose = verbose;
	fs_data->reload = reload;
	fs_data->logfile = log_open(logname);
    
	// libfuse is able to do the rest of the command line parsing; 
	for (i = 1+argstart; (i < argc) && (argv[i][0] == '-'); i++)
	if (argv[i][1] == 'o') i++; // -o takes a parameter; need to skip
  
	if ((argc - i) != 2) return fs_usage(argv[0]);
    
	strcpy(tmpn, "/tmp/sql2textfs-XXXXXX");
	fs_data->rootdir = mkdtemp(tmpn);
	//std::cout << "Temp directory = " << fs_data->rootdir << std::endl;

	try {
		//realpath(argv[i], NULL);
		cppdb::connection_info ci(argv[i]);

		std::string modules_path = "/usr/lib:/usr/local/lib:/usr/lib/sql2text/shared:/usr/local/lib/sql2text/shared";
		if(bindir.size()) { modules_path += ":"; modules_path += bindir + "/lib/sql2text/shared"; }

		if(!ci.has("@modules_path")) ci.properties["@modules_path"] = modules_path;

		cppdb::session sql(ci);
		
		fs_data->h = new sql2text::handle(ci, sql, nc);
		fs_data->h->check();
		
		pthread_mutex_init(&(fs_data->lock), NULL);


		argv[argstart] = argv[0];
		argv[i] = argv[i+1];
		argc--;
	    
		//fprintf(stderr, "about to call fuse_main\n");
		fuse_stat = fuse_main(argc-argstart, argv+argstart, &(fs_oper_init()), fs_data);
		fprintf(stderr, "%s: fuse_main returned %d\n", argv[0], fuse_stat);
		rmdir(fs_data->rootdir);
	    
		return fuse_stat;
	}
	catch(retranse::rtex const &x) {
		std::cerr << "CONFIGURATION ERROR: " << x.s << std::endl;
		return 1;
	}
	catch(std::exception const &e) {
		std::cerr << "ERROR: " << e.what() << std::endl;
		return 1;
	}
	catch(...) {}

	return 0;
}

