exec = test.out
sources = $(wildcard src/*.c)
objects = $(sources:.c=.o)
flags = -I$$HOME/.local/include -I./../src/include/ -I./external/libjson/src/include -g -Wall -lm -Wl,--no-as-needed -ldl -fPIC -rdynamic -L$$HOME/.local/lib -lfjb -lhashmap -ljson -lscss -lpthread

$(exec): $(objects) libfjb.a
	gcc $(objects) $(flags) -o $(exec)

libfjb.a: libjson.a libhashmap.a
	cd .. && make clean; make install && cp libfjb.a tests/.
	ar rcs $@ $^

libjson.a:
	cd ../external/libjson ; make ; make install ; mv ./libjson.a ../../tests/.

libscss.a:
	cd ../external/scss ; make ; make install ; mv ./libscss.a ../../tests/.

libhashmap.a:
	cd ../external/hashmap ; make clean ; make ; make install ; mv ./libhashmap.a ../../tests/.

%.o: %.c include/%.h
	gcc -c $(flags) $< -o $@

clean:
	-rm *.out
	-rm *.o
	-rm *.a
	-rm src/*.o

lint:
	clang-tidy src/*.c src/include/*.h
