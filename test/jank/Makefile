default: help

# The general philosophy and functionality of this makefile is shamelessly stolen from compiler explorer

help: # with thanks to Ben Rady
	@grep -E '^[0-9a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

PODMAN=podman

data/.built: Dockerfile entry.sh
	$(PODMAN) build -t cpptrace-container .
	mkdir -p data
	touch data/.built

.PHONY: build
build: data/.built  ## build the container

.PHONY: run
run: data/.built data/.cloned  ## run the container
	$(PODMAN) run \
		--user=cpptrace \
		--cap-drop=all \
		-v $(realpath ../../src):/opt/work/jank/compiler+runtime/third-party/cpptrace/src \
		-v $(realpath ../../CMakeLists.txt):/opt/work/jank/compiler+runtime/third-party/cpptrace/CMakeLists.txt \
		-it \
		cpptrace-container
		# -v $(realpath data/jank):/opt/work/jank:rw \
	# $(PODMAN) run --user=cpptrace --cap-drop=all --network none -v data/jank:/opt/work/jank:Z -it cpptrace-container

.PHONY: clone
clone: data/.cloned  ## clone code

data/.cloned:
	mkdir -p data
	cd data && git clone --recurse-submodules https://github.com/jank-lang/jank.git && cd jank && git checkout 5668b16
	touch data/.cloned

.PHONY: clean
clean:  ## clean
	rm -rf data
