CFLAGS += -g
pbuffer-test: pbuffer-test.c wrap.o hook.o # compiler-wrap.o
	gcc $(CPPFLAGS) $(CFLAGS) $^ -o pbuffer-test $(LDFLAGS) -lEGL -lGLESv2

pbuffer-test-gles1: pbuffer-test-gles1.c wrap.o hook.o # compiler-wrap.o
	gcc $(CPPFLAGS) $(CFLAGS) $^ -o pbuffer-test-gles1 $(LDFLAGS) -lEGL -lGLESv1_CM

stanadlone-test: standalone-test.c
	gcc $(CPPFLAGS) $(CFLAGS) $^ -o pbuffer-test $(LDFLAGS)

clean:
	rm pbuffer-test pbuffer-teste-gles1 standalone-test wrap.o compiler-wrap.o hook.o
