CC := gcc
CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation
ROM ?= Top Gear 2 (USA).sfc

ifeq ($(OS),Windows_NT)
    EXE_EXT := .exe
    DLL_TARGET := topgear2.dll
    SHARED_FLAGS := -shared
    RES_OBJS := topgear2_res.o
    PIC_FLAG :=
    LDFLAGS ?= -lm
    PYTHON ?= python
else
    EXE_EXT :=
    RES_OBJS :=
    PIC_FLAG := -fPIC
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        DLL_TARGET := libtopgear2.dylib
        SHARED_FLAGS := -dynamiclib
        LDFLAGS ?= -ldl -lm -Wl,-rpath,@executable_path
    else
        DLL_TARGET := libtopgear2.so
        SHARED_FLAGS := -shared
        LDFLAGS ?= -ldl -lm
    endif
    PYTHON ?= python3
endif

ALL_CFLAGS := $(CFLAGS) $(PIC_FLAG) -Iinclude -I. -Isnesrecomp_repo/runner/src/snes

TARGET := topgear2$(EXE_EXT)
TEST_TARGET := test_tg2_boot$(EXE_EXT)
REGRESSION_TARGET := test_regressions$(EXE_EXT)
DEFINITIVE_TARGET := test_definitive_edition$(EXE_EXT)
RACE_TARGET := test_race_and_money$(EXE_EXT)
DIAG_TARGET := diag_frames$(EXE_EXT)

RUNTIME_SRCS := \
	src/topgear2_sha256.c \
	src/topgear2_platform.c \
	src/topgear2_machine.c \
	src/topgear2_ppu_dma.c \
	src/topgear2_video.c \
	src/topgear2_scheduler.c \
	src/topgear2_interp.c \
	src/topgear2_audio.c \
	src/topgear2_runtime_api.c

RUNTIME_OBJS := $(RUNTIME_SRCS:.c=.o)

ifeq ($(NO_LIFT),1)
    GENERATED_SRCS := generated/tg2_dispatch_stub.c
else
    GENERATED_SRCS := $(filter-out generated/tg2_dispatch_stub.c,$(wildcard generated/*.c))
endif
GENERATED_OBJS := $(GENERATED_SRCS:.c=.o)

APU_OBJS := apu.o spc.o dsp.o interp816.o

ALL_CORE_OBJS := $(RUNTIME_OBJS) $(GENERATED_OBJS) $(APU_OBJS)

.PHONY: all clean distclean test diag lift dll check-gen release-romless

all: check-gen $(TARGET) $(DLL_TARGET)

dll: check-gen $(DLL_TARGET)

check-gen:
ifeq ($(strip $(GENERATED_SRCS)),)
	@$(PYTHON) -c "import sys; sys.stderr.write('ERROR: generated/*.c not found. Run make lift ROM=\"path/to/Top Gear 2 (USA).sfc\" or build with NO_LIFT=1.\n'); sys.exit(1)"
endif

generated/tg2_dispatch_stub.c: tools/tg2_lifter.py
	$(PYTHON) tools/tg2_lifter.py --stub --out-dir generated

$(DLL_TARGET): check-gen $(ALL_CORE_OBJS)
	$(CC) $(SHARED_FLAGS) $(ALL_CFLAGS) $(ALL_CORE_OBJS) -o $@ $(LDFLAGS)

$(TARGET): check-gen src/topgear2_runner.o src/topgear2_menu.o $(RES_OBJS) $(ALL_CORE_OBJS)
	$(CC) $(ALL_CFLAGS) src/topgear2_runner.o src/topgear2_menu.o $(RES_OBJS) $(ALL_CORE_OBJS) -o $@ $(LDFLAGS)
	@echo ==========================================================
	@echo Build successful! Run ./$(TARGET) to play Top Gear 2.
	@echo ==========================================================

$(REGRESSION_TARGET): check-gen tests/test_regressions.o src/topgear2_menu.o $(ALL_CORE_OBJS)
	$(CC) $(ALL_CFLAGS) tests/test_regressions.o src/topgear2_menu.o $(ALL_CORE_OBJS) -o $@ $(LDFLAGS)

$(TEST_TARGET): check-gen tests/test_tg2_boot.o $(ALL_CORE_OBJS)
	$(CC) $(ALL_CFLAGS) tests/test_tg2_boot.o $(ALL_CORE_OBJS) -o $@ $(LDFLAGS)

$(DEFINITIVE_TARGET): check-gen tests/test_definitive_edition.o $(ALL_CORE_OBJS)
	$(CC) $(ALL_CFLAGS) tests/test_definitive_edition.o $(ALL_CORE_OBJS) -o $@ $(LDFLAGS)

$(RACE_TARGET): check-gen tests/test_race_and_money.o $(ALL_CORE_OBJS)
	$(CC) $(ALL_CFLAGS) tests/test_race_and_money.o $(ALL_CORE_OBJS) -o $@ $(LDFLAGS)

$(DIAG_TARGET): check-gen tests/diag_frames.o $(ALL_CORE_OBJS)
	$(CC) $(ALL_CFLAGS) tests/diag_frames.o $(ALL_CORE_OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@

topgear2_res.o: topgear2.rc topgear2.ico
	windres topgear2.rc -o topgear2_res.o

apu.o: snesrecomp_repo/runner/src/snes/apu.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@

spc.o: snesrecomp_repo/runner/src/snes/spc.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@

dsp.o: snesrecomp_repo/runner/src/snes/dsp.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@

interp816.o: snesrecomp_repo/runner/src/snes/interp816.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@

lift:
	$(PYTHON) tools/tg2_lifter.py "$(ROM)" --out-dir generated

ifeq ($(NO_LIFT),1)
test: $(REGRESSION_TARGET)
	./$(REGRESSION_TARGET)
else
test: $(REGRESSION_TARGET) $(TEST_TARGET) $(DEFINITIVE_TARGET) $(RACE_TARGET)
	./$(REGRESSION_TARGET) "$(ROM)"
	./$(TEST_TARGET) "$(ROM)"
	./$(DEFINITIVE_TARGET) "$(ROM)"
	./$(RACE_TARGET) "$(ROM)"
endif

diag: $(DIAG_TARGET)
	./$(DIAG_TARGET) "$(ROM)" 180

clean:
	$(PYTHON) -c "import glob, os; [os.remove(p) for pat in ['$(TARGET)', '$(DLL_TARGET)', '$(TEST_TARGET)', '$(REGRESSION_TARGET)', '$(DEFINITIVE_TARGET)', '$(RACE_TARGET)', '$(DIAG_TARGET)', 'src/*.o', 'tests/*.o', 'generated/*.o', '*.o'] for p in glob.glob(pat) if os.path.exists(p)]"

distclean: clean
	$(PYTHON) -c "import shutil; shutil.rmtree('generated', ignore_errors=True)"

release-romless:
	$(MAKE) NO_LIFT=1 distclean
	$(MAKE) NO_LIFT=1 all
	$(PYTHON) -c "import os, shutil; d='dist_topgear2'; shutil.rmtree(d, ignore_errors=True); os.makedirs(d, exist_ok=True); [shutil.copy2(f, os.path.join(d, os.path.basename(f))) for f in ['$(TARGET)', '$(DLL_TARGET)', 'gamecontrollerdb.txt', 'LICENSE', 'THIRD_PARTY_LICENSES.md', 'README.md']]"
	@echo ==========================================================
	@echo ROM-less distribution staged in dist_topgear2/
	@echo Contains zero ROM code and no bundled SDL2 library.
	@echo ==========================================================
