# Detect OS and shell environment
ifeq ($(OS),Windows_NT)
    ifeq ($(findstring sh,$(SHELL)),sh)
        # MSYS2, Cygwin, Git Bash
        EXEEXT = .exe
        RM = rm -f
        MKDIR = mkdir -p
    else
        # Native Windows (cmd)
        EXEEXT = .exe
        RM = del /Q
        MKDIR = if not exist "$(OUTDIR)" mkdir "$(OUTDIR)"
    endif
else
    # Linux, macOS, etc.
    EXEEXT =
    RM = rm -f
    MKDIR = mkdir -p
endif

# Compiler
CC = clang

# Output directory
OUTDIR = out

# Target executable
TARGET = $(OUTDIR)/casmpp$(EXEEXT)

# Source files
SRC = src/main.c

.PHONY: all clean

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(SRC)
	@echo Compiling...
	@$(MKDIR) $(OUTDIR)
	$(CC) -o $@ $<
	@echo Compiled!

# Clean target
clean:
	$(RM) $(TARGET)