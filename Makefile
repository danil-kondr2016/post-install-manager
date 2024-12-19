.PHONY: clean

CPPFLAGS=-Iinclude/ $(shell pkg-config --cflags expat)
CFLAGS=-g
LDFLAGS=$(shell pkg-config --libs expat) -lcomctl32 -luxtheme -lgdi32
WINDRES=windres

OBJECTS=obj/cwalk/cwalk.o \
	obj/postinst/arena.o \
	obj/postinst/commands.o \
	obj/postinst/copy.o \
	obj/postinst/delete.o \
	obj/postinst/fatal.o \
	obj/postinst/install.o \
	obj/postinst/mkdir.o \
	obj/postinst/move.o \
	obj/postinst/parser.o \
	obj/postinst/recurse.o \
	obj/postinst/resource.o \
	obj/postinst/rename.o \
	obj/postinst/repo.o \
	obj/postinst/stricmp.o \
	obj/postinst/postinst.o \
	obj/postinst/tests.o \

postinst.exe: $(OBJECTS)
	$(CC) -o $@ $(CFLAGS) $(OBJECTS) $(LDFLAGS) -mwindows -municode

obj/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

obj/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

obj/%.o: src/%.rc
	@mkdir -p $(@D)
	$(WINDRES) $(CPPFLAGS) -c65001 -o $@ $<

clean:
	-rm -rfv obj/ bin/
