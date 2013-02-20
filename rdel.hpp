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

#ifndef RDEL_TOOL_INCLUDED
#define RDEL_TOOL_INCLUDED


#include <string>


namespace tool {

int rdel (const char *argv);
std::string readlin(const std::string& s, int mode ='r');
bool exist(const std::string& s);

}

#endif

