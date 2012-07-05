CC = g++ -O2 -DNDEBUG -std=gnu++0x

idock: obj/scoring_function.o obj/box.o obj/quaternion.o obj/thread_pool.o obj/receptor.o obj/ligand.o obj/grid_map_task.o obj/monte_carlo_task.o obj/main.o
	$(CC) -o $@ $^ -pthread -lboost_system -lboost_thread -lboost_filesystem -lboost_program_options -lmongoclient -lPocoNet

obj/%.o: src/%.cpp 
	$(CC) -o $@ $< -c

clean:
	rm idock obj/*.o
