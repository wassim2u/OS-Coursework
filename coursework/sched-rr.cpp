/*
 * Round-robin Scheduling Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (2)
 */

/*
 * STUDENT NUMBER: s
 */
#include <infos/kernel/sched.h>
#include <infos/kernel/thread.h>
#include <infos/kernel/log.h>
#include <infos/util/list.h>
#include <infos/util/lock.h>


using namespace infos::kernel;
using namespace infos::util;

/**
 * A round-robin scheduling algorithm
 */
class RoundRobinScheduler : public SchedulingAlgorithm
{
public:
	/**
	 * Returns the friendly name of the algorithm, for debugging and selection purposes.
	 */
	const char* name() const override { return "rr"; }

	/**
	 * Called when a scheduling entity becomes eligible for running.
	 * @param entity
	 */
	void add_to_runqueue(SchedulingEntity& entity) override
	{
		//Interrupts should be disabled when manipulating the runqueue. There will be no need to handle any errors of resource leaks
		//as the RAII wrapper helps deconstruct the l variable at the end of the lifetime of the function.
		UniqueIRQLock l;
		runqueue.enqueue(&entity); //appends entity to end of list
	}

	/**
	 * Called when a scheduling entity is no longer eligible for running.
	 * @param entity
	 */
	void remove_from_runqueue(SchedulingEntity& entity) override
	{
		//Interrupts should be disabled when manipulating the runqueue. There will be no need to handle any errors of resource leaks
		//as the RAII wrapper helps deconstruct the l variable at the end of the lifetime of the function.
		UniqueIRQLock l;
		runqueue.remove(&entity); //removes entitiy
	}

	/**
	 * Called every time a scheduling event occurs, to cause the next eligible entity
	 * to be chosen.  The next eligible entity might actually be the same entity, if
	 * e.g. its timeslice has not expired.
	 */
	SchedulingEntity *pick_next_entity() override
	{
		
		if (runqueue.count() == 0) return NULL; //Returned when there are no entities in runqueue.

		if (runqueue.count() == 1) return runqueue.first(); //Return the only entity in runqueue.

		 //When a new task is to be picked for execution, it is removed from the front of the list, and placed at the back.  
		 // Then, this task is allowed to run for its timeslice.
		auto entity = runqueue.first(); 
		runqueue.remove(entity); 
		runqueue.append(entity);
		return entity;
	


	}

private:
	// A list containing the current runqueue.
	List<SchedulingEntity *> runqueue;
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

RegisterScheduler(RoundRobinScheduler);
