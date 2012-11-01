CGC=$(NV_WINCE_T2_PLAT)/host_bin/cgc
SHADERFIX=$(NV_WINCE_T2_PLAT)/host_bin/shaderfix

CFLAGS += -g
pbuffer-test: pbuffer-test.c tests.o wrap.o hook.o # compiler-wrap.o
	gcc $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lEGL -lGLESv2 -ldl

window-test: window-test.c tests.o wrap.o hook.o # compiler-wrap.o
	gcc $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lEGL -lGLESv2 -lX11 -ldl

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

%.vert.cgbin:%.vert.glsl
	"$(CGC)" -profile ar20vp -ogles -o "$@" "$<"

%.nvbv:%.vert.cgbin
	"$(SHADERFIX)" -o "$@" "$<"
