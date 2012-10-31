CGC=$(NV_WINCE_T2_PLAT)/host_bin/cgc
SHADERFIX=$(NV_WINCE_T2_PLAT)/host_bin/shaderfix

CFLAGS += -g
pbuffer-test: pbuffer-test.c wrap.o hook.o # compiler-wrap.o
	gcc $(CPPFLAGS) $(CFLAGS) $^ -o pbuffer-test $(LDFLAGS) -lEGL -lGLESv2 -ldl

pbuffer-test-gles1: pbuffer-test-gles1.c wrap.o hook.o # compiler-wrap.o
	gcc $(CPPFLAGS) $(CFLAGS) $^ -o pbuffer-test-gles1 $(LDFLAGS) -lEGL -lGLESv1_CM

standalone-test: standalone-test.c nvmap.o nvhost.o
	gcc $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDFLAGS)

compiler: compiler.c
	gcc $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lcgdrv

clean:
	rm -f pbuffer-test pbuffer-teste-gles1 standalone-test wrap.o compiler-wrap.o hook.o nvmap.o nvhost.o

%.frag.cgbin:%.frag.glsl
	"$(CGC)" -profile ar20fp -ogles -o "$@" "$<"

%.nvbf:%.frag.cgbin
	"$(SHADERFIX)" -o "$@" "$<"
