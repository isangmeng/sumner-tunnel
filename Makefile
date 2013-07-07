
CFLAGS += -g 

default: build

rpm: dist

dist: build

CLEANS = sumner
build: sumner

CLEANS += sumner.o
sumner: sumner.o 
	$(CC) $(CFLAGS) -o $@ $^

test:
	@echo "run tests..."

clean:
	rm -f $(CLEANS)
