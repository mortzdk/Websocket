#Shell
SHELL = /bin/bash

#Executeable name
NAME = echo

#Compiler
CC = gcc

#Compiler options
CFLAGS = -O3 -DNDEBUG \
		 -fno-exceptions \
		 -fPIC \
		 -fstack-protector \
		 -funroll-loops \
		 -fvisibility=hidden \
		 -march=native \
		 -MMD \
		 -pedantic \
		 -pedantic-errors \
		 -pipe \
		 -W \
		 -Wall \
		 -Werror \
		 -Wformat \
		 -Wformat-security \
		 -Wformat-nonliteral \
		 -Winit-self \
		 -Winline \
		 -Wl,-z,relro \
		 -Wl,-z,now \
		 -Wmultichar \
		 -Wno-unused-parameter \
		 -Wno-unused-function \
		 -Wno-unused-label \
		 -Wno-deprecated \
		 -Wno-strict-aliasing \
		 -Wpointer-arith \
		 -Wreturn-type \
	     -Wsign-compare \
		 -Wuninitialized

CVER = -std=c99

# Folders
FOLDER = $(shell pwd)

# Files
SRC = $(shell find $(FOLRDER) -name '*.c' -type f;)
SRC_OBJ  = $(patsubst %.c, %.o, $(SRC))
DEPS = $(SRC_OBJ:%.o=%.d)

#what we are trying to build
all: $(NAME)

# Recompile when headers change
-include $(DEPS)

#linkage
$(NAME): $(SRC_OBJ)
	@echo 
	@echo ================ [Creating Shared Object] ================ 
	@echo
	$(CC)  -shared $(CFLAGS) $(CVER) -o $(NAME).so $(SRC_OBJ)
	@echo
	@echo ================ [$(NAME).so compiled succesfully] ================ 
	@echo

# compile every source file
%.o: %.c
	@echo
	@echo ================ [Building Object] ================
	@echo
	$(CC) $(CFLAGS) $(CVER) -c $< -o $@
	@echo
	@echo OK [$<] - [$@]
	@echo

#make clean
clean:
	@echo
	@echo ================ [Cleaning $(NAME)] ================
	@echo
	rm *.d
	rm *.o
	rm *.so
