rem For Visual Studio 2005
rem set VS80COMNTOOLS="C:\Program Files\Microsoft Visual Studio 8\Common7\Tools\"
call "%VS80COMNTOOLS%\vsvars32.bat"
devenv iTunesWeb.sln /rebuild Release /project iTunesWebInstaller
rem devenv iTunesWeb.sln /rebuild Debug /project iTunesWeb

exit 0
rem For Visual Studio Express Edition 2008
rem set VS90COMNTOOLS=C:\Program Files\Microsoft Visual Studio 9.0\Common7\Tools\
call "%VS90COMNTOOLS%\vsvars32.bat"
vcbuild iTunesWeb.vcproj
