CONTIKI_PROJECT = actuators
all: $(CONTIKI_PROJECT)
CONTIKI = ../../..

# Includi i file sorgenti di utils e utils/emlearn

TARGET_SOURCEFILES += $(wildcard $(CONTIKI)/emlearn/*.c)

# Aggiungi i percorsi di include per utils e utils/emlearn/emlearn
CFLAGS += -I$(CONTIKI)/emlearn/emlearn



PLATFORMS_EXCLUDE = sky z1


# Add the utils directory
MODULES_REL += ../utils


# Include the CoAP implementation
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

LDLIBS += -lm

include $(CONTIKI)/Makefile.include
