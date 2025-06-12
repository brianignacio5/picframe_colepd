CFLAGS=-ggdb -O2 -arch x86_64 -I/usr/local/Cellar/gd/2.3.3_6/include
LDFLAGS=-arch x86_64 -L/usr/local/Cellar/gd/2.3.3_6/lib -lgd -lm

conv: conv.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f conv.o conv

.PHONY: clean
