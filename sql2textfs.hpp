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

#ifndef SQL2TEXTFS_HPP_INCLUDED
#define SQL2TEXTFS_HPP_INCLUDED

// The FUSE API has been changed a number of times.  So, our code
// needs to define the version of the API that we assume.  As of this
// writing, the most current API version is 26
#define FUSE_USE_VERSION 26

// need this to get pwrite(). We have to use setvbuf() instead of
// setlinebuf() later in consequence.
#define _XOPEN_SOURCE 500
#define _DARWIN_C_SOURCE

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <string>
#include <vector>
#include <fstream>
#include "sql2text.hpp"

#include "log.hpp"
#include "rdel.hpp"

// This is a macro that returns the fuse private data.
// This data will be needed in all fuse callback functions.
#define FS_DATA ((fs_state *) fuse_get_context()->private_data)

// The struct fs_state contains all the fuse private data.
// This data contains every permanant variable that is
// needed for a single mounted directory.
// This struct can be accessed from every fuse callback
// function using the FS_DATA macro.
struct fs_state {

	// The log-file that logs every action of sql2textfs
	FILE *logfile;
	// Verbose logging flag (0: short, 1: verbose)
	int verbose;
	// Reload flag (0: no reload, 1: reload)
	int reload;

	// The mount directory path
	char *rootdir;

	// Handle to an sql2text database connection
	sql2text::handle* h;

	// Mutual Exclusion handle for pthread library.
	// Ensures that all calls to sql2textfs handlers are
	// handled asynchronously.
	pthread_mutex_t lock;

	// A collection of unique id's
	// Used for acquiring/releasing internal resource handlers, that
	// are passed-down to fuse.
	// A resource handler either holds a string or a vector of strings
	// or both.
	std::map<int, bool> keys;
	std::map<int, std::vector<std::string> > v;
	std::map<int, std::string> s;
	// Acquire a new resource handler and return its id to the caller.
	int n_key()
	{
		int i;
		for(i =0; keys[i]; i++);
		keys[i] = true;
		return i;
	}
	// Remove the resource handler with id i from the keys
	void r_key(int i)
	{
		if(v.find(i) != v.end()) v.erase(i);
		if(s.find(i) != s.end()) s.erase(i);
		keys[i]=false;
	}

	// A flag that is true if a file is open.
	// number n > 0 means file has been opened n times
	std::map<std::string, int> openfiles;
};

// --------------------------------------------------------
// extra helper functions for the fuse callbacks

// Convert a relative path as given from fuse to absolute path
static __attribute__ ((unused)) void fs_fullpath(char fpath[PATH_MAX], const char *path)
{
	strcpy(fpath, FS_DATA->rootdir);
	strncat(fpath, path, PATH_MAX); // ridiculously long paths will
					// break here

	log_vmsg("+ fs_fullpath(\"%s\", \"%s\") = \"%s\"\n",
		FS_DATA->rootdir, path, fpath);
}

// Report errors to logfile and give -errno to caller
static __attribute__ ((unused)) int fs_error(const char *str)
{
	int ret = -errno;

	log_msg("    ERROR %s: %s\n", str, strerror(errno));

	return ret;
}

// --------------------------------------------------------
// callback declarations

int fs_getattr(const char *path, struct stat *statbuf);
int fs_readlink(const char *path, char *link, size_t size);
int fs_mknod(const char *path, mode_t mode, dev_t dev);
int fs_mkdir(const char *path, mode_t mode);
int fs_unlink(const char *path);
int fs_rmdir(const char *path);
int fs_symlink(const char *path, const char *link);
int fs_rename(const char *path, const char *newpath);
int fs_link(const char *path, const char *newpath);
int fs_chmod(const char *path, mode_t mode);
int fs_chown(const char *path, uid_t uid, gid_t gid);
int fs_truncate(const char *path, off_t newsize);
int fs_utime(const char *path, struct utimbuf *ubuf);
int fs_open(const char *path, struct fuse_file_info *fi);
int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int fs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi);
int fs_statfs(const char *path, struct statvfs *statv);
int fs_flush(const char *path, struct fuse_file_info *fi);
int fs_release(const char *path, struct fuse_file_info *fi);
int fs_fsync(const char *path, int datasync, struct fuse_file_info *fi);
int fs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
int fs_getxattr(const char *path, const char *name, char *value, size_t size);
int fs_listxattr(const char *path, char *list, size_t size);
int fs_removexattr(const char *path, const char *name);
int fs_opendir(const char *path, struct fuse_file_info *fi);
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi);
int fs_releasedir(const char *path, struct fuse_file_info *fi);
int fs_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi);
void *fs_init(struct fuse_conn_info *conn);
void fs_destroy(void *userdata);
int fs_access(const char *path, int mask);
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int fs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi);
int fs_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi);

// Declare a struct that will be used for fuse initialization
static struct fuse_operations fs_oper;

// Fill-in the fuse initialization struct with all the required fuse callbacks
static __attribute__ ((unused)) fuse_operations& fs_oper_init()
{
	fs_oper.getattr = fs_getattr;
	//fs_oper.readlink = fs_readlink;
	// no .getdir -- that's deprecated
	//fs_oper.getdir = NULL;
	fs_oper.mknod = fs_mknod;
	fs_oper.mkdir = fs_mkdir;
	fs_oper.unlink = fs_unlink;
	fs_oper.rmdir = fs_rmdir;
	//fs_oper.symlink = fs_symlink;
	fs_oper.rename = fs_rename;
	//fs_oper.link = fs_link;
	//fs_oper.chmod = fs_chmod;
	//fs_oper.chown = fs_chown;
	fs_oper.truncate = fs_truncate;
	fs_oper.utime = fs_utime;
	fs_oper.open = fs_open;
	fs_oper.read = fs_read;
	fs_oper.write = fs_write;

	// ------------------------------

	fs_oper.statfs = fs_statfs;
	fs_oper.flush = fs_flush;
	fs_oper.release = fs_release;
	//fs_oper.fsync = fs_fsync;

	//fs_oper.setxattr = fs_setxattr;
	//fs_oper.getxattr = fs_getxattr;
	//fs_oper.listxattr = fs_listxattr;
	//fs_oper.removexattr = fs_removexattr;

	fs_oper.opendir = fs_opendir;
	fs_oper.readdir = fs_readdir;
	fs_oper.releasedir = fs_releasedir;
	//fs_oper.fsyncdir = fs_fsyncdir;
	fs_oper.init = fs_init;
	fs_oper.destroy = fs_destroy;
	fs_oper.access = fs_access;
	//fs_oper.create = fs_create;
	//fs_oper.ftruncate = fs_ftruncate;
	//fs_oper.fgetattr = fs_fgetattr;
	return fs_oper;
}

#endif // SQL2TEXTFS_HPP_INCLUDED
