# Makefile for SeqPair project.
# Due to portability problems, if you can run the attached programs in bin folder,
# just use them directly. 
# Otherwise, please modify the boost directory below (no guarantee of success of compilation)

CC = g++
CPPFLAGS = -DNDEBUG
# Please modify your boost directory below
BOOSTDIR = "D:\rpgma\Documents\Visual Studio 2017\Projects\Repos\vcpkg\installed\x64-windows\include"
CXXFLAGS = -std=c++14 -O2 -I../include/Aureliano -I$(BOOSTDIR) -fpermissive
COMMON_OBJS = bin/rect.o
RUN_PACKER = bin/run_packer.exe
BOOST_TEST = bin/boost_test.exe
GENERATE_TESTCASE = bin/generate_testcase.exe
AUTORUN = bin/autorun.exe
MINGW32_AUTORUN = bin/mingw32_autorun.exe

.PHONY: all
all: $(RUN_PACKER) $(BOOST_TEST) $(GENERATE_TESTCASE) $(AUTORUN)

$(RUN_PACKER): $(COMMON_OBJS) bin/run_packer.o $(HEADERS)
	$(CC) $(CPPFLAGS) $(CXXFLAGS) $(COMMON_OBJS) bin/run_packer.o -o $@
$(BOOST_TEST): $(COMMON_OBJS) bin/boost_test.o $(HEADERS)
	$(CC) $(CPPFLAGS) $(CXXFLAGS) $(COMMON_OBJS) bin/boost_test.o -o $@
$(GENERATE_TESTCASE): $(COMMON_OBJS) bin/generate_testcase.o $(HEADERS)
	$(CC) $(CPPFLAGS) $(CXXFLAGS) $(COMMON_OBJS) bin/generate_testcase.o -o $@
$(AUTORUN): src/autorun.cpp
	$(CC) $(CPPFLAGS) $(CXXFLAGS) $< -o $@
$(MINGW32_AUTORUN): src/autorun.cpp
	$(CC) $(CPPFLAGS) -DMINGW32_MAKE $(CXXFLAGS) $< -o $@
bin/%.o: src/%.cpp $(HEADERS)
	$(CC) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm ./bin/*

.PHONY: make_testcases
make_testcases:
	# Sadly there doensn't seem to be a portable loop method in makefile...
	$(GENERATE_TESTCASE) 32 16 1 16 testcase/rect32.txt testcase/net32.txt
	$(GENERATE_TESTCASE) 64 32 1 16 testcase/rect64.txt testcase/net64.txt
	$(GENERATE_TESTCASE) 128 64 1 16 testcase/rect128.txt testcase/net128.txt
	$(GENERATE_TESTCASE) 256 128 1 16 testcase/rect256.txt testcase/net256.txt
	$(GENERATE_TESTCASE) 500 250 1 16 testcase/rect500.txt testcase/net500.txt

.PHONY: make_testcase
make_testcase:
	$(GENERATE_TESTCASE) $(n) ($(n)/2) 1 16 testcase/rect$(n).txt testcase/net$(n).txt

.PHONY: rm_testcases
rm_testcases:
	rm ./testcase/*

.PHONY: rm_testcase
rm_testcase:
	rm ./testcase/rect$(n).txt 
	rm ./testcas/net$(n).txt

.PHONY: boost_test
boost_test: 
	$(BOOST_TEST)

.PHONY: run_packer
run_packer:
	$(RUN_PACKER) testcase/rect$(n).txt testcase/net$(n).txt $(alpha) $(method) testcase/layout$(n)-$(alpha)-$(method)-$(thrds).txt $(thrds) 

.PHONY: autorun
autorun:
	$(AUTORUN) $(alpha) $(method) $(thrds)

.PHONY: mingw32_autorun
mingw32_autorun:
	$(MINGW32_AUTORUN) $(alpha) $(method) $(thrds)

.PHONY: help
help:
	@echo "Due to portability constraints, if you can run the attached programs in bin folder,
	@echo "just use them directly." 
	@echo "Otherwise, please modify the BOOSTDIR variable in makefile (no guarantee of 
	@echo "success of compilation)"
	@echo ""
	@echo "all: generates packer program, boost test program, and testcase generator"
	@echo "clean: removes all executables"
	@echo "run_packer: e.g. run_packer n=64 alpha=1.0 method=lcs thrds=1"
	@echo "autorun: runs all default-sized testcases, e.g. autorun alpha=1.0 method=lcs thrds=1"
	@echo "mingw32_autorun: same as autorun, if you are using mingw32-make"
	@echo "make_testcase: generates custom-sized testcase, e.g. generate_testcase n=100"
	@echo "make_testcases: generates default-sized testcases"
	@echo "rm_testcase: removes specified testcase, e.g. rm_testcase n=100"
	@echo "rm_testcases: removes all testcases"
	@echo "boost_test: runs boost test program"