LIBIEC_HOME= /home/pi/c_dms/lib/libiec61850/
MAKE_MK_PATH = /home/pi/IEC61850_C
PROJECT_BINARY_NAME = client_example_control
PROJECT_SOURCES = client_example_control.c
CJSON = cJSON.c
LOG = log.c

include $(MAKE_MK_PATH)/make/target_system.mk
include $(MAKE_MK_PATH)/make/stack_includes.mk
include $(MAKE_MK_PATH)/make/other_lib.mk

all:	$(PROJECT_BINARY_NAME)

include $(MAKE_MK_PATH)/make/common_targets.mk

$(PROJECT_BINARY_NAME):	$(PROJECT_SOURCES) $(LIB_NAME)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(PROJECT_BINARY_NAME) $(PROJECT_SOURCES) $(CJSON) $(LOG) $(INCLUDES) $(LIB_NAME) $(LDLIBS) $(OTHER_LIBS)

clean:
	rm -f $(PROJECT_BINARY_NAME)
