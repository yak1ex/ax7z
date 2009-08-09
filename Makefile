VER = 0.7-457y3
DIR = ax7z-$(VER)

dist: mkpatch
	rm -rf $(DIR)
	mkdir -p $(DIR)
	cp -p Release/ax7z.spi ax7z.txt $(DIR)
	(cd $(DIR); zip ../$(DIR).zip *)
	rm -rf $(DIR)
	
build:
	/cygdrive/c/Program\ Files/Application/Microsoft\ Visual\ Studio\ .NET\ 2003/Common7/IDE/devenv.exe ./00am.sln /Rebuild Release

mkpatch:
	rm -f $(DIR).patch
	-env LANG=C diff -urN -X diff-exclude.txt /var/tmp/ax7z_src-orig . > $(DIR).patch
	-env LANG=C diff -u /var/tmp/ax7z_src-orig/7z/7zip/UI/Common/OpenArchive.cpp 7z/7zip/UI/Common/OpenArchive.cpp >> $(DIR).patch
	-env LANG=C diff -u /var/tmp/ax7z_src-orig/7z/7zip/UI/Common/LoadCodecs.cpp 7z/7zip/UI/Common/LoadCodecs.cpp >> $(DIR).patch

tag:
	svn copy . https://yak.myhome.cx/repos/source/ax7z/tags/$(DIR)

retag:
	svn remove https://yak.myhome.cx/repos/source/ax7z/tags/$(DIR)
	svn copy . https://yak.myhome.cx/repos/source/ax7z/tags/$(DIR)

sqlite3/sqlite3.o: sqlite3/sqlite3.c

test: test.cpp sqlite3/sqlite3.o
	g++ -Wall -o test test.cpp sqlite3/sqlite3.o
