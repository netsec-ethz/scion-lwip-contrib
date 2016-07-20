all:
	make -C ./ports/unix/proj/scion
clean:
	make -C ./ports/unix/proj/scion clean
install:
	make -C ./ports/unix/proj/scion install
uninstall:
	make -C ./ports/unix/proj/scion uninstall
