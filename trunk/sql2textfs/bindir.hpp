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

#ifndef SQL2TEXT_BINDIR_INCLUDED
#define SQL2TEXT_BINDIR_INCLUDED

#include <string>
#include <string.h>

static std::string get_bin_dir(const char* argv0)
{
    using namespace std;
    string result;
    // does not consider PATH
    const char* sp;
    if(!(sp=strrchr(argv0,'/'))) return "";
    const char *p = argv0;
    string r;
    while(p != sp) r.push_back(*p++);
    return r;

    /*
    // alternative way
    // (indlude rdel.hpp)
    std::string s = tool::readlin("/proc/self/exe");
    s=tool::readlin(s, 'd');
    */
}

static std::string get_file_dir(const char* s)
{
    return get_bin_dir(s);
}

#endif // include guard

