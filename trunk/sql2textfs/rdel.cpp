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

#define _XOPEN_SOURCE 500
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "rdel.hpp"
#include <cstring>
#include <fstream>

namespace tool {

static int do_delete (const char *fpath, const struct stat *sb,
                        int tflag, struct FTW *ftwbuf)
{
    switch (tflag) {
        case FTW_D:
        case FTW_DNR:
        case FTW_DP:
            rmdir (fpath);
            break;
        default:
            unlink (fpath);
            break;
    }

    return (0);
}

int rdel (const char *argv)
{
    return (nftw (argv, do_delete, 20, FTW_DEPTH));
}


std::string readlin(const std::string& s, int mode)
{
    int sz = 1024 + s.size();
    int s1;
    char* buf=0;
    do{
      sz*=2;
      if(buf) free(buf);
      buf=(char*)calloc(sz,1);
      if(mode == 'r')
      	s1 = ::readlink(s.c_str(), buf, sz-1);
      else
        { strcpy(buf, s.c_str()); s1 = s.size(); }
    }
    while(s1>=sz-1);
    buf[s1]=0;
    if(mode=='d') {
      char*c =strrchr(buf,'/');
      if(c) *c=0;
    }
    
    std::string r = buf;
    free(buf);
    return r;   
}

bool exist(const std::string& s)
{
	std::ifstream i(s.c_str());
	return i.good();
}

}

