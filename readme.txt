How to complier the project ?

WINDOWS:
	stpool_win_proj (vs2008)

LINUX/MAC
   ./configure
   make && make install

ARM
  ./configure --host=cross-complier-
  make && make install

NDK
   ./configure
   ndk-build
  

