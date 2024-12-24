.PHONY: clean

CPPFLAGS=-Iinclude/ $(shell pkg-config --cflags expat)
CFLAGS=-g
LDFLAGS=$(shell pkg-config --libs expat) -lcomctl32 -luxtheme -lgdi32 -lole32 -luuid
WINDRES=windres

OBJECTS=$(patsubst src/%.c,obj/%.o,$(wildcard src/**/*.c))
DEPS=$(patsubst src/%.c,deps/%.d,$(wildcard src/**/*.c))

bin/postinst.exe: $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) -o $@ $(CFLAGS) $(OBJECTS) $(LDFLAGS) -mwindows -municode

obj/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

obj/%.o: src/%.c deps/%.d
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

obj/%.o: src/%.rc
	@mkdir -p $(@D)
	$(WINDRES) $(CPPFLAGS) -c65001 -o $@ $<

deps: $(DEPS)

deps/%.d: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -MM $< -MT $(patsubst src/%.c,obj/%.o,$<)  > $@

obj/postinst/resource.o: src/postinst/version.h
obj/postinst/version.o: src/postinst/version.h
-include $(wildcard deps/**/*.d)

clean:
	-rm -rfv obj/ bin/ deps/

