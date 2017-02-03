COMPILER = g++
#I used something from c++11 i forgot what though whoops
COMPILERFLAGS = -c -std=c++11 -Wall
SOURCE = phunt.cpp
OBJECT = $(SOURCE:.cpp=.o)
HEADER = 

#output to phunt
EXECUTABLE = phunt

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECT)
	$(COMPILER) $(OBJECT) -o $@

%.o: %.cpp $(HEADER)
	$(COMPILER) $(COMPILERFLAGS) $<

#Delete everything if you "make clean"
clean:
	rm -rf *.o $(EXECUTABLE)
