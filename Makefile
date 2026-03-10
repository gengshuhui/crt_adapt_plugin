# crtadapt Makefile
#
# Targets:
#   make check           — show GCC + GLIBC versions in both Ubuntu 22 and Ubuntu 20
#   make build           — compile test_compat (no adapter) in Ubuntu 22 Docker
#   make analyze         — show GLIBC symbol requirements of compiled binary
#   make test-native     — run unadapted binary in Ubuntu 20 (expected: GLIBC error)
#   make build-adapted   — compile test_compat with crtadapt shim in Ubuntu 22 Docker
#   make test-adapted    — run adapted binary in Ubuntu 20 (expected: all PASS)
#   make all             — run full pipeline
#   make clean           — remove compiled binaries

WORK       := $(shell pwd)
BIN_DIR    := $(WORK)/bin
PLUGIN_DIR := $(WORK)/plugin

IMG_BUILD := crtadapt-build
IMG_TEST  := crtadapt-test

# Use sudo if current session does not have the docker group active
DOCKER    := $(shell id -nG | tr ' ' '\n' | grep -qx docker && echo docker || echo "sudo docker")

DOCKER_BUILD_OPTS := \
    -v $(WORK):/work:ro \
    -v $(BIN_DIR):/work/bin \
    --rm

# Plugin build: source ro, plugin dir writable (for crtadapt.so output)
DOCKER_PLUGIN_BUILD_OPTS := \
    -v $(WORK):/work:ro \
    -v $(PLUGIN_DIR):/work/plugin \
    --rm

DOCKER_TEST_OPTS := \
    -v $(BIN_DIR):/work/bin:ro \
    --rm

# -------------------------------------------------------------------------

.PHONY: all check build analyze test-native build-adapted test-adapted clean \
        docker-build-image docker-test-image \
        build-plugin build-plugin-adapted analyze-plugin test-plugin

all: check build analyze test-native build-adapted test-adapted

# -------------------------------------------------------------------------
# Docker image management
# -------------------------------------------------------------------------

docker-build-image:
	@echo ">>> Building Docker image: $(IMG_BUILD) (ubuntu:22.04)"
	$(DOCKER) build -t $(IMG_BUILD) -f docker/Dockerfile.build .

docker-test-image:
	@echo ">>> Building Docker image: $(IMG_TEST) (ubuntu:20.04)"
	$(DOCKER) build -t $(IMG_TEST) -f docker/Dockerfile.test .

# -------------------------------------------------------------------------
# Environment checks
# -------------------------------------------------------------------------

check: docker-build-image docker-test-image
	@echo ""
	@echo "=========================================="
	@echo " CHECK: Ubuntu 22.04 build environment"
	@echo "=========================================="
	$(DOCKER) run $(DOCKER_BUILD_OPTS) $(IMG_BUILD) bash /work/scripts/check-env.sh

	@echo ""
	@echo "=========================================="
	@echo " CHECK: Ubuntu 20.04 test environment"
	@echo "=========================================="
	$(DOCKER) run \
	    -v $(WORK)/scripts:/scripts:ro \
	    --rm $(IMG_TEST) bash /scripts/check-env.sh

# -------------------------------------------------------------------------
# Build without adapter (native Ubuntu 22 GCC, no version pinning)
# -------------------------------------------------------------------------

build: docker-build-image
	@echo ""
	@echo "=========================================="
	@echo " BUILD: test_compat (no adapter)"
	@echo "=========================================="
	@mkdir -p $(BIN_DIR)
	$(DOCKER) run $(DOCKER_BUILD_OPTS) $(IMG_BUILD) \
	    gcc -o /work/bin/test_compat \
	        /work/test/test_compat.c \
	        -pthread -static-libgcc \
	        -Wall -O2
	@echo "Output: $(BIN_DIR)/test_compat"

# -------------------------------------------------------------------------
# Symbol analysis
# -------------------------------------------------------------------------

analyze: docker-build-image
	@echo ""
	@echo "=========================================="
	@echo " ANALYZE: GLIBC symbol requirements"
	@echo "=========================================="
	$(DOCKER) run $(DOCKER_BUILD_OPTS) $(IMG_BUILD) \
	    bash /work/scripts/analyze-symbols.sh /work/bin/test_compat

# -------------------------------------------------------------------------
# Test unadapted binary on Ubuntu 20
# -------------------------------------------------------------------------

test-native: docker-test-image
	@echo ""
	@echo "=========================================="
	@echo " TEST (native): run on Ubuntu 20.04"
	@echo "=========================================="
	@echo "Expecting GLIBC version error..."
	-$(DOCKER) run $(DOCKER_TEST_OPTS) $(IMG_TEST) /work/bin/test_compat
	@echo "(non-zero exit above = compatibility issue confirmed)"

# -------------------------------------------------------------------------
# Build with crtadapt shim
# -------------------------------------------------------------------------

build-adapted: docker-build-image
	@echo ""
	@echo "=========================================="
	@echo " BUILD: test_compat WITH crtadapt adapter"
	@echo "=========================================="
	@mkdir -p $(BIN_DIR)
	$(DOCKER) run $(DOCKER_BUILD_OPTS) $(IMG_BUILD) bash -c " \
	    gcc -o /work/bin/test_compat_adapted \
	        /work/src/crtadapt.c \
	        /work/test/test_compat.c \
	        -include /work/src/crtadapt-symver.h \
	        -pthread -static-libgcc -Wall -O2 && \
	    patchelf --add-needed libpthread.so.0 /work/bin/test_compat_adapted"
	@echo "Output: $(BIN_DIR)/test_compat_adapted"

analyze-adapted: docker-build-image
	@echo ""
	@echo "=========================================="
	@echo " ANALYZE: adapted binary GLIBC requirements"
	@echo "=========================================="
	$(DOCKER) run $(DOCKER_BUILD_OPTS) $(IMG_BUILD) \
	    bash /work/scripts/analyze-symbols.sh /work/bin/test_compat_adapted

# -------------------------------------------------------------------------
# Test adapted binary on Ubuntu 20
# -------------------------------------------------------------------------

test-adapted: docker-test-image
	@echo ""
	@echo "=========================================="
	@echo " TEST (adapted): run on Ubuntu 20.04"
	@echo "=========================================="
	$(DOCKER) run $(DOCKER_TEST_OPTS) $(IMG_TEST) /work/bin/test_compat_adapted

# -------------------------------------------------------------------------
# GCC Plugin workflow
#
# The plugin emits the same .symver + COMDAT wrapper assembly automatically
# into every compiled TU, replacing the manual -include / crtadapt.c approach.
# Usage: gcc -fplugin=/work/plugin/crtadapt.so ...
# -------------------------------------------------------------------------

build-plugin: docker-build-image
	@echo ""
	@echo "=========================================="
	@echo " BUILD: crtadapt GCC plugin"
	@echo "=========================================="
	$(DOCKER) run $(DOCKER_PLUGIN_BUILD_OPTS) $(IMG_BUILD) bash -c " \
	    PLUGIN_INC=\$$(gcc -print-file-name=plugin)/include && \
	    g++ -I\$$PLUGIN_INC -fPIC -O2 -Wall -shared \
	        -o /work/plugin/crtadapt.so \
	        /work/plugin/crtadapt_plugin.c"
	@echo "Output: $(PLUGIN_DIR)/crtadapt.so"

build-plugin-adapted: docker-build-image build-plugin
	@echo ""
	@echo "=========================================="
	@echo " BUILD: test_compat WITH GCC plugin"
	@echo "=========================================="
	@mkdir -p $(BIN_DIR)
	$(DOCKER) run $(DOCKER_BUILD_OPTS) $(IMG_BUILD) bash -c " \
	    gcc -o /work/bin/test_compat_plugin \
	        /work/test/test_compat.c \
	        -fplugin=/work/plugin/crtadapt.so \
	        -pthread -static-libgcc -Wall -O2 && \
	    patchelf --add-needed libpthread.so.0 /work/bin/test_compat_plugin"
	@echo "Output: $(BIN_DIR)/test_compat_plugin"

analyze-plugin: docker-build-image
	@echo ""
	@echo "=========================================="
	@echo " ANALYZE: plugin-adapted binary GLIBC requirements"
	@echo "=========================================="
	$(DOCKER) run $(DOCKER_BUILD_OPTS) $(IMG_BUILD) \
	    bash /work/scripts/analyze-symbols.sh /work/bin/test_compat_plugin

test-plugin: docker-test-image
	@echo ""
	@echo "=========================================="
	@echo " TEST (plugin): run on Ubuntu 20.04"
	@echo "=========================================="
	$(DOCKER) run $(DOCKER_TEST_OPTS) $(IMG_TEST) /work/bin/test_compat_plugin

# -------------------------------------------------------------------------

clean:
	rm -f $(BIN_DIR)/test_compat $(BIN_DIR)/test_compat_adapted \
	      $(BIN_DIR)/test_compat_plugin $(PLUGIN_DIR)/crtadapt.so
