#Shell
SHELL = /bin/bash 
#Executeable name
NAME = WSServer

#Compiler
CC = gcc

#Version
VER = $(shell git describe --abbrev=0 --tags && echo $? || echo "2.0.0")

#Debug or Release
PROFILE = -Og -g -DNDEBUG
DEBUG = -Og -g
RELEASE = -O3 -funroll-loops -DNDEBUG
SPACE = -Os -DNDEBUG
MODE = "release"
EXEC = $(RELEASE)

#Compiler options
CFLAGS = $(EXEC) \
		 -fno-exceptions \
		 -fPIC \
		 -fstack-protector \
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
		 -DWSS_SERVER_VERSION=\"$(VER)\" \
		 -D_DEFAULT_SOURCE \
		 -DUSE_RPMALLOC

CVER = -std=c11

# Flags
FLAGS_EXTRA = -pthread -lm -ldl
FLAGS_CRITERION = -lcriterion

# Folders
ROOT = $(shell pwd)
LOG_FOLDER = $(ROOT)/log
BUILD_FOLDER = $(ROOT)/build
BIN_FOLDER = $(ROOT)/bin
SRC_FOLDER = $(ROOT)/src
INCLUDE_FOLDER = $(ROOT)/include
DOCS_FOLDER = $(ROOT)/docs
TEST_FOLDER = $(ROOT)/test
CONF_FOLDER = $(ROOT)/conf
REPORTS_FOLDER = $(ROOT)/reports
EXTENSIONS_FOLDER = $(ROOT)/extensions
SUBPROTOCOLS_FOLDER = $(ROOT)/subprotocols

# Include folders
INCLUDES = -I$(INCLUDE_FOLDER) -I$(SRC_FOLDER) -I$(EXTENSIONS_FOLDER) -I$(SUBPROTOCOLS_FOLDER)

# Files
SRC = $(shell find $(SRC_FOLDER) -name '*.c' -type f;)
TESTS = $(shell find $(TEST_FOLDER) -name 'test_*.c' -type f;)
SRC_OBJ  = $(subst $(SRC_FOLDER), $(BUILD_FOLDER), $(patsubst %.c, %.o, $(SRC)))
TEST_OBJ = ${subst ${TEST_FOLDER}, ${BUILD_FOLDER}, ${patsubst %.c, %.o, $(TESTS)}}
ALL_OBJ  = ${SRC_OBJ} ${TEST_OBJ}
TEST_NAMES = ${patsubst ${TEST_FOLDER}/%.c, %, ${TESTS}}
DEPS = $(ALL_OBJ:%.o=%.d)

$(shell pkg-config --exists openssl)
ifeq ($(.SHELLSTATUS), 0)
	FLAGS_EXTRA += $(shell pkg-config --libs openssl)
	CFLAGS += $(shell pkg-config --cflags openssl) -DUSE_OPENSSL
endif

.PHONY: valgrind cachegrind callgrind clean subprotocols extensions autobahn autobahn_debug autobahn_call autobahn_cache count release debug profiling space test ${addprefix run_,${TEST_NAMES}}

#what we are trying to build
all: bin build docs log subprotocols extensions $(NAME)

build:
	if [[ ! -e $(BUILD_FOLDER) ]]; then mkdir -p $(BUILD_FOLDER); fi

bin:
	if [[ ! -e $(BIN_FOLDER) ]]; then mkdir -p $(BIN_FOLDER); fi

log:
	if [[ ! -e $(LOG_FOLDER) ]]; then mkdir -p $(LOG_FOLDER); fi

docs: $(SRC)
	doxygen $(CONF_FOLDER)/doxyfile.conf

release_mode:
	$(eval EXEC = $(RELEASE))
	$(eval MODE = "release")


debug_mode:
	$(eval EXEC = $(DEBUG))
	$(eval MODE = "debug")

profiling_mode:
	$(eval EXEC = $(PROFILE))
	$(eval MODE = "profiling")

space_mode:
	$(eval EXEC = $(SPACE))
	$(eval MODE = "space")

# Recompile when headers change
-include $(DEPS)

#linkage
$(NAME): $(SRC_OBJ)
	@echo 
	@echo ================ [Linking] ================ 
	@echo
	$(CC) $(CFLAGS) $(CVER) -o $(BIN_FOLDER)/$@ $(filter-out $(filter-out $(BUILD_FOLDER)/$@.o, $(addsuffix .o, $(addprefix $(BUILD_FOLDER)/, $(NAME)))), $^) $(FLAGS_EXTRA) $(INCLUDES)
	@echo
	@echo ================ [$(NAME) compiled succesfully] ================ 
	@echo

# compile every source file
$(BUILD_FOLDER)/%.o: $(SRC_FOLDER)/%.c
	@echo
	@echo ================ [Building Object] ================
	@echo
	$(CC) $(CFLAGS) $(CVER) $(INCLUDES) -c $< -o $@
	@echo
	@echo OK [$<] - [$@]
	@echo

# compile every test file
$(BUILD_FOLDER)/%.o: $(TEST_FOLDER)/%.c
	@echo
	@echo ================ [Building Object] ================
	@echo
	$(CC) --coverage $(CFLAGS) $(CVER) $(INCLUDES) -c $< -o $@
	@echo
	@echo OK [$<] - [$@]
	@echo

# Link test objects
${TEST_NAMES}: debug_mode bin build doc log ${SRC_OBJ} ${TEST_OBJ}
	@echo
	@echo ================ [Linking Tests] ================
	@echo
	$(CC) ${CFLAGS} ${CVER} -o ${BIN_FOLDER}/$@ ${BUILD_FOLDER}/$@.o\
		$(filter-out $(addsuffix .o, $(addprefix ${BUILD_FOLDER}/, main)), $(filter-out ${BUILD_FOLDER}/test_%.o, $(ALL_OBJ)))\
		${FLAGS_EXTRA} -lgcov ${FLAGS_CRITERION} $(INCLUDES)
	@echo
	@echo ================ [$@ compiled succesfully] ================

extensions:
	cd $(EXTENSIONS_FOLDER)/permessage-deflate/ && make $(MODE)

subprotocols:
	cd $(SUBPROTOCOLS_FOLDER)/echo/ && make $(MODE)
	cd $(SUBPROTOCOLS_FOLDER)/broadcast/ && make $(MODE)

#make valgrind
valgrind: clean debug_mode all
	@echo
	@echo ================ [Executing $(NAME) using Valgrind] ================
	@echo
	valgrind -v --leak-check=full --log-file="$(LOG_FOLDER)/valgrind.log" --track-origins=yes \
	--show-reachable=yes $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/wss.json

#make cachegrind
cachegrind: clean profiling_mode all
	@echo
	@echo ================ [Executing $(NAME) using Cachegrind] ================
	@echo
	valgrind --tool=cachegrind --trace-children=yes --cachegrind-out-file=$(LOG_FOLDER)/$(NAME).callgrind.log $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/wss.json

#make callgrind
callgrind: clean profiling_mode all
	@echo
	@echo ================ [Executing $(NAME) using Callgrind] ================
	@echo
	valgrind --tool=callgrind --simulate-cache=yes --branch-sim=yes --callgrind-out-file=$(LOG_FOLDER)/$(NAME).callgrind.log $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/wss.json

#make clean
clean:
	@echo
	@echo ================ [Cleaning $(NAME)] ================
	@echo
	rm -rf $(BIN_FOLDER)
	rm -rf $(BUILD_FOLDER)
	rm -rf $(LOG_FOLDER)

#make count
count:
	@echo
	@echo ================ [Counting lines in $(NAME)] ================
	@echo
	sloccount --wide --follow -- $(SRC_FOLDER) $(INCLUDE_FOLDER) $(TEST_FOLDER)

#make autobahn
autobahn: release
	if [[ ! -e $(REPORTS_FOLDER) ]]; then mkdir -p $(REPORTS_FOLDER); fi
	$(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/autobahn.json &
	sleep 1
	docker build -t wsserver/autobahn -f Dockerfile .
	docker run -it --rm \
	--network="host" \
    -v ${CONF_FOLDER}:/config \
    -v ${REPORTS_FOLDER}:/reports \
    -p 9001:9001 \
    --name fuzzingclient \
    wsserver/autobahn
	pkill $(NAME) || true

#make autobahn
autobahn_debug: debug
	if [[ ! -e $(REPORTS_FOLDER) ]]; then mkdir -p $(REPORTS_FOLDER); fi
	valgrind -v --leak-check=full --log-file="$(LOG_FOLDER)/valgrind.log" --track-origins=yes \
	--show-reachable=yes $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/autobahn.debug.json &
	sleep 3
	docker build -t wsserver/autobahn -f Dockerfile .
	docker run -it --rm \
	--network="host" \
    -v ${CONF_FOLDER}:/config \
    -v ${REPORTS_FOLDER}:/reports \
    -p 9001:9001 \
    --name fuzzingclient \
    wsserver/autobahn
	pkill -SIGINT memcheck

#make autobahn_call
autobahn_call: profiling
	if [[ ! -e $(REPORTS_FOLDER) ]]; then mkdir -p $(REPORTS_FOLDER); fi
	valgrind --tool=callgrind --simulate-cache=yes --branch-sim=yes --callgrind-out-file=$(LOG_FOLDER)/$(NAME).callgrind.log $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/autobahn.json &
	sleep 1
	docker build -t wsserver/autobahn -f Dockerfile .
	docker run -it --rm \
	--network="host" \
    -v ${CONF_FOLDER}:/config \
    -v ${REPORTS_FOLDER}:/reports \
    -p 9001:9001 \
    --name fuzzingclient \
    wsserver/autobahn
	pkill -SIGINT memcheck

#make autobahn_cache
autobahn_cache: profiling
	if [[ ! -e $(REPORTS_FOLDER) ]]; then mkdir -p $(REPORTS_FOLDER); fi
	valgrind --tool=cachegrind --trace-children=yes --cachegrind-out-file=$(LOG_FOLDER)/$(NAME).callgrind.log $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/autobahn.json &
	sleep 1
	docker build -t wsserver/autobahn -f Dockerfile .
	docker run -it --rm \
	--network="host" \
    -v ${CONF_FOLDER}:/config \
    -v ${REPORTS_FOLDER}:/reports \
    -p 9001:9001 \
    --name fuzzingclient \
    wsserver/autobahn
	pkill -SIGINT memcheck

#make test
test: $(TEST_NAMES) ${addprefix run_,${TEST_NAMES}}

#make run_test_* 
${addprefix run_,${TEST_NAMES}}: ${TEST_NAMES}
	@echo ================ [Running test ${patsubst run_%,%,$@}] ================
	@echo
	${BIN_FOLDER}/${patsubst run_%,%,$@} --verbose

#make release
release: clean release_mode all

#make debug
debug: clean debug_mode all

#make profiling
profiling: clean profiling_mode all

#make space
space: clean space_mode all
