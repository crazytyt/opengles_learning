
TARGET_NAME   = /root/tt/wayland_egl
CC = gcc

################################################################################
# Installation directory.

INSTALL_DIR = /root/tt
OBJ_DIR = /root/tt

################################################################################
# Add extra flags for object files.

CFLAGS += -fPIC -Werror

################################################################################
# Define flags for the linker.

LDFLAGS := -Wl,-z,defs

################################################################################
# Supply necessary libraries.

LDLIBS := -L/usr/local/lib \
          -l:gbm_viv.so -lwayland-client -lEGL -lGLESv2 -lm -lVDK

################################################################################
# Describe object files.

OBJECTS := $(OBJ_DIR)/wayland_egl.o

$(OBJ_DIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

################################################################################
# Define targets.

.PHONY: all clean install
all: $(TARGET_NAME)

clean:
	@rm -rf $(OBJ_DIR)/*.o

install: all
	@-cp $(TARGET_NAME) $(INSTALL_DIR)

$(TARGET_NAME): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(OBJ_DIR)
	$(CC) -c $(CFLAGS) -o $@ $<
