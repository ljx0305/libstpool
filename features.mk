# LIB mode type (Debug/Release)
LIB_MODE = Debug

# OS type (LINUX/ARM, OSX, NDK)
OS_TYPE  = LINUX

# OS width (32/64)
OS_WIDTH = 32

# Strip the library (yes/no)
STRIP_LIB = no

#------------------------------------------
FEATURES_FLAGS :=-DOS_T_$(OS_TYPE) -DOS_W_$(OS_WIDTH) 

#OS: LINUX/ARM, NDK(android), OSX
ifneq ($(OS_TYPE), NDK) 
FEATURES_FLAGS +=-DHAVE_PTHREAD_RWLOCK -DHAVE_PTHREAD_ATTR_GETINHERITSCHED  
endif

ifneq ($(OS_TYPE), OSX) 
FEATURES_FLAGS +=-DHAVE_CLOCK_GETTIME -DHAVE_CLOCK_MONOTONIC -DHAVE_SEM_TIMEDWAIT \
	-DHAVE_PRCTL  
endif

ifeq ($(LIB_MODE), Debug)  
FEATURES_FLAGS  +=-g -D_DEBUG -D_GNU_SOURCE -fPIC  
STRIP_LIB = no  
else
FEATURES_FLAGS  +=-O2 -DNDEBUG -D_GNU_SOURCE -fPIC  
endif

ifeq ($(OS_WIDTH), 64)
FEATURES_FLAGS += -D_OS_WIDTH_TYPE_64 -m64 
else
FEATURES_FLAGS += -D_OS_WIDTH_TYPE_32  
endif

ifeq ($(LIBTEVENT_CONFIG_POOL_TIMER), yes)   
FEATURES_FLAGS += -DCONFIG_POOL_GROUP_TIMER    
endif
#------------------------------------------

CFLAGS += $(FEATURES_FLAGS)   
CPPFLAGS += $(FEATURES_FLAGS)   

