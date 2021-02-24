/*
 * FIFO Scheduling Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (2)
 */

/*
 * STUDENT NUMBER: s1870697
 * 
 * ## BONUS MARK - Question: 
 * Why does sched-test2 causes the system to stop responding to further commands
 * when running the FIFO algorithm (i.e Why does the Enter command not stop the test?)
 * 
 * # Short answer: 
 * It is a case of infinite loop in the execution code of sched-test2 in the case of FIFO. The instruction while(getch()!='\n') that listens to 
 * whether the 'Enter' key is pressed (i.e a new line is created) is no longer reachable.
 * 
 * # Explanation: 
 * One observation when running sched-test2 is that the outputs printed to terminal correspond to the thread
 * that first ran as such: 
 * "Thread 1 ticking every 1.5s\n"
 * "TICK 1!\n"
 * "TICK 1!\n"
 * ... and it continues printing TICK 1 until InfOS is forcibly quit through the host machine terminal using Ctrl+C.
 * Thread 2 is never called because we never see the terminal printing outputs related to the second thread: "TICK 2!"
 * 
 * The reason being is that of the nature of FIFO. The algorithm runs a thread until its completion or until a thread becomes SLEEPING (or suspended),
 * which is when it will be removed from the runqueue as it is paused for execution until another event occurs. On the other hand, Round Robin
 * operates differently as the scheduler alternates between different threads after a certain time quanta is exceeded.
 * 
 * Knowing this, looking at the source code for sched-test2, we will notice that there is no call that puts a thread into sleep (usleep does not correspond
 * to a thread being put to sleep as it does not change the state of the thread, but mainly informs the system to wait a certain amount of time like 1.5s before moving to the next instruction line). 
 * Simply put, since the while condition is always satisfied in line 10 as there is no condition that changes the boolean variable terminate, the instruction lines that 
 * prints "TICK 1!" is ran infinitely.
 * 
 * In addition, the instruction lines corresponding to getch()!= '\n' which stops that test can no longer be reachable, hence why we notice this behaviour.
 * 
 */
#include <infos/kernel/sched.h>
#include <infos/kernel/thread.h>
#include <infos/kernel/log.h>
#include <infos/util/list.h>
#include <infos/util/lock.h>



using namespace infos::kernel;
using namespace infos::util;

/**
 * A FIFO scheduling algorithm
 */
class FIFOScheduler : public SchedulingAlgorithm
{
public:
	/**
	 * Returns the friendly name of the algorithm, for debugging and selection purposes.
	 */
	const char* name() const override { return "fifo"; }

	/**
	 * Called when a scheduling entity becomes eligible for running.
	 * @param entity
	 */
	void add_to_runqueue(SchedulingEntity& entity) override
	{	//Interrupts should be disabled when manipulating the runqueue. There will be no need to handle any errors of resource leaks
		//as the RAII wrapper helps deconstruct the l variable at the end of the lifetime of the function.
		UniqueIRQLock l;
		runqueue.enqueue(&entity); //Entity added to the end of the list
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
		runqueue.remove(&entity); //Entity removed from runqueue
		
	}

	/**
	 * Called every time a scheduling event occurs, to cause the next eligible entity
	 * to be chosen.  The next eligible entity might actually be the same entity, if
	 * e.g. its timeslice has not expired, or the algorithm determines it's not time to change.
	 */
	SchedulingEntity *pick_next_entity() override
	{
		//Returned when there are no entities in runqueue.
		if (runqueue.count() == 0){ 
			return NULL; 
		} 
		//Return the first eligible entity from the list.
		return runqueue.first(); 
		
	}

private:
	// A list containing the current runqueue.
	List<SchedulingEntity *> runqueue;
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

RegisterScheduler(FIFOScheduler);
