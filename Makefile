MAKEFLAGS += --no-print-directory
all: master development

master/Makefile: CMakeLists.txt
	cmake -DCMAKE_BUILD_TYPE=Release -DSTATIC=ON -DCADICAL_GIT_TAG=master -B master
master: master/Makefile
	cmake --build master --parallel

development/Makefile: CMakeLists.txt
	cmake -DCMAKE_BUILD_TYPE=Release -DSTATIC=ON -DCADICAL_GIT_TAG=development -B development
development: development/Makefile
	cmake --build development --parallel

debug: CMakeLists.txt
	cmake -DCMAKE_BUILD_TYPE=Debug -DCADICAL_GIT_TAG=development -B development
	cmake --build development --parallel
development/fuzzer: CMakeLists.txt
	cmake -DCMAKE_BUILD_TYPE=Release -DSTATIC=ON -DFUZZ=ON -DCADICAL_GIT_TAG=development -B development
	cmake --build development --parallel
run: development/fuzzer development
	./development/certified ./development/bmc bug.aag wit
fuzz: development/fuzzer development
	./development/fuzzer ./development/bmc

clean:
	rm -rf build bin master development
.PHONY: all docker clean run fuzz master development
