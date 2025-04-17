TARGET   := mem_crash_tests
SRCS     := main.cpp heap_overflow.cpp kernel_access.cpp
OBJS     := $(SRCS:.cpp=.o)
CXXFLAGS := -std=c++17 -O0 -g -I$(KAIZEN_INC) -Wall -Wextra -pedantic

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	./$(TARGET) --test both --trials 3

plot: all
	python3 plot_results.py mem_crash_results.csv

clean:
	rm -f $(TARGET) $(OBJS) mem_crash_results.csv mem_crash_plot.png

.PHONY: all run plot clean
