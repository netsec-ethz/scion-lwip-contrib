all:
	make -C ./ports/unix/proj/scion
	make -C ./apps/tcpscion
clean:
	make -C ./ports/unix/proj/scion clean
	make -C ./apps/tcpscion clean

