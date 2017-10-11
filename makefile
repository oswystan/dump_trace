#######################################################################
##                     Copyright (C) 2017 wystan
##
##       filename: makefile
##    description:
##        created: 2017-07-06 11:42:13
##         author: wystan
##
#######################################################################
.PHONY: all clean

bin := main
src := main.c debug_trace.c
obj := $(src:.c=.o)
obj := $(obj:.cpp=.o)
ld_flags := -lunwind -lunwind-$(shell uname -m) -lpthread

shared_src := libshared.c
shared_obj := $(shared_src:.c=.o)
shared_name := libshared.so

all: $(bin)

$(bin): $(obj) $(shared_name)
	@gcc $^ -o $(bin) $(ld_flags)
	@echo "[gen] "$@

$(shared_name): $(shared_obj)
	@echo "[gen] $@"
	@gcc -shared $^ -o $@

%.o:%.c
	@echo "[ cc] "$@
	@gcc -c -g -fPIC $< -o $@
%.o:%.cpp
	@echo "[cpp] "$@
	@g++ -c $< -o $@

clean:
	@echo "cleaning..."
	@rm -f *.o $(bin) $(shared_name)
	@echo "done."

test:
	@LD_LIBRARY_PATH=. ./main
#######################################################################
