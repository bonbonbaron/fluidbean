ifdef dbg
run = gdb --args
cmake_args += -DCMAKE_BUILD_TYPE=Debug
else
run =
cmake_args += -DCMAKE_BUILD_TYPE=Release
endif

examples: example/sf_/Boomwhacker.sf2

ifdef sf3
examples: example/sf_/Boomwhacker.sf3
cmake_args += -DENABLE_SF3=YES
ifeq ($(sf3),stb)
cmake_args += -DSTB_VORBIS=YES
endif
endif

example/%: out/example
	@echo Run example
	@LD_LIBRARY_PATH=out $(run) $^ $* $*.pcm

clean:
	@rm -rf out

out/example: src/main.c src/sf3Inf.c out/libfluidbean.a
	@$(CC) -g -o $@ $< -I../include -Lout -lfluidbean -lbotox -lm src/sf3Inf.c

.PHONY: $(addprefix out/libfluidbean.,a)
$(addprefix out/libfluidbean.,a):
	@mkdir -p out
	@cd out && cmake $(cmake_args) ../../ && make
