TARGET=$(patsubst src/%.c,%,$(wildcard src/*.c))
SIMPLEC_FLAGS=--dev
MEM=build/example.mem
prg?=all

.SECONDARY:
.PHONY: all clean run test
all: $(patsubst %,build/c_%,$(TARGET)) $(patsubst %,build/%,$(TARGET))

build:
	mkdir -p $@

test_out:
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

$(MEM): all
	@printf "" > $@
	@echo newos os fromfile ../jaris/asm/bin/os >> $@
	@echo newstdlib stdlib fromfile ../jaris/asm/bin/stdlib >> $@
	@echo setbootloader ../jaris/asm/bin/bootloader >> $@
	@echo newfile font fromfile ../jaris/font >> $@
	@echo newfile sh fromfile ../jaris/asm/bin/sh >> $@
	@echo newfile pipe fromfile ../jaris/asm/bin/pipe >> $@
	@echo newfile tee fromfile ../jaris/asm/bin/tee >> $@
	@echo newfile shutdown fromfile ../jaris/asm/bin/shutdown >> $@
	@for filename in $(TARGET); do \
		echo newfile $$filename fromfile build/$$filename >> $@ ; \
	done

%.mem.bin: %.mem
	../jaris/encodemem -o $@ $<

../simpleC: ../simpleC.c
	make -C .. simpleC

run: $(MEM).bin build/$(prg) build/c_$(prg)
	-./build/c_$(prg)
	-../jaris/sim -i "$(prg)\nshutdown\n" --mem $< $(args)

ifeq ($(prg),all)
test: $(MEM).bin $(patsubst %,test_out/%,$(TARGET)) $(patsubst %,test_out/c_%,$(TARGET)) | test_out
	for prg in $(TARGET); do git diff --no-index test_out/$$prg test_out/c_$$prg; done
else
test: $(MEM).bin test_out/$(prg) test_out/c_$(prg) | test_out
	git diff --no-index test_out/$(prg) test_out/c_$(prg)
endif

test_out/c_%: build/c_% | test_out
	./$< > $@

test_out/%: $(MEM).bin build/% | test_out
	../jaris/sim -i "pipe $(@F) | tee test.out\nshutdown\n" --mem $< $(args)
	../jaris/decodemem test.out sim.mem.out.bin | tr '\000\001' ' #' > $@

clean:
	rm -rf build
	rm -rf test_out
	rm -f sim.mem.out.bin

