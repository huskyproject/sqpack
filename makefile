COPT = -g -O2 -DUNIX -I../smapi -I../fidoconfig

sqpack: sqpack.c
	gcc $(COPT) sqpack.c -lfidoconfig -lsmapilnx -o sqpack