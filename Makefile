PMAJOR = 3
PMINOR = 6
REL = y$(PMAJOR)b$(PMINOR)
VER = 0.7-457$(REL)
DIR = ax7z-$(VER)

.PHONY: release dist build mkpatch tag gtag retag test

release: build dist

release.h: Makefile release.h.in
	sed -e 's/@PMAJOR@/$(PMAJOR)/;s/@PMINOR@/$(PMINOR)/;s/@REL@/$(REL)/' release.h.in > release.h

dist: mkpatch
	rm -rf $(DIR)
	mkdir -p $(DIR)
	cp -p Release/ax7z.spi ax7z.txt $(DIR)
	(cd $(DIR); zip ../$(DIR).zip *)
	rm -rf $(DIR)
	
build: release.h
	/cygdrive/c/Program\ Files\ \(x86\)/Application/Microsoft\ Visual\ Studio\ 11.0/Common7/IDE/devenv.exe ./00am.sln /Build Release

rebuild: release.h
	/cygdrive/c/Program\ Files\ \(x86\)/Application/Microsoft\ Visual\ Studio\ 11.0/Common7/IDE/devenv.exe ./00am.sln /Rebuild Release

dbuild: release.h
	/cygdrive/c/Program\ Files\ \(x86\)/Application/Microsoft\ Visual\ Studio\ 11.0/Common7/IDE/devenv.exe ./00am.sln /Build Debug

drebuild: release.h
	/cygdrive/c/Program\ Files\ \(x86\)/Application/Microsoft\ Visual\ Studio\ 11.0/Common7/IDE/devenv.exe ./00am.sln /Rebuild Debug

mkpatch:
	rm -f $(DIR).patch
	-env LANG=C diff -urN -X diff-exclude.txt /var/tmp/ax7z_src-orig . > $(DIR).patch
	-env LANG=C diff -u /var/tmp/ax7z_src-orig/7z/7zip/UI/Common/OpenArchive.cpp 7z/7zip/UI/Common/OpenArchive.cpp >> $(DIR).patch
	-env LANG=C diff -u /var/tmp/ax7z_src-orig/7z/7zip/UI/Common/LoadCodecs.cpp 7z/7zip/UI/Common/LoadCodecs.cpp >> $(DIR).patch

tag:
	git tag $(DIR)

retag:
	git tag -f $(DIR)

sqlite3/sqlite3.o: sqlite3/sqlite3.c

test: test.cpp sqlite3/sqlite3.o
	g++ -Wall -o test test.cpp sqlite3/sqlite3.o
