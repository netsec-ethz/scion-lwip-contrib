all:
	$(MAKE) -C ./ports/unix/proj/scion
clean:
	$(MAKE) -C ./ports/unix/proj/scion clean
install:
	$(MAKE) -C ./ports/unix/proj/scion install
uninstall:
	$(MAKE) -C ./ports/unix/proj/scion uninstall
