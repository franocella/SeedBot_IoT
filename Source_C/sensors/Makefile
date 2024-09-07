CONTIKI_PROJECT = sensors

all: $(CONTIKI_PROJECT)

PLATFORMS_EXCLUDE = sky z1

CONTIKI = ../../..


# Add the utils directory
MODULES_REL += ../utils


# Include the CoAP implementation
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

LDLIBS += -lm


include $(CONTIKI)/Makefile.include
