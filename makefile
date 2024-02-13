bin/ss: bin/common.o bin/instance.o bin/simulator.o bin/snapshot.o main.c
	@echo "compiling main executable..."
	@clang  -o bin/ss bin/instance.o bin/simulator.o bin/common.o bin/snapshot.o main.c -lvulkan -lglfw -Wall

bin/common.o: common.c
	@echo "compiling common object file..."
	@clang -o bin/common.o -c common.c -O2 -Wall

bin/simulator.o: simulator.c 
	@echo "compiling simulator object file..."
	@clang -o bin/simulator.o -c simulator.c -O2 -Wall

bin/instance.o: instance.c
	@echo "compiling instance object file..."
	@clang -o bin/instance.o -c instance.c -O2 -Wall

bin/snapshot.o: snapshot.c
	@echo "compiling snapshot object file..."
	@clang -o bin/snapshot.o -c snapshot.c -O2 -Wall

.PHONY: shaders frag vert comp

shaders: shaders/Frendering.spv shaders/Vrendering.spv shaders/simulation.spv
	@echo "shaders compiled."


shaders/Frendering.spv:	rendering.frag
	@echo "compiling fragment shader..."
	@glslc -o shaders/Frendering.spv rendering.frag


shaders/Vrendering.spv:	rendering.vert
	@echo "compiling vertex shader..."
	@glslc -o shaders/Vrendering.spv rendering.vert


shaders/simulation.spv:	simulation.comp
	@echo "compiling compute shader..."
	@glslc -o shaders/simulation.spv simulation.comp
