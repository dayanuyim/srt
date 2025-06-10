OBJS = srt.o main.o
TARGET = srt

CXXFLAGS = -std=c++23 -g3 -Wall -Wextra -pthread #-static-libgcc

#UnitTestHome=/usr/unittest-cpp/UnitTest++
PROJECT_HOME := ..
BOOST_ROOT := /opt/homebrew/opt/boost

INCS += -I$(BOOST_ROOT)/include
INCS += -I$(PROJECT_HOME)/toolbox/CppToolbox
#INCS += -I$(UnitTestHome)/src

#LDLIBS += -L$(UnitTestHome) -lUnitTest++
#LDLIBS += /usr/lib/gcc/x86_64-linux-gnu/4.8/libstdc++.a
LDLIBS += -L$(BOOST_ROOT)/lib
LDLIBS += -lboost_system
#LDLIBS += -lboost_thread
LDLIBS += -lboost_date_time
LDLIBS += -lboost_regex
#LDLIBS += -lboost_unit_test_framework

CHECKER = cppcheck --quiet --enable=all --error-exitcode=255 \
		  --template='[{file}:{line}]: {severity} ({id}) {message}' \
		  --inline-suppr \
	      --exitcode-suppressions=./cppcheck_exit_suppr.txt
#CHCKER_EX_DEFS += -U UNITTESTCPP_H

#######################################################
all: $(TARGET)

$(TARGET): $(OBJS)
#	$(CHECKER) $(CHCKER_EX_DEFS) $(INCS) $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDLIBS)

main.o: main.cpp srt.h
	$(CXX) $(CXXFLAGS) -c $< $(INCS)


srt.o: srt.cpp srt.h
	$(CXX) $(CXXFLAGS) -c $< $(INCS)

.PHONY:
clean:
	$(RM) $(TARGET) *.o
