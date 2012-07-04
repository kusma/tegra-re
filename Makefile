pbuffer-test: pbuffer-test.c wrap.o compiler-wrap.o hook.o
	gcc $(CPPFLAGS) -g $^ -o pbuffer-test $(LDFLAGS) -lEGL -lGLESv2
