# plugin/Makefile — build crtadapt.so GCC compatibility plugin
#
# Requires: gcc-11-plugin-dev (Ubuntu 22 package)
#
# Usage inside Ubuntu 22 Docker build container:
#   make -C plugin
#
# The top-level Makefile drives this via the 'build-plugin' target.

PLUGIN_INC := $(shell gcc -print-file-name=plugin)/include

crtadapt.so: crtadapt_plugin.c
	g++ -I$(PLUGIN_INC) -fPIC -O2 -Wall -shared -o crtadapt.so crtadapt_plugin.c

.PHONY: clean
clean:
	rm -f crtadapt.so
