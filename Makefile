VER = 0.7-457y2b0
DIR = ax7z-$(VER)

dist: release mkpatch
	rm -rf $(DIR)
	mkdir -p $(DIR)
	cp Release/ax7z.spi ax7z.txt $(DIR)
	(cd $(DIR); zip ../$(DIR).zip *)
	rm -rf $(DIR)
	
release:
	/cygdrive/c/Program\ Files/Application/Microsoft\ Visual\ Studio\ .NET\ 2003/Common7/IDE/devenv.exe ./00am.sln /Build Release

mkpatch:
	rm -f $(DIR).patch
	-env LANG=C diff -urN -X diff-exclude.txt /var/tmp/ax7z_src-orig . > $(DIR).patch

tag:
	svn copy . https://yak.myhome.cx/repos/source/ax7z/tags/$(DIR)

retag:
	svn remove https://yak.myhome.cx/repos/source/ax7z/tags/$(DIR)
	svn copy . https://yak.myhome.cx/repos/source/ax7z/tags/$(DIR)

