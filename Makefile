#Shell
SHELL = /bin/bash 

#Executeable name
NAME = WSServer

#Compiler
CC = gcc

#Version
ifeq ($(VER),)
	VER = $(shell git describe --abbrev=0 --tags)
endif

# Folders
ROOT = $(shell pwd)
LOG_FOLDER = $(ROOT)/log
BUILD_FOLDER = $(ROOT)/build
BIN_FOLDER = $(ROOT)/bin
SRC_FOLDER = $(ROOT)/src
INCLUDE_FOLDER = $(ROOT)/include
TEST_FOLDER = $(ROOT)/test
CONF_FOLDER = $(ROOT)/conf
GEN_FOLDER = $(ROOT)/generated
RESOURCES_FOLDER = $(ROOT)/resources
EXTENSIONS_FOLDER = $(ROOT)/extensions
SUBPROTOCOLS_FOLDER = $(ROOT)/subprotocols
SCRIPTS_FOLDER = $(ROOT)/scripts

#Debug or Release
PROFILING = -Og -g -DNDEBUG
DEBUG = -Og -g --param max-inline-insns-single=1000
RELEASE = -O3 -funroll-loops -DNDEBUG
SPACE = -Os -DNDEBUG
MODE = "release"
PREV_MODE = "$(shell cat $(BUILD_FOLDER)/.mode 2> /dev/null)"
PREV_SSL = $(shell cat $(BUILD_FOLDER)/.ssl 2> /dev/null)
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
		 -D_DEFAULT_SOURCE \
		 -DWSS_SERVER_VERSION=\"$(VER)\" \
		 -DUSE_RPMALLOC #\
		 -DUSE_POLL\

# C version
CVER = -std=c11

# Flags
FLAGS_EXTRA = -pthread -lm -ldl
FLAGS_TEST = -lgcov
FLAGS_COVERAGE =
FLAGS_INCLUDES = -I$(INCLUDE_FOLDER) -I$(SRC_FOLDER) -I$(EXTENSIONS_FOLDER) -I$(SUBPROTOCOLS_FOLDER)

# Files
SRC = $(shell find $(SRC_FOLDER) -iname '*.c' -type f;)
TESTS = $(shell find $(TEST_FOLDER) -iname 'test_*.c' -type f;)
SRC_OBJ  = $(subst $(SRC_FOLDER), $(BUILD_FOLDER), $(patsubst %.c, %.o, $(SRC)))
TEST_OBJ = ${subst ${TEST_FOLDER}, ${BUILD_FOLDER}, ${patsubst %.c, %.o, $(TESTS)}}
ALL_OBJ  = ${SRC_OBJ} ${TEST_OBJ}
TEST_NAMES = ${patsubst ${TEST_FOLDER}/%.c, %, ${TESTS}}
DEPS = $(ALL_OBJ:%.o=%.d)

ifeq ($(BUMP),)
	BUMP = default
endif

ifneq ($(SSL_LIBRARY_PATH),)
	SSL_MODE=$(SSL_LIBRARY_PATH)
	FLAGS_INCLUDES += -I$(SSL_LIBRARY_PATH)/include -L$(SSL_LIBRARY_PATH)/lib
	export LD_LIBRARY_PATH=$(SSL_LIBRARY_PATH)/lib
endif

ifndef TRAVIS
# Which SSL implementation to use
ifeq ($(SSL),)
	SSL = OPENSSL
	SSL_MODE = OPENSSL
endif

$(shell pkg-config --exists libssl libcrypto)
ifeq ($(.SHELLSTATUS),0)
ifeq ("$(SSL)","OPENSSL")
	SSL_MODE = OPENSSL
	FLAGS_EXTRA += $(shell pkg-config --libs libssl libcrypto)
	CFLAGS += $(shell pkg-config --cflags libssl libcrypto) -DUSE_OPENSSL
endif
endif

$(shell pkg-config --exists wolfssl)
ifeq ($(.SHELLSTATUS),0)
ifeq ("$(SSL)","WOLFSSL")
	SSL_MODE = WOLFSSL
	FLAGS_EXTRA += $(shell pkg-config --libs wolfssl)
	CFLAGS += $(shell pkg-config --cflags wolfssl) -DUSE_WOLFSSL
endif
endif

$(shell pkg-config --exists criterion)
ifeq ($(.SHELLSTATUS),0)
	FLAGS_TEST += $(shell pkg-config --libs criterion)
endif
else
	SSL_MODE = OPENSSL
	FLAGS_EXTRA += -lssl -lcrypto
	CFLAGS += -DUSE_OPENSSL
	FLAGS_TEST += -lcriterion
endif

.PHONY: valgrind version bump cachegrind callgrind clean subprotocols extensions autobahn massconnect massconnect_debug autobahn_debug autobahn_call autobahn_cache analysis count release debug profiling space test ${addprefix run_,${TEST_NAMES}}

#what we are trying to build
all: internal_clean version bin build ssl log subprotocols extensions $(NAME)

build:
	if [[ ! -e $(BUILD_FOLDER) ]]; then mkdir -p $(BUILD_FOLDER); fi
	echo "$(MODE)" >> $(BUILD_FOLDER)/.mode

bin:
	if [[ ! -e $(BIN_FOLDER) ]]; then mkdir -p $(BIN_FOLDER); fi

log:
	if [[ ! -e $(LOG_FOLDER) ]]; then mkdir -p $(LOG_FOLDER); fi

docs: $(SRC)
	rm -rf $(GEN_FOLDER)/documentation
	mkdir -p $(GEN_FOLDER)/documentation
	doxygen $(CONF_FOLDER)/doxyfile.conf

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
	@echo ================ [Linking] ================ 
	@echo
	$(CC) $(CFLAGS) ${FLAGS_COVERAGE} $(CVER) $(FLAGS_INCLUDES) -o $(BIN_FOLDER)/$@ $(filter-out $(filter-out $(BUILD_FOLDER)/$@.o, $(addsuffix .o, $(addprefix $(BUILD_FOLDER)/, $(NAME)))), $^) $(FLAGS_EXTRA)
	@echo
	@echo ================ [$(NAME) compiled succesfully] ================ 
	@echo

# compile every source file
$(BUILD_FOLDER)/%.o: $(SRC_FOLDER)/%.c
	@echo
	@echo ================ [Building Object] ================
	@echo
	$(CC) $(CFLAGS) ${FLAGS_COVERAGE} $(CVER) $(FLAGS_INCLUDES) -c $< -o $@
	@echo
	@echo OK [$<] - [$@]
	@echo

# compile every test file
$(BUILD_FOLDER)/%.o: $(TEST_FOLDER)/%.c
	@echo
	@echo ================ [Building Object] ================
	@echo
	$(CC) --coverage $(CFLAGS) ${FLAGS_COVERAGE} $(CVER) $(FLAGS_INCLUDES) -c $< -o $@
	@echo
	@echo OK [$<] - [$@]
	@echo

# Link test objects
${TEST_NAMES}: internal_clean debug_mode bin build log ${SRC_OBJ} ${TEST_OBJ}
	@echo
	@echo ================ [Linking Tests] ================
	@echo
	$(CC) ${CFLAGS} ${FLAGS_COVERAGE} ${CVER} $(FLAGS_INCLUDES) -o ${BIN_FOLDER}/$@ ${BUILD_FOLDER}/$@.o\
		$(filter-out $(addsuffix .o, $(addprefix ${BUILD_FOLDER}/, main)), $(filter-out ${BUILD_FOLDER}/test_%.o, $(ALL_OBJ)))\
		${FLAGS_EXTRA} ${FLAGS_TEST}
	@echo
	@echo ================ [$@ compiled succesfully] ================

extensions:
	cd $(EXTENSIONS_FOLDER)/permessage-deflate/ && make $(MODE)

subprotocols:
	cd $(SUBPROTOCOLS_FOLDER)/echo/ && make $(MODE)
	cd $(SUBPROTOCOLS_FOLDER)/broadcast/ && make $(MODE)

#make valgrind
valgrind: debug_mode all
	@echo
	@echo ================ [Executing $(NAME) using Valgrind] ================
	@echo
	valgrind -v --leak-check=full --log-file="$(LOG_FOLDER)/valgrind.log" --track-origins=yes \
	--show-reachable=yes $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/wss.json

#make cachegrind
cachegrind: profiling_mode all
	@echo
	@echo ================ [Executing $(NAME) using Cachegrind] ================
	@echo
	valgrind --tool=cachegrind --trace-children=yes --cachegrind-out-file=$(LOG_FOLDER)/$(NAME).callgrind.log $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/wss.json

#make callgrind
callgrind: profiling_mode all
	@echo
	@echo ================ [Executing $(NAME) using Callgrind] ================
	@echo
	valgrind --tool=callgrind --simulate-cache=yes --branch-sim=yes --callgrind-out-file=$(LOG_FOLDER)/$(NAME).callgrind.log $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/wss.json

internal_clean:
	@echo
	@echo ================ [Cleaning $(NAME)] ================
	@echo
	if [ $(PREV_MODE) != $(MODE) ]; then rm -rf $(BUILD_FOLDER); fi
	if [ "$(PREV_SSL)" != "$(SSL)" ]; then rm -rf $(BUILD_FOLDER); fi
	rm -rf $(BIN_FOLDER)
	rm -rf $(LOG_FOLDER)

#make clean
clean: internal_clean
	rm -rf $(BUILD_FOLDER);

#make count
count:
	@echo
	@echo ================ [Counting lines in $(NAME)] ================
	@echo
	sloccount --wide --follow -- $(SRC_FOLDER) $(INCLUDE_FOLDER) $(TEST_FOLDER)

#make autobahn
autobahn: release
	rm -rf $(GEN_FOLDER)/autobahn
	if [[ ! -e $(GEN_FOLDER) ]]; then mkdir -p $(GEN_FOLDER); fi
	$(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/autobahn.json &
	sleep 5
	docker build -t wsserver/autobahn -f Dockerfile.test .
	docker run -it --rm \
	--network="host" \
    -v ${CONF_FOLDER}:/config \
    -v ${GEN_FOLDER}:/generated \
    -p 9001:9001 \
    --name fuzzingclient \
    wsserver/autobahn
	pkill $(NAME) || true

#make autobahn_debug
autobahn_debug: debug
	rm -rf $(GEN_FOLDER)/autobahn
	if [[ ! -e $(GEN_FOLDER) ]]; then mkdir -p $(GEN_FOLDER); fi
	valgrind -v --leak-check=full --log-file="$(LOG_FOLDER)/valgrind.log" --track-origins=yes \
	--show-reachable=yes $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/autobahn.debug.json &
	sleep 10
	docker build -t wsserver/autobahn -f Dockerfile.test .
	docker run -it --rm \
	--network="host" \
    -v ${CONF_FOLDER}:/config \
    -v ${GEN_FOLDER}:/generated \
    -p 9001:9001 \
    --name fuzzingclient \
    wsserver/autobahn
	pkill -SIGINT memcheck

#make autobahn_call
autobahn_call: profiling
	rm -rf $(GEN_FOLDER)/autobahn
	if [[ ! -e $(GEN_FOLDER) ]]; then mkdir -p $(GEN_FOLDER); fi
	valgrind --tool=callgrind --simulate-cache=yes --branch-sim=yes --callgrind-out-file=$(LOG_FOLDER)/$(NAME).callgrind.log $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/autobahn.debug.json &
	sleep 3
	docker build -t wsserver/autobahn -f Dockerfile.test .
	docker run -it --rm \
	--network="host" \
    -v ${CONF_FOLDER}:/config \
    -v ${GEN_FOLDER}:/generated \
    -p 9001:9001 \
    --name fuzzingclient \
    wsserver/autobahn
	pkill -SIGINT memcheck

#make autobahn_cache
autobahn_cache: profiling
	rm -rf $(GEN_FOLDER)/autobahn
	if [[ ! -e $(GEN_FOLDER) ]]; then mkdir -p $(GEN_FOLDER); fi
	valgrind --tool=cachegrind --trace-children=yes --cachegrind-out-file=$(LOG_FOLDER)/$(NAME).callgrind.log $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/autobahn.debug.json &
	sleep 3
	docker build -t wsserver/autobahn -f Dockerfile.test .
	docker run -it --rm \
	--network="host" \
    -v ${CONF_FOLDER}:/config \
    -v ${GEN_FOLDER}:/generated \
    -p 9001:9001 \
    --name fuzzingclient \
    wsserver/autobahn
	pkill -SIGINT memcheck

#make massconnect
massconnect: release
	if [[ ! -e $(GEN_FOLDER) ]]; then mkdir -p $(GEN_FOLDER); fi
	$(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/autobahn-massconnect.json &
	sleep 1
	docker build -t wsserver/autobahn -f Dockerfile.massconnect .
	docker run -it --rm \
	--network="host" \
    -v ${CONF_FOLDER}:/config \
    -v ${GEN_FOLDER}:/generated \
    -p 9001:9001 \
    --name massconnect \
    wsserver/autobahn
	pkill $(NAME) || true

#make massconnect_debug
massconnect_debug: debug
	if [[ ! -e $(GEN_FOLDER) ]]; then mkdir -p $(GEN_FOLDER); fi
	$(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/autobahn-massconnect.json &
	sleep 3
	docker build -t wsserver/autobahn -f Dockerfile.massconnect .
	docker run -it --rm \
	--network="host" \
    -v ${CONF_FOLDER}:/config \
    -v ${GEN_FOLDER}:/generated \
    -p 9001:9001 \
    --name massconnect \
    wsserver/autobahn
	pkill $(NAME) || true

#make autobahn_cache
massconnect_cache: profiling
	if [[ ! -e $(GEN_FOLDER) ]]; then mkdir -p $(GEN_FOLDER); fi
	ulimit -n 100000 && valgrind --tool=cachegrind --trace-children=yes --cachegrind-out-file=$(LOG_FOLDER)/$(NAME).callgrind.log $(BIN_FOLDER)/$(NAME) -c $(CONF_FOLDER)/autobahn-massconnect.json &
	sleep 3
	docker build -t wsserver/autobahn -f Dockerfile.massconnect .
	docker run -it --rm \
	--network="host" \
    -v ${CONF_FOLDER}:/config \
    -v ${GEN_FOLDER}:/generated \
    -p 9001:9001 \
    --name massconnect \
    wsserver/autobahn
	pkill -SIGINT memcheck

criterion:
	$(eval FLAGS_COVERAGE = -fprofile-arcs -ftest-coverage)

#make test
test: criterion subprotocols extensions $(TEST_NAMES) ${addprefix run_,${TEST_NAMES}} 
	rm -rf $(GEN_FOLDER)/gcov
	mkdir -p $(GEN_FOLDER)/gcov
	gcovr --object-directory $(BUILD_FOLDER) -r . --html --html-details --html-title $(NAME) -o $(GEN_FOLDER)/gcov/index.html

#make run_test_* 
${addprefix run_,${TEST_NAMES}}: ${TEST_NAMES}
	@echo ================ [Running test ${patsubst run_%,%,$@}] ================
	@echo
	${BIN_FOLDER}/${patsubst run_%,%,$@} --verbose

analysis:
	cppcheck --language=c -f -q --enable=warning,performance,portability -$(CVER) --error-exitcode=1 -i$(TEST_FOLDER) $(FLAGS_INCLUDES) .
 
#make version
version:
	@echo
	@echo ================ [$(NAME) - version $(VER)] ================
	@echo

#make release
release: release_mode all

#make debug
debug: debug_mode all

#make profiling
profiling: profiling_mode all

#make space
space: space_mode all

ssl:
	echo $(SSL_MODE) >> $(BUILD_FOLDER)/.ssl

#make bump
bump:
	$(eval VER = $(shell $(SCRIPTS_FOLDER)/bump.sh --$(BUMP)))
	make release VER=$(VER)
