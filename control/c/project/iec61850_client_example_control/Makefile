LIBIEC_HOME= /home/ubuntu/2025proj/GIPAT_SCADA/IEC61850/lib/libiec61850
MAKE_MK_PATH = /home/ubuntu/2025proj/GIPAT_SCADA/IEC61850/control/c
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
