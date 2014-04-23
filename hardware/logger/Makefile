
BASE=freebird_pcb01

all: $(BASE).zip

$(BASE).txt: $(BASE).drl
	cp $< $@

$(BASE).gml: $(BASE)-PCB_Edges.gbr
	cp $< $@


$(BASE).gtl: $(BASE)-Front.gtl
	cp $< $@

$(BASE).gbl: $(BASE)-Back.gbl
	cp $< $@

$(BASE).gts: $(BASE)-Mask_Front.gts
	cp $< $@

$(BASE).gbs: $(BASE)-Mask_Back.gbs
	cp $< $@

$(BASE).gto: $(BASE)-SilkS_Front.gto 
	cp $< $@

$(BASE).gbo: $(BASE)-SilkS_Back.gbo
	cp $< $@


SRCS=$(BASE).gml $(BASE).gtl $(BASE).gbl $(BASE).gts $(BASE).gbs \
 $(BASE).gto $(BASE).gbo $(BASE).txt


$(BASE).zip: $(SRCS)
	-rm $(BASE).zip
	zip $(BASE).zip $(SRCS)

.PHONY: gerbv

gerbv: $(SRCS)
	gerbv $(SRCS)
