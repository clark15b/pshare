all:
	gcc -fno-exceptions -fno-rtti -O2 -DWITH_LIBUUID -o pshare main.cpp upnp.cpp soap.cpp tmpl.cpp mem.cpp proxy.cpp common.cpp -luuid
	strip pshare

mipsel:
	$(MAKE) embedded SDK=/usr/local/toolchain-mipsel_gcc3.4.6/mipsel-linux/bin
	./mkipkg.sh $@

ar71xx:
	$(MAKE) embedded SDK=/usr/local/toolchain-mips_gcc4.1.2/mips-linux/bin
	./mkipkg.sh $@

embedded:
	$(SDK)/gcc -fno-exceptions -fno-rtti -O2 -DWITH_URANDOM -o pshare main.cpp upnp.cpp soap.cpp tmpl.cpp mem.cpp proxy.cpp common.cpp
	$(SDK)/strip pshare

tests:
	g++ -fno-exceptions -fno-rtti -o test test.cpp upnp.cpp -luuid
