BUILD := build

.PHONY: test clean

test:
	cmake -S . -B $(BUILD)
	cmake --build $(BUILD) --target table_test
	ctest --test-dir $(BUILD) --output-on-failure

clean:
	rm -rf $(BUILD)
