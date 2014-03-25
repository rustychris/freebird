
BASE=freebird_pcb01

all: $(BASE).zip

$(BASE).txt: $(BASE).drl
	cp $< $@

$(BASE)-PCB_Edges.gml: $(BASE)-PCB_Edges.gbr
	cp $< $@


SRCS=$(BASE)-PCB_Edges.gml $(BASE)-Front.gtl $(BASE)-Back.gbl $(BASE)-Mask_Front.gts $(BASE)-Mask_Back.gbs \
 $(BASE)-SilkS_Front.gto $(BASE)-SilkS_Back.gbo $(BASE).txt


$(BASE).zip: $(SRCS)
	zip ${BASE}.zip $(SRCS)

.PHONY: gerbv

gerbv:
	gerbv $(SRCS)
