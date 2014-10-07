all: sandbox.so test

test: test.c Makefile
	gcc $< -o $@

check: sandbox.so test
	LD_PRELOAD=./sandbox.so ./test

sandbox.so: inject.c Makefile
	gcc $< -shared -fPIC -o $@ `pkg-config --libs --cflags libseccomp` -ldl
