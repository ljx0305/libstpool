How to complier the project ?

WINDOWS:
	stpool_win_proj (vs2008)

LINUX:
	.Configure the env in the features.mk
     (
	 	OS_TYPE = LINUX
		...
	 )
  
	. make clean; make all

NDK:
  .Configure the env in the features.mk
     (
	 	OS_TYPE = NDK
		...
	 )
  
  .ndk-build


MAC:
   .Configure the env in the features.mk
     (
	 	OS_TYPE = OSX
		...
	)
   
   . make clean; make all
  

