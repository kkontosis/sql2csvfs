#!/bin/bash

# sql2textfs builder script
#
# This script notifies about dependencies,
# downloads some dependencies and builds sql2textfs and library
# For quick help, run with `--help`
#
# script written by Kimon Kontosis


all=
updatesrc=
updateall=
cleanall=

depends=1
haserror=0
passeddeps=0

cppdb_repo="https://github.com/kkontosis/cppdb.git"
retranse_repo="https://github.com/kkontosis/retranse.git"
sql2text_repo="https://github.com/kkontosis/sql2text.git"
sql2textfs_repo="https://github.com/kkontosis/sql2csvfs.git"

function green { echo -en '\E[37;32m'"\033[1m"; }
function red { echo -en '\E[37;31m'"\033[1m"; }
function bold { echo -en "\033[1m"; }
function thin { echo -en "\033[0m"; }
function normal { tput sgr0; }

debpkg="g++ binutils make subversion cmake pkg-config libfuse-dev fuse-utils libpcre++-dev unixodbc-dev libmysqlclient-dev libpq-dev libsqlite3-dev"

function tstapt 
{
	echo ""
	echo apt-based system detected. Generating package suggestions...
	r=
	set - $debpkg
	while [ $# -gt 0 ]
	do
		na=0
		dpkg -s $1 2>/dev/null | grep installed | grep -v not-installed >/dev/null && na=1
		test $na -eq 0 && r="$r $1"
		shift
	done
	if [ "$r" == "" ]
	then
		echo Suggested packages seem to be installed.
		exit 1
	fi
	
	echo "Suggested packages are:$r"
	echo "please try:"
	echo "	sudo apt-get install$r"
	
	exit 1
}
function chkapt { if [ "$passeddeps" == "1" ]; then exit 1; fi; apt-get --help 2>/dev/null >/dev/null && tstapt; }
function ok { green; echo -e "\t[ok]"; normal; }
function erhelp { echo "To see all the errors for missing dependencies type $0 --showall"; }
function error { red; echo ""; echo -en ""; echo "error: $@"; normal; if [ "$depends" == "1" -a "$all" == "1" ]; then haserror=1; else erhelp; chkapt; exit 1; fi; }

function enable-later {  echo $@; }
function check-lib { echo -e "$2 $3 $4 $5 $6 $7\nint main(){}" | g++ -o /dev/null -x c++ - -l$1 2>/dev/null; }
function check-libstd { echo -e "#include<sstream>\nint main(){}" | g++ -o /dev/null -x c++ - 2>/dev/null; }
function check-fuse { echo -e "#include<fuse.h>\nint main(){}" | g++ -D FUSE_USE_VERSION=25 -o /dev/null -x c++ - $(pkg-config --cflags --libs fuse) 2>/dev/null; }
function check-libpq { echo -e "#ifdef __APPLE__\n#include <libpq-fe.h>\n#else\n#include <postgresql/libpq-fe.h>\n#endif\nint main(){}" | g++ -o /dev/null -x c++ - `pkg-config --cflags libpq 2>/dev/null` -lpq 2>/dev/null; }
function check-libmysqlclient { echo -e "#ifdef __APPLE__\n#include <mysql.h>\n#else\n#include <mysql/mysql.h>\n#endif\nint main(){}" | g++ -o /dev/null -x c++ - `pkg-config --cflags mysqlclient 2>/dev/null` -lpq 2>/dev/null; }
function check-bin { $@ >/dev/null 2>/dev/null; }

function test-fuse
{
  bold; echo -n "Checking if library fuse is installed...  "; thin
  if check-fuse $@
  then
    ok
  else
    error library \"fuse\" is not installed. Please install the development package of this library using your distribution\'s package manager.
  fi
}
function test-lib-custom
{
  bold; echo -n "Checking if library $1 is installed...  "; thin
  if $2
  then
    ok
  else
    varname="disable_$1"
    if [ "${!varname}" == "1" ]
    then
      echo -e "\t[disabled]";
    else
      error "library \"$1\" is not installed. Please install the development package of this library using your distribution's package manager. To disable support for this library please set the environment variable '$varname'. For example: "'$'" $varname=1 $0;"
    fi
  fi
}
function test-lib
{
  bold; echo -n "Checking if library $1 is installed...  "; thin
  if check-lib $@
  then
    ok
  else
    error library \"$1\" is not installed. Please install the development package of this library using your distribution\'s package manager.
  fi
}
function option-lib
{
  bold; echo -n "Checking if library $1 is installed...  "; thin
  if check-lib $@
  then
    ok
  else
    varname="disable_$1"
    if [ "${!varname}" == "1" ]
    then
      echo -e "\t[disabled]";
    else
      error "library \"$1\" is not installed. Please install the development package of this library using your distribution's package manager. To disable support for this library please set the environment variable '$varname'. For example: "'$'" $varname=1 $0;"
    fi
  fi
}
function test-bin
{
  bold; echo -n "Checking if program $1 is installed...  "; thin
  if check-bin $@
  then
    ok
  else
    error program \"$1\" does not seem to be installed. Please install it using your distribution\'s package manager.
  fi
}
function test-bin-fuse
{
  if [[ "$(uname -s)" == "Darwin" ]]
  then
    return 0
  fi

  bold; echo -n "Checking if fuse-utils are installed...  "; thin
  if check-bin fusermount -V
  then
    ok
  else
    error "the package fuse-utils (fusermount) does not seem to be installed. Please install it using your distribution's package manager."
  fi
}

echo -n Welcome to ; bold; echo -n " sql2textfs " ; thin; echo source builder.
echo ""
echo -n This script will download and install all external libraries needed to 
echo " compile"
bold; echo -n "sql2textfs. " ; thin;
echo "Note that the certain system libraries are required."
echo -n "For installing those libraries "
echo -n please 
echo " refer to your distribution package"
echo -n installers. Alternatively the required libraries can be downloaded from
echo " their"
echo -n official site, built from source code and installed following the
echo " instructions"
echo that come with their source code.
echo To display a list of all the options of this script use the argument --help
echo ""

if [ "$1" == "--showall" ]; then all=1; fi
if [ "$1" == "--updatesrc" ]; then updatesrc=1; fi
if [ "$1" == "--updateall" ]; then updatesrc=1; updateall=1; fi
if [ "$1" == "--cleanall" ]; then cleanall="cleanall"; fi
if [ "$1" == "--install" ]; then cleanall="install"; fi
if [ "$1" == "--help" ]; then
	echo Syntax:
	echo $0 [options]
	echo ""
	echo options:
	echo ""
	 echo --help"		"Show this help
	 echo --showall"	"Show all dependency errors upon failure
	 echo --install"	"Install to /usr/local/
	 echo --cleanall"	"Clean all binaries
	 echo --updatesrc"	"Update sources from online repositories
	 echo --updateall"	"Update all sources including cppdb
	echo ""
	exit 0
fi

function srcbackup {
	if [ "$(ls $1)" ]
	then
		echo creating backup of $1 in $1.old ...
		rm -rf $1.old
		cp -r $1 $1.old
	fi
}
function svnuprep {
                ( cd $1 && git pull ) || error "Cannot git pull $1. Is subversion updated? In there an internet connection? Should the repository not exist, please update $1 manually.";
}

function dellall {
# the name of this function is misleading...!
	echo "Checking $1 for source updates..."

	if test -e $1/.git
	then
		svnuprep $1
	else
		#if svn ls $1 >/dev/null 2>/dev/null
		#then
		#	svnuprep $1
		#else	

			srcbackup $1
			rm -rf $1/*
			rm -rf $1/.git
		#fi
	fi
}

if [ "$updateall" == "1" ]
then
#	echo not implemented
#	exit 1
#	srcbackup cppdb
	dellall cppdb
fi

if [ "$updatesrc" == "1" ]
then
#	echo not implemented
#	exit 1
#	srcbackup retranse
	dellall retranse
#	srcbackup sql2text
	dellall sql2text
#	srcbackup sql2textfs
	dellall sql2textfs
fi


test-bin g++ --version
test-bin make --version
test-bin cmake --version
test-bin pkg-config --version
test-bin git  --version
test-bin-fuse

echo ""

bold; echo -n Checking if standard c++ development library is installed...; thin
if check-libstd; then ok; else error An appropriate version of the C++ standard library does not seem to be installed. Please install the development package of libstd++ using your distribution\'s package manager.  ; fi

test-lib pcre '#include <pcre.h>'
test-lib pcre++ '#include <pcre++.h>'
test-fuse
test-lib-custom pq check-libpq
test-lib-custom libmysqlclient check-libmysqlclient

option-lib sqlite3 '#include <sqlite3.h>'
option-lib odbc '#include <sqlext.h>'

echo ""
if [ "$haserror" -ne 0 ] ; then error 'Some requirements were not met.'; exit 1; fi

passeddeps=1

echo "Requirements met. Proceeding with installation..."
echo ""

if check-lib cppdb; then
  bold; echo -n Found cppdb library development package...; thin; ok
else
  if ! test -f cppdb/cppdb/frontend.h; then
    bold; echo Checking out cppdb source code...; thin
    git clone "$cppdb_repo" cppdb || { error "Cannot check out cppdb. Is subversion updated? In there an internet connection? Should the repository not exist, please download and install cppdb manually."; }
  fi

  bold; echo -n cppdb source code; thin; ok

  echo Compiling cppdb...
  { cd cppdb && mkdir -p lib && cd lib && cmake .. && make && cd ../..; } || { error "Could not compile cppdb."; }

  bold; echo -n built cppdb from source; thin; ok
fi

if [ "$cleanall" == "cleanall" ] 
then 
  echo Cleaning all cppdb binaries...
  rm -rf cppdb/lib; 
  bold; echo -n cleaned cppdb binaries; thin; ok
  echo ""
  echo Cleaning all binaries in the following projects...
fi


#
echo ""

if ! test -f retranse/Makefile; then
  bold; echo Checking out retranse source code...; thin
    git clone "$retranse_repo" retranse || { error "Cannot check out retranse. Is subversion updated? In there an internet connection? Should the repository not exist, please download and install retranse manually."; }
fi

bold; echo Compiling retranse...; thin;
{ cd retranse && make $cleanall && cd ..; } || error "Could not compile retranse."; 

bold; echo -n built libretranse; thin; ok

#

echo ""

if ! test -f sql2text/Makefile; then
  bold; echo Checking out sql2text source code...; thin
    git clone "$sql2text_repo" sql2text || { error "Cannot check out sql2text. Is subversion updated? In there an internet connection? Should the repository not exist, please download and install sql2text manually."; }
    ( cd sql2text ; git pull ; cd .. )
fi

bold; echo Compiling sql2text...; thin;
{ cd sql2text && make $cleanall && cd ..; } || error "Could not compile sql2text."; 

bold; echo -n built libsql2text; thin; ok

#

echo ""

if ! test -f sql2textfs/Makefile; then
  bold; echo Checking out sql2textfs source code...; thin
    git clone "$sql2textfs_repo" sql2textfs || { error "Cannot check out sql2textfs. Is subversion updated? In there an internet connection? Should the repository not exist, please download and install sql2textfs manually."; }
    ( cd sql2textfs ; git pull ; cd .. )
fi

bold; echo Compiling sql2textfs...; thin;
{ cd sql2textfs && make $cleanall && cd ..; } || error "Could not compile sql2textfs.";

bold; echo -n built sql2textmount; thin; ok



