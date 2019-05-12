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
#include "pstream.h"

// This is the mode that is used for mkdir in the temporary
// directory.
#define DB_MKDIR_MODE 0770

// Macros to lock and unlock the main mutex
#define LOCKIT pthread_mutex_lock(&(FS_DATA->lock));
#define UNLOCKIT pthread_mutex_unlock(&(FS_DATA->lock));

////////////////////////////////////////////////////////////////////////////////
// helpers
////////////////////////////////////////////////////////////////////////////////

// The struct monolock allows locking and unclocking of
// the main mutex, of the fuse private data. It is merely
// a wrapper class. It also unlocks the mutex if it remained
// locked by the current object, at the destructor, allowing to
// automatically unlock after throwing exceptions,
// returning from functions etc. The actual class is not thread-safe.
struct monolock {
	volatile bool l;
	monolock() : l(false) {}
	void lock() { if(!l) { l=true; LOCKIT } }
	void unlock() { if(l) { l=false; UNLOCKIT } }
	~monolock() { unlock(); }
};

// Read a whole table from the database to a temporary file
bool readtab(const char * p1, const char * p2)
{
	log_vmsg("+ readtab(%s, %s)\n", p1, p2);

	using namespace std;
	fs_state* b = FS_DATA;
	try {
		log_vmsg("+ + readtab running ls_tabh\n");
		std::string sx=b->h->ls_tabh(p1, p2);
		if(sx.size()==0) return false;
		log_vmsg("+ + readtab ls_tabh: ok!\n");
		ofstream of((std::string(b->rootdir) + "/" + p1 + "/" + p2).c_str());
		if(!of.good()) return false;
		log_vmsg("+ + readtab running cat_tab\n");
		of << sx << std::endl;
		b->h->cat_tab(p1, p2, of);
		log_vmsg("+ + readtab cat tab: ok!\n");
	}
	catch(...) {
		return false;
	}
	log_vmsg("+ readtab: ok!\n");
	return true;
}

#define DBCLONEEXT ".o"
// Check if filename contains an invalid sequence
// for example, ".o" is reserved for the db clones
inline bool checkdot(const char* path)
{
	const char* p;
	//return false;
	return (p=strchr(path,'.')) && *(p+1)=='o' && *(p+2)==0;
}

// Check if file exists
inline bool fexist(const char* s)
{
	std::ifstream ifs(s);
	return ifs.good();
}

// Make a clone of a table's temporary file to file with extension .o
bool copytab(const char * p1, const char * p2)
{
	log_vmsg("+ copytab(%s, %s)\n", p1, p2);

	using namespace std;
	fs_state* b = FS_DATA;
	ifstream ff((std::string(b->rootdir) + "/" + p1 + "/" + p2).c_str());
	if(!ff.good()) return false;
	ofstream of((std::string(b->rootdir) + "/" + p1 + "/" + p2 + DBCLONEEXT).c_str());
	if(!of.good()) return false;
	of << ff.rdbuf();
	/*while(ff.good() && !ff.eof()) {
	ff.read
	}*/
	return true;
}

// Create a new table from data in file `from`, p1=db, p2=table name
bool run_create(const char *from, const char* p1, const char* p2)
{
	log_vmsg("+ run_create(%s, %s, %s)\n",from,p1,p2);

	fs_state* b = FS_DATA;

	std::ifstream is(from);
	std::string line;

	if(std::getline(is, line)) {
		log_vmsg("+ + creating table with: %s\n",line.c_str());
		b->h->mk_tab(p1, p2, line);
		log_vmsg("+ + created table.\n");
	}
	else return false;

	sql2text::tbl info;
	try{
		b->h->info_tab(info, p1, p2);
	}
	catch(retranse::rtex& r)
	{
		log_vmsg("+ + crerr_retranse!!:%s:@%s/%s\n",from,p1,p2);
		return false;
	}
	catch(std::exception& r)
	{
		log_vmsg("+ + crerr_std!!:%s:@%s/%s==%s\n",from,p1,p2,r.what());
		return false;
	}
	catch(...)
	{
		log_vmsg("+ + crerr!!:%s:@%s/%s\n",from,p1,p2);
		return false;
	}

	while (std::getline(is, line)) {
		log_vmsg("+ + create line=%s\n",line.c_str());
		//of << line << std::endl;
		b->h->add_tab_row(info, p1, p2, line.c_str());
		log_vmsg("+ + create line_end=%s\n",line.c_str());
	}
	log_msg("+ run_create: ok!\n");

	return true;
}

// Execute `diff' to find modified lines. Apply modifications to database.
// Return true if there have been modifications and they are applied.
// from, to: temporary files to check with diff. p1=db, p2=table name
bool exec_diff(const char *from, const char *to, const char* p1, const char* p2)
{
	log_vmsg("+ exec_diff(%s, %s, %s, %s)\n",from,to,p1,p2);
	/*
	   //redi::ipstream proc("./some_command", redi::pstreams::pstderr);
	  std::string line;
	  // read child's stdout
	  while (std::getline(proc.out(), line))
		std::cout << "stdout: " << line << 'n';
	  // read child's stderr
	  while (std::getline(proc.err(), line))
		std::cout << "stderr: " << line << 'n';
	*/
	/*
	   fs_state* b = FS_DATA;

	   std::vector<std::string> arg;
	   arg.push_back(from);
	   arg.push_back(to);
	   redi::ipstream is("/usr/bin/diff", arg);
	   std::string line;
	   std::ofstream of((std::string(b->rootdir) + "/" + p1 + "/" + p2 + ".po").c_str());
	   while (std::getline(is, line)) {
		log_msg("line=%s\n",line.c_str());
		 of << line << std::endl;
	   }
	*/
	fs_state* b = FS_DATA;

	// Execution of `diff'
	std::vector<std::string> arg;
	arg.push_back("/usr/bin/diff");
	arg.push_back(from);
	arg.push_back(to);
	redi::ipstream is("/usr/bin/diff", arg);
	std::string line;

	// Get table info
	sql2text::tbl info;
	try{
		b->h->info_tab(info, p1, p2);
	}
	catch(retranse::rtex& r)
	{
		log_vmsg("+ + differr_retranse!!:%s:%s@%s/%s\n",from,to,p1,p2);
		return false;
	}
	catch(std::exception& r)
	{
		log_vmsg("+ + differr_std!!:%s:%s@%s/%s==%s\n",from,to,p1,p2,r.what());
		return false;
	}
	catch(...)
	{
		log_vmsg("+ + differr!!:%s:%s@%s/%s\n",from,to,p1,p2);
		return false;
	}

	// Update different lines:
	int ch=0;
	while (std::getline(is, line)) {
		log_vmsg("+ + diff line=%s\n",line.c_str());
		//of << line << std::endl;
		if(line.size()>=2 && line[0]=='<')
			{ ch++; b->h->add_tab_row(info, p1, p2, line.c_str()+2); }
		if(line.size()>=2 && line[0]=='>')
			{ ch++; b->h->rm_tab_row(info, p1, p2, line.c_str()+2); }
		log_vmsg("+ + diff line_end=%s\n",line.c_str());
	}
	log_vmsg("+ exec_diff: ok!\n");

	if(ch) return true;
	return false;
	// true if diff
	return true;
}

bool existance(const char* path, const char* tmpname, const char* reldir, const char* fname, int& retstat, const char* error_str)
{
	if(!fexist(tmpname)) {
		if(!readtab(reldir, fname))
			{ return (false); }
		else if(!copytab(reldir, fname))
			{ retstat = fs_error(error_str); return (false); }
	} else { // exists, test for reload trial
		if(FS_DATA->reload && !FS_DATA->openfiles[path])
		{
			if(fexist((std::string(tmpname)+DBCLONEEXT).c_str())) //(was on database, not pseudo-file)
			{
				// probably safe to reload this file...
				if(!readtab(reldir, fname))
					{ return (false); }
				else if(!copytab(reldir, fname))
					{ retstat = fs_error(error_str); return (false); }
			}
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//
////////////////////////////////////////////////////////////////////////////////

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int fs_getattr(const char *path, struct stat *statbuf)
{
	monolock ml;
	ml.lock();

	int retstat = 0;
	char fpath[PATH_MAX];
	char fgpath[PATH_MAX];
	const char *sx;

	log_vmsg("\n");
	log_msg("fs_getattr(path=\"%s\", statbuf=0x%08x)\n", path, statbuf);
	fs_fullpath(fgpath, path);

	retstat = lstat(fgpath, statbuf);

	if(retstat != 0 && path[0] && (sx=strchr(path+1,'/')) && !checkdot(path)) {
		log_vmsg("+ fs_getattr criterion met\n");
		strcpy(fpath,path+1);
		fpath[sx-path-1]=0;
		retstat = fs_error("fs_getattr lstat");
		if(!existance(path, fgpath, fpath, fpath+(sx-path), retstat, "fs_getattr error"))
			return (retstat);
		fs_fullpath(fpath, path);
		retstat = lstat(fpath, statbuf);
	}

	if (retstat != 0)
		{ return fs_error("fs_getattr lstat"); return retstat; }

	log_stat(statbuf);

	ml.unlock(); // ensure no silly optimizations
	return retstat;
}


/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int fs_mknod(const char *path, mode_t mode, dev_t dev)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	char fpath[PATH_MAX];

	log_vmsg("\n");
	log_msg("fs_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n", path, mode, dev);
	fs_fullpath(fpath, path);

	// On Linux this could just be 'mknod(path, mode, rdev)' but this
	//  is more portable
	if (S_ISREG(mode)) {
		retstat = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (retstat < 0)
			retstat = fs_error("fs_mknod open");
		else {
			retstat = close(retstat);
			if (retstat < 0)
				retstat = fs_error("fs_mknod close");
		}
	} else if (S_ISFIFO(mode)) {
		retstat = mkfifo(fpath, mode);
		if (retstat < 0)
			retstat = fs_error("fs_mknod mkfifo");
	} else {
		retstat = mknod(fpath, mode, dev);
		if (retstat < 0)
			retstat = fs_error("fs_mknod mknod");
	}

	ml.unlock();
	return retstat;
}

/** Create a directory */
int fs_mkdir(const char *path, mode_t mode)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	char fpath[PATH_MAX];

	log_vmsg("\n");
	log_msg("fs_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);

	if(path[0] && strchr(path+1,'/')){ fs_error("fs_mkdir mkdir"); return -1; }

	fs_state* b = FS_DATA;

	try {
		b->h->mk_db(path+1);
	}
	catch(retranse::rtex& e) // if fail to create database
		{ log_vmsg("+ config: %s\n",e.s.c_str()); return -1; }
	catch(std::exception& e) // if fail to create database
		{ log_vmsg("+ cppdb: %s\n",e.what()); return -1; }
	catch(...) // if fail to create database
		{ fs_error("fs_mkdir mkdir"); return -1; }


	fs_fullpath(fpath, path);

	retstat = mkdir(fpath, DB_MKDIR_MODE);
	if (retstat < 0)
		retstat = fs_error("fs_mkdir mkdir");

	ml.unlock();
	return retstat;
}


/** Remove a directory */
int fs_rmdir(const char *path)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	char fpath[PATH_MAX];

	log_vmsg("\n");
	log_msg("fs_rmdir(path=\"%s\")\n", path);
	fs_fullpath(fpath, path);

	retstat = rmdir(fpath);
	if (retstat < 0)
		{ retstat = fs_error("fs_rmdir rmdir"); return -1; }

	fs_state* b = FS_DATA;

	try {

		b->h->rm_db(path+1);

	}catch(...) // if fail to delete database
	{
		mkdir(fpath, DB_MKDIR_MODE);
		fs_error("fs_rmdir rmdir");
		retstat = -1;
		return retstat;
	}

	ml.unlock();
	return retstat;
}

/** Remove a file */
int fs_unlink(const char *path)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	char fpath[PATH_MAX];
	char fgpath[PATH_MAX];
	const char *sx;
	struct stat statbuf;

	log_vmsg("\n");
	log_msg("fs_unlink(path=\"%s\")\n", path);
	fs_fullpath(fgpath, path);

	retstat = lstat(fgpath, &statbuf);

	if(retstat == 0 && path[0] && (sx=strchr(path+1,'/')) && !checkdot(path)) {
		log_vmsg("+ fs_unlink criterion met\n");
		strcpy(fpath,path+1);
		fpath[sx-path-1]=0;

		fs_state* b = FS_DATA;

		try {

			if(fexist((std::string(fgpath)+DBCLONEEXT).c_str())) {

				// do only remove from database if clone file is present
				b->h->rm_tab(fpath, fpath+(sx-path));

				unlink((std::string(fgpath)+DBCLONEEXT).c_str());
			}

		} catch(...) {
			return (retstat = fs_error("fs_unlink unlink"));
		}
	}

	retstat = unlink(fgpath);
	if (retstat < 0)
		retstat = fs_error("fs_unlink unlink");

	ml.unlock();
	return retstat;
}


/** Rename a file */
// both path and newpath are fs-relative
int fs_rename(const char *path, const char *newpath)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	char fpath[PATH_MAX];
	char fgpath[PATH_MAX];
	char fnewpath[PATH_MAX];
	const char*sx;
	struct stat statbuf;

	log_vmsg("\n");
	log_msg("fs_rename(fpath=\"%s\", newpath=\"%s\")\n", path, newpath);

	fs_state* b = FS_DATA;

	try{
		fs_fullpath(fgpath, path);

		retstat = lstat(fgpath, &statbuf);

		log_vmsg("+ fs_rename_pre = %d\n",retstat);

		// safety check to run ls b4 mv
		if(retstat != 0 && path[0] && (sx=strchr(path+1,'/')) && !checkdot(path)) {
			retstat = fs_error("fs_getattr lstat");
			strcpy(fpath,path+1);
			fpath[sx-path-1]=0;

			if(!existance(path, fgpath, fpath, fpath+(sx-path), retstat, "fs_rename error"))
				return (retstat);

			fs_fullpath(fpath, path);
			retstat = lstat(fpath, &statbuf);
		}

		if(retstat == 0 && path[0] && (sx=strchr(path+1,'/')) && !checkdot(path)) {
			log_vmsg("+ fs_rename_in! = %d \n",retstat);
			strcpy(fpath,path+1);
			fpath[sx-path-1]=0;
			if(strncmp(fpath, newpath+1, strlen(fpath))
				||(strchr(fpath+(sx-path), '/'))
				||(strchr(newpath+1+(sx-path), '/')))
					return retstat = fs_error("fs_rename rename");

			log_vmsg("+ fs_rename mv_tab %s %s %s \n", fpath, fpath+(sx-path), newpath+1+(sx-path));

			b->h->mv_tab(fpath, fpath+(sx-path), newpath+1+(sx-path));


			fs_fullpath(fpath, path);
			fs_fullpath(fnewpath, newpath);

			retstat = rename(fpath, fnewpath);
			if (retstat < 0)
				retstat = fs_error("fs_rename rename");

            strcat(fpath, DBCLONEEXT);
            strcat(fnewpath, DBCLONEEXT);
            retstat = rename(fpath, fnewpath);
            if (retstat < 0)
                retstat = fs_error("fs_rename rename");

			log_vmsg("+ fs_rename mv_tab succeeded\n");
			return retstat;
		}
		else
			return retstat = fs_error("fs_open open");

		fs_fullpath(fpath, path);
		fs_fullpath(fnewpath, newpath);

		retstat = rename(fpath, fnewpath);
		if (retstat < 0)
			retstat = fs_error("fs_rename rename");

		ml.unlock();
		return retstat;

	}
	catch(...)
	{
		ml.unlock();
		return retstat = -1; //fs_error("fs_rename rename");
	}

}



/** Change the size of a file */
int fs_truncate(const char *path, off_t newsize)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	char fpath[PATH_MAX];

	log_vmsg("\n");
	log_msg("fs_truncate(path=\"%s\", newsize=%lld)\n", path, newsize);
	fs_fullpath(fpath, path);

	retstat = truncate(fpath, newsize);
	if (retstat < 0)
		fs_error("fs_truncate truncate");
	//TODO: data changing eval.

	ml.unlock();
	return retstat;
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int fs_utime(const char *path, struct utimbuf *ubuf)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	char fpath[PATH_MAX];

	log_vmsg("\n");
	log_msg("fs_utime(path=\"%s\", ubuf=0x%08x)\n", path, ubuf);
	fs_fullpath(fpath, path);

	retstat = utime(fpath, ubuf);
	if (retstat < 0)
	retstat = fs_error("fs_utime utime");

	ml.unlock();
	return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int fs_open(const char *path, struct fuse_file_info *fi)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	int fd;
	char fpath[PATH_MAX];
	char fgpath[PATH_MAX];
	const char* sx;

	log_vmsg("\n");
	log_msg("fs_open(path\"%s\", fi=0x%08x)\n", path, fi);
	fs_fullpath(fgpath, path);
	retstat = 1; // lstat(fpath, statbuf);

	if(retstat != 0 && path[0] && (sx=strchr(path+1,'/')) && !checkdot(path)) {

		strcpy(fpath,path+1);
		fpath[sx-path-1]=0;

		if(!existance(path, fgpath, fpath, fpath+(sx-path), retstat, "fs_open error"))
			return (retstat = fs_error("fs_open error"));

		//if(!fexist((std::string(fgpath)+".o").c_str()))
		//  if(!copytab(fpath, fpath+(sx-path))) return (retstat = fs_error("fs_open error"));

		fs_fullpath(fpath, path);

		fd = open(fpath, fi->flags);
		if (fd < 0)
			retstat = fs_error("fs_open open");

		fi->fh = fd;
		log_fi(fi);

		FS_DATA->openfiles[path]++;

		ml.unlock();
		return 0;

	} else {

		return retstat = fs_error("fs_open open");

		// old open imp
		/*
		retstat = 0;
		fd = open(fgpath, fi->flags);
		if (fd < 0)
			retstat = fs_error("fs_open open");

		fi->fh = fd;
		log_fi(fi);

		ml.unlock();
		return retstat;
		*/

	}

}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	monolock ml;
	ml.lock();
	int retstat = 0;

	log_vmsg("\n");
	log_msg("fs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
		path, buf, size, offset, fi);
	// no need to get fpath on this one, since I work from fi->fh not the path
	log_fi(fi);

	retstat = pread(fi->fh, buf, size, offset);
	if (retstat < 0)
		retstat = fs_error("fs_read read");

	ml.unlock();
	return retstat;
}


/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
// As  with read(), the documentation above is inconsistent with the
// documentation for the write() system call.
int fs_write(const char *path, const char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	monolock ml;
	ml.lock();
	int retstat = 0;

	log_vmsg("\n");
	log_msg("fs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
		path, buf, size, offset, fi);
	// no need to get fpath on this one, since I work from fi->fh not the path
	log_fi(fi);

	retstat = pwrite(fi->fh, buf, size, offset);
	if (retstat < 0)
		retstat = fs_error("fs_write pwrite");

	ml.unlock();
	return retstat;
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int fs_statfs(const char *path, struct statvfs *statv)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	char fpath[PATH_MAX];

	log_vmsg("\n");
	log_msg("fs_statfs(path=\"%s\", statv=0x%08x)\n", path, statv);
	fs_fullpath(fpath, path);

	// get stats for underlying filesystem
	retstat = statvfs(fpath, statv);
	if (retstat < 0)
		retstat = fs_error("fs_statfs statvfs");

	log_statvfs(statv);

	ml.unlock();
	return retstat;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
 //TODO: here put all the messy stuff hehehe!
int fs_flush(const char *path, struct fuse_file_info *fi)
{
	monolock ml;
	ml.lock();
	int retstat = 0;

	log_vmsg("\n");
	log_msg("fs_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
	// no need to get fpath on this one, since I work from fi->fh not the path
	log_fi(fi);

	ml.unlock();
	return retstat;
}




/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int fs_release(const char *path, struct fuse_file_info *fi)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	char fpath[PATH_MAX];
	char fgpath[PATH_MAX];
	const char* sx;

	log_vmsg("\n");
	log_msg("fs_release(path=\"%s\", fi=0x%08x)\n", path, fi);
	log_fi(fi);

	// We need to close the file.  Had we allocated any resources
	// (buffers etc) we'd need to free them here as well.
	retstat = close(fi->fh);
	FS_DATA->openfiles[path]--;
	//return retstat;

	fs_fullpath(fpath, path);

	try {
		retstat = 1;//lstat(fpath, statbuf);

		if(retstat != 0 && path[0] && (sx=strchr(path+1,'/')) && !checkdot(path)) {
			strcpy(fpath,path+1);
			fpath[sx-path-1]=0;
			fs_fullpath(fgpath, path);

			if(fexist((std::string(fgpath)+DBCLONEEXT).c_str())) {

				bool rv=exec_diff(fgpath, (std::string(fgpath)+".o").c_str(), fpath, fpath+(sx-path));
				//if(!fexist(fgpath))
				if(rv) {
					if(!readtab(fpath, fpath+(sx-path)))
						return (retstat = fs_error("fs_release read db error"));
					//if(!fexist((std::string(fgpath)+".o").c_str()))
					if(!copytab(fpath, fpath+(sx-path)))
						return (retstat = fs_error("fs_release copy error"));

				}

				//if(!rv)
				//	return retstat = -1;
			} else {
				bool rv=run_create(fgpath, fpath, fpath+(sx-path));
				if(rv) {
					if(!readtab(fpath, fpath+(sx-path)))
						return (retstat = fs_error("fs_release read db error after creation"));
					//if(!fexist((std::string(fgpath)+".o").c_str()))
					if(!copytab(fpath, fpath+(sx-path)))
						return (retstat = fs_error("fs_release copy error"));

				}

			}

			//no: if(!readtab(fpath, fpath+(sx-path))) return (retstat = fs_error("fs_open error"));
			//if(!copytab(fpath, fpath+(sx-path))) return (retstat = fs_error("fs_open error"));

		}

		retstat = 0;

		ml.unlock();
		return retstat;
	}
	catch(...)
	{
		log_vmsg("+ fs_release diff exception\n");
		if(!readtab(fpath, fpath+(sx-path)))
			return (retstat = fs_error("fs_release read error after exception"));
		if(!copytab(fpath, fpath+(sx-path)))
			return (retstat = fs_error("fs_release copy error after exception"));
		ml.unlock();
		return retstat = -1; //fs_error("fs_rename rename");
	}
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int fs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
	monolock ml;
	ml.lock();
	int retstat = 0;

	log_vmsg("\n");
	log_msg("fs_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n", path, datasync, fi);
	log_fi(fi);

	if (datasync) {
#ifdef __APPLE__
		// fcntl(fi->fh, F_FULLFSYNC);
		retstat = fsync(fi->fh);
#else
		retstat = fdatasync(fi->fh);
#endif
	} else
		retstat = fsync(fi->fh);

	if (retstat < 0)
		fs_error("fs_fsync fsync");

	ml.unlock();
	return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int fs_opendir(const char *path, struct fuse_file_info *fi)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	char fpath[PATH_MAX];

	log_vmsg("\n");
	log_msg("fs_opendir(path=\"%s\", fi=0x%08x)\n", path, fi);

	fs_state* b = FS_DATA;
	DIR* dp;
	int k=b->n_key();
	fi->fh = -1;


	//////////////////////////////////
	// TODO: case where sub-dir exists but is not yet created
	if(!strcmp(path, "/")) { } // root
	else {
		fs_fullpath(fpath, path);

		dp = opendir(fpath);
		if (dp == NULL)
			retstat = fs_error("fs_opendir opendir");
		closedir(dp);
	}
	//////////////////////////////////

	fi->fh = (intptr_t) k;
	//fi->fh = (intptr_t) dp;

	log_fi(fi);

	ml.unlock();
	return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	char fpath[PATH_MAX];
	int pflag = !strcmp(path,"/");

	log_vmsg("\n");
	log_msg("fs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
		path, buf, filler, offset, fi);
	// once again, no need for fullpath -- but note that I need to cast fi->fh

	fs_state* b = FS_DATA;
	int k = (int) fi->fh;
	if(k==-1) return 0;


	DIR* dp;
	dirent* de;

	try{

		if(!strcmp(path, "/")) { // root ls

			b->v[k] = b->h->ls_root();
		} else {

			b->v[k] = b->h->ls_db(path+1);

			// append actual files from opendir, if not included
			fs_fullpath(fpath, path);
			dp=opendir(fpath);
			if(dp==NULL)
				return (retstat = fs_error("fs_readdir readdir"));

			// Every directory contains at least two entries: . and ..  If my
			// first call to the system readdir() returns NULL I've got an
			// error; near as I can tell, that's the only condition under
			// which I can get an error from readdir()
			de = readdir(dp);
			if (de == 0)
				return (retstat = fs_error("fs_readdir readdir"));

			// This will copy the entire directory into the buffer.  The loop exits
			// when either the system readdir() returns NULL, or filler()
			// returns something non-zero.  The first case just means I've
			// read the whole directory; the second means the buffer is full.
			do {
				if(strcmp(de->d_name, ".") && strcmp(de->d_name, "..")
					&& !checkdot(de->d_name)) {
					std::vector<std::string>& vv(b->v[k]);
					int j = 0;
					for(; j < vv.size(); j++)
						if(!strcmp(vv[j].c_str(), de->d_name)) break;
					if(j == vv.size()) {
						log_msg("+ fs_readdir: adding temporary file %s\n", de->d_name);
						vv.push_back(de->d_name);
					}
				}
			} while ((de = readdir(dp)) != NULL);
			closedir(dp);

		}

	}
    catch(...)
	{
		return -1;
	}

	std::vector<std::string>& v=b->v[k];

	if(buf){
		if (filler(buf, ".", NULL, 0) != 0) {
			log_vmsg(" + fs_readdir error: filler: buffer full");
			return -ENOMEM;
		}
		if (filler(buf, "..", NULL, 0) != 0) {
			log_vmsg(" + fs_readdir error: filler: buffer full");
			return -ENOMEM;
		}
	}
	for(size_t i=0;i<v.size();i++) {
		log_vmsg("+ fs_readdir: calling filler with name %s\n", v[i].c_str());
		if(buf) if (filler(buf, v[i].c_str(), NULL, 0) != 0) {
			log_vmsg(" + fs_readdir error: filler: buffer full");
			return -ENOMEM;
		}

		if(pflag) {
			log_vmsg("+ fs_readdir: mkdir path is: %s\n",
				(std::string(b->rootdir)+"/"+v[i]).c_str());
		    mkdir((std::string(b->rootdir)+"/"+v[i]).c_str(), DB_MKDIR_MODE);
		}
	}

    /*
    dp = (DIR *) (uintptr_t) fi->fh;

    // Every directory contains at least two entries: . and ..  If my
    // first call to the system readdir() returns NULL I've got an
    // error; near as I can tell, that's the only condition under
    // which I can get an error from readdir()
    de = readdir(dp);
    if (de == 0) {
	retstat = fs_error("fs_readdir readdir");
	return retstat;
    }

    // This will copy the entire directory into the buffer.  The loop exits
    // when either the system readdir() returns NULL, or filler()
    // returns something non-zero.  The first case just means I've
    // read the whole directory; the second means the buffer is full.
    do {
	log_msg("calling filler with name %s\n", de->d_name);
	if (filler(buf, de->d_name, NULL, 0) != 0) {
	    log_msg("    ERROR fs_readdir filler:  buffer full");
	    return -ENOMEM;
	}
    } while ((de = readdir(dp)) != NULL);
    */

	log_fi(fi);

	ml.unlock();
	return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int fs_releasedir(const char *path, struct fuse_file_info *fi)
{
	monolock ml;
	ml.lock();
	int retstat = 0;

	log_vmsg("\n");
	log_msg("fs_releasedir(path=\"%s\", fi=0x%08x)\n", path, fi);
	log_fi(fi);

	int k = (int) fi->fh;
	if(k!=-1)
		FS_DATA->r_key(k);

	//closedir((DIR *) (uintptr_t) fi->fh);

	ml.unlock();
	return retstat;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ???
int fs_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
	monolock ml;
	ml.lock();
	int retstat = 0;

	log_vmsg("\n");
	log_msg("fs_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n", path, datasync, fi);
	log_fi(fi);

	ml.unlock();
	return retstat;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *fs_init(struct fuse_conn_info *conn)
{
	log_vmsg("\n");
	log_msg("fs_init()\n");

	fuse_file_info a;
	fuse_fill_dir_t filler=0;//if any problem occurs delete =0
	fs_opendir("/",&a);
	fs_readdir("/",NULL,filler,0,&a);
	fs_releasedir("/",&a);

	return FS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void fs_destroy(void *userdata)
{
	log_vmsg("\n");
	log_msg("fs_destroy(userdata=0x%08x)\n", userdata);

	delete FS_DATA->h;
	//rmdir (FS_DATA->rootdir);
	tool::rdel (FS_DATA->rootdir);
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int fs_access(const char *path, int mask)
{
	monolock ml;
	ml.lock();
	int retstat = 0;
	char fpath[PATH_MAX];

	log_vmsg("\n");
	log_msg("fs_access(path=\"%s\", mask=0%o)\n", path, mask);
	fs_fullpath(fpath, path);

	retstat = access(fpath, mask);

	if (retstat < 0)
		retstat = fs_error("fs_access access");

	ml.unlock();
	return retstat;
}


////////////////////////////////////////////////////////////////////////////////
// END OF FILE
////////////////////////////////////////////////////////////////////////////////


