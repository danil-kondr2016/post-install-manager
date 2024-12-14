CPPFLAGS=-Iinclude/

obj/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

obj/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<
