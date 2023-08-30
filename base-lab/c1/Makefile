OUT_DIR=./build

all: build

build: clean
	mkdir -p $(OUT_DIR)
	p4c -o $(OUT_DIR) /shared/srv6_forward.p4

clean:
	rm -rf ./build