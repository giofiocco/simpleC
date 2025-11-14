TARGET=$(patsubst src/%.c,%,$(wildcard src/*.c))
SIMPLEC_FLAGS=--dev

.SECONDARY:
.PHONY: all clean run
all: $(patsubst %,build/c_%,$(TARGET)) $(patsubst %,build/%,$(TARGET))

build:
	mkdir -p $@

build/c_%: src/%.c | build
	$(CC) -Wall -Wextra -g -o $@ $< clib_wrapper.c

build/%: build/%.o build/c_stdlib.o | build
	../jaris/linker -g --stdlib-path ../jaris/asm/bin/stdlib -o $@ $(filter %.o,$^)

build/%.o: build/%.asm | build
	../jaris/assembler -g -o $@ $<

build/%.asm: src/%.c ../simpleC | build
	../simpleC $(SIMPLEC_FLAGS) -o $@ $<

build/c_stdlib.o: c_stdlib.asm | build
	../jaris/assembler -g -o $@ $<

build/example.mem: all
	@printf "" > build/example.mem
	@echo newos os fromfile ../jaris/asm/bin/os >> build/example.mem
	@echo newstdlib stdlib fromfile ../jaris/asm/bin/stdlib >> build/example.mem
	@echo setbootloader ../jaris/asm/bin/bootloader >> build/example.mem
	@echo newfile font fromfile ../jaris/font >> build/example.mem
	@echo newfile sh fromfile ../jaris/asm/bin/sh >> build/example.mem
	@echo newfile shutdown fromfile ../jaris/asm/bin/shutdown >> build/example.mem
	@for filename in $(TARGET); do \
		echo newfile $$filename fromfile build/$$filename >> build/example.mem ; \
	done

%.mem.bin: %.mem
	../jaris/encodemem -o $@ $<

../simpleC: ../simpleC.c
	make -C .. simpleC

run: build/example.mem.bin
	-./build/c_$(prg)
	-../jaris/sim -i "$(prg)\nshutdown\n" --mem $< $(args)

clean:
	rm -rf build
	rm -f sim.mem.out.bin

