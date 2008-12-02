rem For Visual Studio 2003
rem devenv iTunesWeb.sln /rebuild Release /project iTunesWebInstaller
rem devenv iTunesWeb.sln /rebuild Debug

rem For Visual Studio Express Edition 2008
rem set VS90COMNTOOLS=C:\Program Files\Microsoft Visual Studio 9.0\Common7\Tools\
call "%VS90COMNTOOLS%\vsvars32.bat"
vcbuild iTunesWeb.vcproj
