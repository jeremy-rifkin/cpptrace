default: help

# The general philosophy and functionality of this makefile is shamelessly stolen from compiler explorer

help: # with thanks to Ben Rady
	@grep -E '^[0-9a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

.PHONY: build
build: debug  ## build in debug mode

.PHONY: debug
debug:  ## build in debug mode
	cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DCPPTRACE_BUILD_TESTING=On
	cmake --build build

.PHONY: release
release:  ## build in release mode (with debug info)
	cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DCPPTRACE_BUILD_TESTING=On
	cmake --build build

.PHONY: debug-msvc
debug-msvc:  ## build in debug mode
	cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DCPPTRACE_BUILD_TESTING=On
	cmake --build build --config Debug

.PHONY: release-msvc
release-msvc:  ## build in release mode (with debug info)
	cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DCPPTRACE_BUILD_TESTING=On
	cmake --build build --config RelWithDebInfo

.PHONY: clean
clean:  ## clean
	rm -rf build

.PHONY: test
test: debug  ## test
	cd build && ninja test

.PHONY: test-release
test-release: release  ## test-release
	cd build && ninja test

# .PHONY: test-msvc
# test-msvc: debug-msvc  ## test
# 	cmake --build build --target RUN_TESTS --config Debug

# .PHONY: test-msvc-release
# test-msvc-release: release-msvc  ## test-release
# 	cmake --build build --target RUN_TESTS --config Release
