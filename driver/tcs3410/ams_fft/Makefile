
CC      = arm-linux-gnueabihf-gcc
AR      = arm-linux-gnueabihf-ar
RANLIB  = arm-linux-gnueabihf-gcc-ranlib

INC_PATH    = h
# just another way of add the -I
#INC_DIR    = $(foreach d, $(INC_PATH), -I$d)
INC_DIR     = $(INC_PATH:%=-I%)
SRC_DIR     = src
OBJ_DIR     = obj


CPPFLAGS   += $(INC_DIR)
CFLAGS     += -W -Wall -Werror -Wextra
LFLAGS      = ruv

LIB_NAME    = libams_fft.a

SRC         = $(wildcard $(SRC_DIR)/*.c)

OBJ         = $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean

all: $(LIB_NAME)

# Debug - print the variable and its value
print-%  : ; @echo $* = $($*)

$(LIB_NAME): $(OBJ)
	$(AR) $(LFLAGS) $@ $^
	$(RANLIB) $@

# Build the algorithm object code
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | obj
#	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

obj:
	mkdir -p $@

clean:
	$(RM) $(OBJ) $(LIB_NAME)

