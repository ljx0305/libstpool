#piggy_xrh@163.com

.PHONY:LIBS clean

include complier.mk features.mk

INCLUDE =-I. -Ipool -Ipool/core -Ipool/com -Ipool/gp -Ipool/rt
CFLAGS += $(INCLUDE)

VPATH =.:pool:pool/core:pool/com:pool/gp:pool/rt:examples
OBJS_DIR :=.obj

DEF_TARGET  :=libmsglog.a libmsglog.so \
			libstpool.a libstpool.so  

OBJS_stpool := ospx.o  ospx_error.o timer.o\
			  sm_cache.o objpool.o cpool_core.o cpool_core_gc.o \
			  cpool_factory.o cpool_wait.o cpool_com_method.o \
			  cpool_rt_factory.o cpool_rt_core_method.o cpool_rt_method.o cpool_rt_extend_method.o cpool_rt_internal.o\
			  cpool_rt_scheduler_dump.o \
			  cpool_gp_factory.o cpool_gp_core_method.o cpool_gp_method.o cpool_gp_extend_method.o cpool_gp_advance_method.o\
			  cpool_gp_internal.o cpool_gp_entry.o cpool_gp_wait.o \
			  stpool.o stpool_group.o

OBJS_msglog := msglog.o
LIBS:PREPARE $(DEF_TARGET)

PREPARE:   
	@for d in $(OBJS_DIR) $(OBJS_DEMO_DIR); do \
		[ -d $$d ] || mkdir -p $$d; \
	done

libmsglog.a:$(addprefix $(OBJS_DIR)/, $(OBJS_msglog)) 
	$(AR) $(ARFLAGS) $@ $^ 
	chmod +x $@
ifeq ($(STRIP_LIB), yes)
	$(STRIP) $(STRIPFLAGS) $@
endif

libmsglog.so:$(addprefix $(OBJS_DIR)/, $(OBJS_msglog)) 
	$(CC) --shared -o  $@ $^
	chmod +x $@
ifeq ($(STRIP_LIB), yes)
	$(STRIP) $(STRIPFLAGS) $@
endif

libstpool.a:$(addprefix $(OBJS_DIR)/, $(OBJS_stpool)) libmsglog.a
	$(AR) $(ARFLAGS) $@ $(filter-out ligmsglog.a, $^)
	chmod +x $@
ifeq ($(STRIP_LIB), yes)
	$(STRIP) $(STRIPFLAGS) $@
endif

libstpool.so:$(addprefix $(OBJS_DIR)/, $(OBJS_stpool)) libmsglog.so
	$(CC) --shared -o  $@ $^ 
	chmod +x $@
ifeq ($(STRIP_LIB), yes)
	$(STRIP) $(STRIPFLAGS) $@
endif

EXAMPLE := $(addprefix examples/, demo demo-pri demo-sche demo-group demo-test)

demos: $(EXAMPLE)
all: LIBS demos

LDFLAGS +=-lpthread -lm
ifneq ($(filter -DHAVE_CLOCK_GETTIME, $(CFLAGS)), )
LDFLAGS += -lrt
endif

%demo:$(OBJS_DIR)/demo.o libstpool.a libmsglog.a
	$(CC) $^ $(LDFLAGS) -o $@ 

%demo-pri:$(OBJS_DIR)/demo-pri.o libstpool.a libmsglog.a
	$(CC) $^ $(LDFLAGS) -o $@ 

%demo-sche:$(OBJS_DIR)/demo-sche.o libstpool.a libmsglog.a
	$(CC) $^ $(LDFLAGS) -o $@ 

%demo-group:$(OBJS_DIR)/demo-group.o libstpool.a libmsglog.a
	$(CC) $^ $(LDFLAGS) -o $@ 

%demo-test:$(OBJS_DIR)/demo-test.o libstpool.a libmsglog.a
	$(CC) $^ $(LDFLAGS) -o $@ 


$(OBJS_DIR)/%.o:%.c
	$(CC) -c $(CFLAGS) -c $^ -o $@

$(OBJS_DIR)/%.o:%.cpp
	$(CPP) -c $(CPPFLAGS) -c $^ -o $@

demos-clean:
	-@rm $(EXAMPLE)

clean:
	-@rm $(OBJS_DIR)/*.o $(DEF_TARGET) $(EXAMPLE) *.o -fr
