all: flrn.1

%.1: %.1.iso zapaccents
	LC_ALL=C sed -f zapaccents $< > $@
