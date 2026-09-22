MODE   ?= Debug
FILTER ?= *
BUILD  := build-$(MODE)
TABLE_TEST     := table_test
COLLECTOR_TEST := tgc_test

.PHONY: configure test-table test-collector test test-all-modes fmt clean

configure:
	cmake -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=$(MODE)

test-table: configure
	cmake --build $(BUILD) --target $(TABLE_TEST)
	./$(BUILD)/$(TABLE_TEST) --gtest_filter='$(FILTER)'

test-tgc: configure
	cmake --build $(BUILD) --target $(COLLECTOR_TEST)
	./$(BUILD)/$(COLLECTOR_TEST) --gtest_filter='$(FILTER)'

test: configure
	cmake --build $(BUILD)
	ctest --test-dir $(BUILD) --output-on-failure

test-all-modes:
	$(MAKE) test MODE=Debug
	$(MAKE) test MODE=RelWithDebInfo

fmt:
	git ls-files -co --exclude-standard '*.cpp' '*.h' | xargs clang-format -i

clean:
	rm -rf build-*