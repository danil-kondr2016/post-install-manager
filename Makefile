.PHONY: clean

CPPFLAGS=-Iinclude/ $(shell pkg-config --cflags expat)
LDFLAGS=$(shell pkg-config --libs expat)

obj/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

obj/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	-rm -rfv obj/ bin/
