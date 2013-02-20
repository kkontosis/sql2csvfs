/*  sql2textfs, a FUSE filesystem for mounting database tables as text files 
 *  Copyright (C) 2012, Kimon Kontosis
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

#ifndef _LOG_H_
#define _LOG_H_
#include <stdio.h>

//  macro to log fields in structs.
#define log_struct(st, field, format, typecast) \
  log_msg("    " #field " = " #format "\n", typecast st->field)

FILE *log_open(const char* fname);
void log_fi (struct fuse_file_info *fi);
void log_stat(struct stat *si);
void log_statvfs(struct statvfs *sv);
void log_utime(struct utimbuf *buf);

void log_msg(const char *format, ...);
void log_vmsg(const char *format, ...);
#endif
