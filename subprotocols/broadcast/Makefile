#Shell
SHELL = /bin/bash

#Executeable name
NAME = broadcast

#Compiler
CC = gcc

#Debug or Release
PROFILE = -Og -g -DNDEBUG
DEBUG = -Og -g
RELEASE = -O3 -funroll-loops -DNDEBUG
SPACE = -Os -DNDEBUG
EXEC = $(RELEASE)

#Compiler options
CFLAGS = $(EXEC) \
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
		 -Wuninitialized \
		 -D_DEFAULT_SOURCE

CVER = -std=c11

# Folders
FOLDER = $(shell pwd)
SUBPROTOCOLS_FOLDER = $(FOLDER)/../

# Include folders
INCLUDES = -I$(SUBPROTOCOLS_FOLDER)

# Files
SRC = $(shell find $(FOLRDER) -name '*.c' -type f;)
SRC_OBJ  = $(patsubst %.c, %.o, $(SRC))
DEPS = $(SRC_OBJ:%.o=%.d)

.PHONY: clean release debug profiling space

#what we are trying to build
all: $(NAME)

release_mode:
	$(eval EXEC = $(RELEASE))

debug_mode:
	$(eval EXEC = $(DEBUG))

profiling_mode:
	$(eval EXEC = $(PROFILE))

space_mode:
	$(eval EXEC = $(SPACE))

# Recompile when headers change
-include $(DEPS)

#linkage
$(NAME): $(SRC_OBJ)
	@echo 
	@echo ================ [Creating Shared Object] ================ 
	@echo
	$(CC) -shared $(CFLAGS) $(CVER) -o $(NAME).so $(SRC_OBJ) $(INCLUDES)
	@echo
	@echo ================ [$(NAME).so compiled succesfully] ================ 
	@echo

# compile every source file
%.o: %.c
	@echo
	@echo ================ [Building Object] ================
	@echo
	$(CC) $(CFLAGS) $(CVER) -c $< -o $@ $(INCLUDES)
	@echo
	@echo OK [$<] - [$@]
	@echo

#make clean
clean:
	@echo
	@echo ================ [Cleaning $(NAME)] ================
	@echo
	rm -f *.d
	rm -f *.o
	rm -f *.so

#make release
release: clean release_mode all

#make debug
debug: clean debug_mode all

#make profiling
profiling: clean profiling_mode all

#make space
space: clean space_mode all
