pbuffer-test: pbuffer-test.c
	gcc $(CPPFLAGS) pbuffer-test.c -o pbuffer-test $(LDFLAGS) -lEGL -lGLESv2

