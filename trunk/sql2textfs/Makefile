
# parameters

# installation dirs, for binaries and configuration files
INSTPATH=/usr/local/
ETCPATH=/

CFLAGS=-Os -fPIC
RETRANSE_FLAGS=-I ../retranse
RETRANSE_LIBS=-l:../retranse/libretranse.a -lpcre++ -lpcre
CPPDB_FLAGS=-I ../cppdb
CPPDB_LIBS=-L ../cppdb/lib -lcppdb
#CPPDB_STATIC=-L ../cppdb/lib -l:libcppdb.a -lpthread -lmysqlclient -lpq -lodbc
CPPDB_STATIC=-L ../cppdb/lib -l:libcppdb.a -lodbc
SQL2TEXT_FLAGS=-I ../sql2text/
SQL2TEXT_LIBS=-l:../sql2text/libsql2text.a
FUSE_FLAGS=`pkg-config fuse --cflags`
FUSE_LIBS=`pkg-config fuse --libs`
FLAGS=$(CPPDB_FLAGS) $(SQL2TEXT_FLAGS) $(RETRANSE_FLAGS) $(FUSE_FLAGS)
LIBS=$(CPPDB_STATIC) $(SQL2TEXT_LIBS) $(RETRANSE_LIBS) $(FUSE_LIBS)

# all

.PHONY: all

all: sql2textmount


DEPENDENCIES=lib/sql2text/shared/libcppdb_mysql.so lib/sql2text/shared/libcppdb_postgresql.so lib/sql2text/shared/libcppdb_sqlite3.so 

lib/sql2text/shared/libcppdb_mysql.so: 
	cp ../cppdb/lib/libcppdb_mysql.so lib/sql2text/shared

lib/sql2text/shared/libcppdb_postgresql.so: 
	cp ../cppdb/lib/libcppdb_postgresql.so lib/sql2text/shared

lib/sql2text/shared/libcppdb_sqlite3.so: 
	cp ../cppdb/lib/libcppdb_sqlite3.so lib/sql2text/shared

CONFIGURATION=etc/sql2text/config.ret

etc/sql2text/config.ret:
	cp -r ../sql2text/etc .
	
#main targets

sql2textmount : sql2textmount.o fuse.o log.o rdel.o $(DEPENDENCIES) $(CONFIGURATION)
	g++ -o sql2textmount $(FLAGS) -g sql2textmount.o fuse.o log.o rdel.o $(LIBS)

.cpp.o: 
	$(CXX) $(FLAGS) -c $<


clean:
	rm -f *.o

cleanall: clean
	rm -f lib/sql2text/shared/* sql2textmount


.PHONY: install, uninstall

install: all
	cp -r lib $(INSTPATH)
	cp -r etc $(ETCPATH)
	cp sql2textmount $(INSTPATH)bin/

uninstall:
	rm -f $(INSTPATH)bin/sql2textmount
	rm -rf $(ETCPATH)etc/sql2text
	rm -rf $(INSTPATH)lib/sql2text


