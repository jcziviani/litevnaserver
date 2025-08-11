PROJECT_EXE := litevnaserver
PROJECT_SOURCE := src/main.cpp
PROJECT_INCLUDE := -Isrc

# ---------------------------------------------------------------------------------------------
.PHONY : showOptions mkdirs ${PROJECT_EXE}

ARCH := $(shell uname -m)
BUILD := release
COMPILER := g++
BUILD_DIR := ${CURDIR}/bin/linux/${ARCH}/${BUILD}
LDLIBS := -lpthread -ldl -lrt
PROJECT_OUTPUT_EXE := ${BUILD_DIR}/${PROJECT_EXE}

$(if $(filter $(BUILD),debug release),,$(error Invalid BUILD option: $(BUILD). Available options are: debug or release))
$(if $(filter $(COMPILER),g++ clang++),,$(error Invalid COMPILER option: $(COMPILER). Available options are: g++ or clang++))

CXX.g++.FLAGS := -std=c++14 -Wall -pthread
CXX.g++.FLAGS.debug := -g -fstack-protector-all -fsanitize=address
CXX.g++.FLAGS.release :=  -DNDEBUG -O3 -static-libstdc++ -fstack-protector

CXX.clang++.FLAGS := -std=c++14 -Wall -pthread
CXX.clang++.FLAGS.debug := -O0 -g -fstack-protector-all -fsanitize=address
CXX.clang++.FLAGS.release :=  -DNDEBUG -O2 -static-libstdc++ -fstack-protector 

${PROJECT_EXE}: TARGET := ${PROJECT_EXE}
${PROJECT_EXE}: showOptions mkdirs
	${COMPILER} ${CXX.${COMPILER}.FLAGS} ${CXX.${COMPILER}.FLAGS.${BUILD}} -o ${PROJECT_OUTPUT_EXE} ${PROJECT_INCLUDE} ${PROJECT_SOURCE} ${PROJECT_LDFLAGS} ${LDLIBS} ${PROJECT_LDLIBS.${BUILD}}

mkdirs:
	mkdir -p ${BUILD_DIR}

showOptions:
	@echo "-----------------------------------------------------------------------------";
	@echo "";
	@echo "Current options:";
	@echo "  make BUILD=${BUILD} COMPILER=${COMPILER}";
	@echo "";
	@echo "Available options:";
	@echo "  BUILD=debug      Build version for debugging.";
	@echo "  BUILD=release    Build optimized version (Default).";
	@echo "";
	@echo "  COMPILER=g++     Compile using g++ (Default).";
	@echo "  COMPILER=clang++ Compile using clang++.";
	@echo "";
	@echo "-----------------------------------------------------------------------------";
	@echo "";
	@echo "Building ${PROJECT_EXE} executable ${PROJECT_OUTPUT_EXE}";
	@echo "";
