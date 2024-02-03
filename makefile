bin/ss: bin/common.o bin/instance.o bin/simulator.o
	@echo "compiling main executable..."
	@clang  -o bin/ss bin/instance.o bin/simulator.o bin/common.o main.c -lvulkan -lglfw

bin/common.o: common.c
	@echo "compiling common object file..."
	@clang -o bin/common.o -c common.c -lvulkan -lglfw -O2

bin/simulator.o: simulator.c 
	@echo "compiling simulator object file..."
	@clang -o bin/simulator.o -c simulator.c -lvulkan -lglfw -O2

bin/instance.o: instance.c
	@echo "compiling instance object file..."
	@clang -o bin/instance.o -c instance.c -lvulkan -lglfw -O2

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
