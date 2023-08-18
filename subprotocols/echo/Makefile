#Shell
SHELL = /bin/bash

#Executeable name
NAME = echo

#Compiler
CC = gcc

# Folders
FOLDER = $(shell pwd)
SUBPROTOCOLS_FOLDER = $(FOLDER)/../
INCLUDE_FOLDER = $(FOLDER)/../../include

#Debug or Release
PROFILING = -Og -g -DNDEBUG
DEBUG = -Og -g --param max-inline-insns-single=1000
RELEASE = -O3 -funroll-loops -DNDEBUG
SPACE = -Os -DNDEBUG
MODE = "release"
PREV_MODE = "$(shell cat $(BUILD_FOLDER)/.mode 2> /dev/null)" 
EXEC = $(RELEASE)

#Compiler options
CFLAGS = $(EXEC) \
		 -pedantic \
		 -pedantic-errors \
		 -fno-exceptions \
		 -fPIC \
		 -fstack-protector -Wl,-z,relro -Wl,-z,now \
		 -fvisibility=hidden \
		 -march=native \
		 -MMD \
		 -pipe \
		 -W \
		 -Wall \
		 -Werror \
		 -Wextra \
		 -Wformat \
		 -Wformat-security \
		 -Wformat-nonliteral \
		 -Winit-self \
		 -Winline \
		 -Wmultichar \
		 -Wpointer-arith \
		 -Wreturn-type \
	     -Wsign-compare \
		 -Wuninitialized \
		 -D_GNU_SOURCE \
		 -D_DEFAULT_SOURCE

# Compiler version
CVER = -std=c11

# Flags
FLAGS_INCLUDES = -I$(SUBPROTOCOLS_FOLDER) -I$(INCLUDE_FOLDER)

# Files
SRC = $(shell find $(FOLRDER) -name '*.c' -type f;)
SRC_OBJ  = $(patsubst %.c, %.o, $(SRC))
DEPS = $(SRC_OBJ:%.o=%.d)

.PHONY: clean release debug profiling space

#what we are trying to build
all: $(NAME)

release_mode:
	$(eval EXEC = $(RELEASE))
	$(eval MODE = "release")

debug_mode:
	$(eval EXEC = $(DEBUG))
	$(eval MODE = "debug")

profiling_mode:
	$(eval EXEC = $(PROFILING))
	$(eval MODE = "profiling")

space_mode:
	$(eval EXEC = $(SPACE))
	$(eval MODE = "space")

# Recompile when headers change
-include $(DEPS)

#linkage
$(NAME): $(SRC_OBJ)
	@echo 
	@echo ================ [Creating Shared Object] ================ 
	@echo
	$(CC) -shared $(CFLAGS) $(CVER) $(FLAGS_INCLUDES) -o $(NAME).so $(SRC_OBJ)
	@echo
	@echo ================ [$(NAME).so compiled succesfully] ================ 
	@echo

# compile every source file
%.o: %.c
	@echo
	@echo ================ [Building Object] ================
	@echo
	$(CC) $(CFLAGS) $(CVER) -c $< -o $@ $(FLAGS_INCLUDES)
	@echo
	@echo OK [$<] - [$@]
	@echo

#make clean
clean:
	@echo
	@echo ================ [Cleaning $(NAME)] ================
	@echo
	if [ $(PREV_MODE) != $(MODE) ]; then \
		rm -f *.d; \
		rm -f *.o; \
	fi
	rm -f *.so

#make release
release: release_mode clean all

#make debug
debug: debug_mode clean all

#make profiling
profiling: profiling_mode clean all

#make space
space: space_mode clean all
