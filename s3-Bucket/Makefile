# Makefile for LocalHive S3 Bucket
# Usage: make [target]

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -Iinclude
LDFLAGS = -lboost_system -lsqlite3 -lpthread

# Directories
SRCDIR = lib
SERVERDIR = server
TESTDIR = tests
BUILDDIR = build
OBJDIR = $(BUILDDIR)/obj

# Source files
LIB_SOURCES = $(wildcard $(SRCDIR)/*.cpp)
SERVER_SOURCES = $(wildcard $(SERVERDIR)/*.cpp) $(wildcard $(SERVERDIR)/routes/*.cpp)
TEST_SOURCES = $(wildcard $(TESTDIR)/*.cpp)

# Object files
LIB_OBJECTS = $(LIB_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
SERVER_OBJECTS = $(SERVER_SOURCES:$(SERVERDIR)/%.cpp=$(OBJDIR)/server_%.o)
TEST_OBJECTS = $(TEST_SOURCES:$(TESTDIR)/%.cpp=$(OBJDIR)/test_%.o)

# Targets
SERVER_TARGET = $(BUILDDIR)/s3_bucket
TEST_TARGET = $(BUILDDIR)/s3_bucket_tests

.PHONY: all clean test run install

all: $(SERVER_TARGET) $(TEST_TARGET)

# Create directories
$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# Build server
$(SERVER_TARGET): $(LIB_OBJECTS) $(SERVER_OBJECTS) | $(BUILDDIR)
	$(CXX) $(LIB_OBJECTS) $(SERVER_OBJECTS) -o $@ $(LDFLAGS)
	@echo "Server built successfully: $@"

# Build tests
$(TEST_TARGET): $(LIB_OBJECTS) $(TEST_OBJECTS) | $(BUILDDIR)
	$(CXX) $(LIB_OBJECTS) $(TEST_OBJECTS) -o $@ $(LDFLAGS)
	@echo "Tests built successfully: $@"

# Compile library sources
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile server sources
$(OBJDIR)/server_%.o: $(SERVERDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/server_routes_%.o: $(SERVERDIR)/routes/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile test sources
$(OBJDIR)/test_%.o: $(TESTDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run tests
test: $(TEST_TARGET)
	@echo "Running tests..."
	./$(TEST_TARGET)

# Run server
run: $(SERVER_TARGET)
	@echo "Starting LocalHive S3 Server..."
	./$(SERVER_TARGET)

# Clean build files
clean:
	rm -rf $(BUILDDIR)
	@echo "Build files cleaned"

# Install dependencies (Ubuntu/Debian)
install-deps:
	@echo "Installing dependencies..."
	sudo apt-get update
	sudo apt-get install -y build-essential cmake libboost-all-dev libsqlite3-dev

# Help
help:
	@echo "LocalHive S3 Bucket Makefile"
	@echo "Available targets:"
	@echo "  all          - Build server and tests"
	@echo "  test         - Build and run tests"
	@echo "  run          - Build and run server"
	@echo "  clean        - Clean build files"
	@echo "  install-deps - Install dependencies (Ubuntu/Debian)"
	@echo "  help         - Show this help"