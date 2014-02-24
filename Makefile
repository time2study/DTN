CONTIKI = ../..

CONTIKI_PROJECT = test-dtn

all: $(CONTIKI_PROJECT)

CONTIKI_SOURCEFILES += dtn.c


include $(CONTIKI)/Makefile.include
