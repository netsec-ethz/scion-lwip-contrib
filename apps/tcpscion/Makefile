all:
	# gcc client.c  -I../../../scion-lwip/src/include/ -I. -I../../ports/unix/include/ -I../../../scion-lwip/src/include/ipv4 -I../../ports/unix/proj/scion/  ../../ports/unix/proj/scion/liblwip.so -lpthread -o client
	# gcc server.c  -I../../../scion-lwip/src/include/ -I. -I../../ports/unix/include/ -I../../../scion-lwip/src/include/ipv4 -I../../ports/unix/proj/scion/  ../../ports/unix/proj/scion/liblwip.so -lpthread -o server
	gcc  srvcli.c client.c server.c  -I../../../scion-lwip/src/include/ -I. -I../../ports/unix/include/ -I../../../scion-lwip/src/include/ipv4 -I../../ports/unix/proj/scion/ -I../../../scion/lib/ ../../ports/unix/proj/scion/liblwip.so -lpthread -o srvcli

clean:
	rm -f server client srvcli

