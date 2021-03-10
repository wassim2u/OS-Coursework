/*
 * Buddy Page Allocation Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (3)
 */

/*
 * STUDENT NUMBER: s1870697
 */
#include <infos/mm/page-allocator.h>
#include <infos/mm/mm.h>
#include <infos/kernel/kernel.h>
#include <infos/kernel/log.h>
#include <infos/util/math.h>
#include <infos/util/printf.h>

using namespace infos::kernel;
using namespace infos::mm;
using namespace infos::util;

//Student Note: The length of free_areas is defined as MAX_ORDER +1, to incorporate the fact that
//the highest order in free_areas (the last index in the array) will be the index == MAX_ORDER
#define MAX_ORDER 17 


/**
 * A buddy page allocation algorithm.
 */
class BuddyPageAllocator : public PageAllocatorAlgorithm
{
private:
	/**
	 * Returns the number of pages that comprise a 'block', in a given order.
	 * @param order The order to base the calculation off of.
	 * @return Returns the number of pages in a block, in the order.
	 */
	static inline constexpr uint64_t pages_per_block(int order)
	{
		/* The number of pages per block in a given order is simply 1, shifted left by the order number.
		 * For example, in order-2, there are (1 << 2) == 4 pages in each block.
		 */
		return (1 << order);
	}
	
	/**
	 * Returns TRUE if the supplied page descriptor is correctly aligned for the 
	 * given order.  Returns FALSE otherwise.
	 * @param pgd The page descriptor to test alignment for.
	 * @param order The order to use for calculations.
	 */
	static inline bool is_correct_alignment_for_order(const PageDescriptor *pgd, int order)
	{
		// Calculate the page-frame-number for the page descriptor, and return TRUE if
		// it divides evenly into the number pages in a block of the given order.
		return (sys.mm().pgalloc().pgd_to_pfn(pgd) % pages_per_block(order)) == 0;
	}
	
	/** Given a page descriptor, and an order, returns the buddy PGD.  The buddy could either be
	 * to the left or the right of PGD, in the given order.
	 * @param pgd The page descriptor to find the buddy for.
	 * @param order The order in which the page descriptor lives.
	 * @return Returns the buddy of the given page descriptor, in the given order.
	 */
	PageDescriptor *buddy_of(PageDescriptor *pgd, int order)
	{
		// (1) Make sure 'order' is within range
		if (order >= MAX_ORDER) {
			return NULL;
		}

		// (2) Check to make sure that PGD is correctly aligned in the order
		if (!is_correct_alignment_for_order(pgd, order)) {
			return NULL;
		}
				
		// (3) Calculate the page-frame-number of the buddy of this page.
		// * If the PFN is aligned to the next order, then the buddy is the next block in THIS order.
		// * If it's not aligned, then the buddy must be the previous block in THIS order.
		uint64_t buddy_pfn = is_correct_alignment_for_order(pgd, order + 1) ?
			sys.mm().pgalloc().pgd_to_pfn(pgd) + pages_per_block(order) : 
			sys.mm().pgalloc().pgd_to_pfn(pgd) - pages_per_block(order);
		
		// (4) Return the page descriptor associated with the buddy page-frame-number.
		return sys.mm().pgalloc().pfn_to_pgd(buddy_pfn);
	}
	
	/**
	 * Inserts a block into the free list of the given order.  The block is inserted in ascending order.
	 * @param pgd The page descriptor of the block to insert.
	 * @param order The order in which to insert the block.
	 * @return Returns the slot (i.e. a pointer to the pointer that points to the block) that the block
	 * was inserted into.
	 */
	PageDescriptor **insert_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, find the slot in which the page descriptor
		// should be inserted.
		PageDescriptor **slot = &_free_areas[order];
		
		// Iterate whilst there is a slot, and whilst the page descriptor pointer is numerically
		// greater than what the slot is pointing to.
		while (*slot && pgd > *slot) {
			slot = &(*slot)->next_free;
		}
		
		// Insert the page descriptor into the linked list.
		pgd->next_free = *slot;
		*slot = pgd;
		
		// Return the insert point (i.e. slot)
		return slot;
	}
	
	/**
	 * Removes a block from the free list of the given order.  The block MUST be present in the free-list, otherwise
	 * the system will panic.
	 * @param pgd The page descriptor of the block to remove.
	 * @param order The order in which to remove the block from.
	 */
	void remove_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, iterate until the block has been located in the linked-list.
		PageDescriptor **slot = &_free_areas[order];
		while (*slot && pgd != *slot) {
			slot = &(*slot)->next_free;
		}

		// Make sure the block actually exists.  Panic the system if it does not.
		if (*slot != pgd){
			mm_log.messagef(LogLevel::DEBUG, "%lx" ,pgd);
		}
		assert(*slot == pgd);
		
		// Remove the block from the free list.
		*slot = pgd->next_free;
		pgd->next_free = NULL;
	}
	
	/**
	 * Given a pointer to a block of free memory in the order "source_order", this function will
	 * split the block in half, and insert it into the order below.
	 * @param block_pointer A pointer to a pointer containing the beginning of a block of free memory.
	 * @param source_order The order in which the block of free memory exists.  Naturally,
	 * the split will insert the two new blocks into the order below.
	 * @return Returns the left-hand-side of the new block.
	 */
	PageDescriptor *split_block(PageDescriptor **block_pointer, int source_order)
	{
		// Make sure there is an incoming pointer.
		assert(*block_pointer);
		
		// Make sure the block_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));
		
		//Make sure that the block isnt in the lowest order which is 0 as we cannot split blocks containing one page once we reach the lowest order.
		assert(source_order>0);
		//Calculate the block size of the original order
		auto source_block_size = pages_per_block(source_order);
		//Calculate the block size in the lower new order
		auto new_order = source_order -1 ;
		auto new_order_block_size = pages_per_block(source_order-1);
		//Retrieve the two blocks which are buddies 
		PageDescriptor *left_buddy = *block_pointer;
		PageDescriptor *right_buddy = *block_pointer + new_order_block_size;

		//Make sure that the buddies associated are correct
		assert(left_buddy == buddy_of(right_buddy, new_order));		

		//Remove the original block and insert the two new blocks (which are buddies) into the lower order
		remove_block(*block_pointer,source_order);
		PageDescriptor **block_1 = insert_block(left_buddy,new_order);
		PageDescriptor **block_2 = insert_block(right_buddy,new_order);

		return left_buddy;

	}
	
	/**
	 * Takes a block in the given source order, and merges it (and it's buddy) into the next order.
	 * This function assumes both the source block and the buddy block are in the free list for the
	 * source order.  If they aren't this function will panic the system.
	 * @param block_pointer A pointer to a pointer containing a block in the pair to merge.
	 * @param source_order The order in which the pair of blocks live.
	 * @return Returns the new slot that points to the merged block.
	 */
	PageDescriptor **merge_block(PageDescriptor **block_pointer, int source_order)
	{
		assert(*block_pointer);
		
		// Make sure the area_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));

		// Make sure order is less than the max order, since we cannot merge blocks to an order greater than  MAX_ORDER
		assert(source_order<MAX_ORDER);

		//Calculate the block size of the original order
		int source_block_size = pages_per_block(source_order);
		//Create pointers of the current block and its buddy to be merged
		PageDescriptor *block = *block_pointer;
		PageDescriptor *buddy_block = buddy_of(*block_pointer, source_order);
		PageDescriptor **merged_block;
		//Remove the blocks first from free_areas
		remove_block(*block_pointer,source_order);
		remove_block(buddy_block,source_order);

		int upper_new_order = source_order +1 ;
		//Since buddy_of may return blocks on right or left, see which buddy would be aligned in the higher order.
		//Insert block that is on the "left side" to ensure proper alignment in the higher order.
		if (is_correct_alignment_for_order(block, upper_new_order)){
			merged_block = insert_block(block,upper_new_order);
		}
		else{
			merged_block = insert_block(buddy_block,upper_new_order);
		}

		return merged_block;

		
	}

	
public:
	/**
	 * Constructs a new instance of the Buddy Page Allocator.
	 */
	BuddyPageAllocator() {
		// Iterate over each free area, and clear it.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			_free_areas[i] = NULL;
		}
	}
	
	/**
	 * Allocates 2^order number of contiguous pages
	 * @param order The power of two, of the number of contiguous pages to allocate.
	 * @return Returns a pointer to the first page descriptor for the newly allocated page range, or NULL if
	 * allocation failed.
	 */
	PageDescriptor *alloc_pages(int order) override
	{
		//assert failure if order isnt under MAX_ORDER 
		assert(order<=MAX_ORDER || order>=0);


		int current_order = order;
		auto block_pointer = _free_areas[current_order];

		//While the block is pointing NULL or current order is greater than the order to allocate in,
		//the while loop ensures that the blocks are split properly
		while(current_order>order || block_pointer==NULL){
			//In the scenario that current order goes outside the appropriate range, return NULL since
			//we cannot access them.
			//Could happen in the scenario that there are no pages in free_areas (No blocks in any order; hence NULL)
			if (current_order > MAX_ORDER || current_order < 0){
				return NULL;
			}
			auto block = _free_areas[current_order];
			//If a pointer to the first block in current_order exists, split them into a lower order
			if (block != NULL){
				block_pointer = split_block(&_free_areas[current_order], current_order);
				current_order--;
			}
			//If no block is found in the current order, go to the higher order to find blocks to split
			else{
				current_order++;
			}
		}

		//Remove the block allocated in the source order
		remove_block(block_pointer,order);
		return block_pointer;
		}

	
	/**
	 * Frees 2^order contiguous pages.
	 * @param pgd A pointer to an array of page descriptors to be freed.
	 * @param order The power of two number of contiguous pages to free.
	 */
	void free_pages(PageDescriptor *pgd, int order) override
	{
		// Make sure that the incoming page descriptor is correctly aligned
		// for the order on which it is being freed, for example, it is
		// illegal to free page 1 in order-1.
		assert(is_correct_alignment_for_order(pgd, order));
		

		//Insert block into free area
		PageDescriptor **block_inserted = insert_block(pgd, order);
		//Make sure that the page is contained in that block
		assert(is_page_inside_block(*block_inserted,order,pgd));

		//If we are not in the highest order, then we can merge blocks to higher orders
		if (order!=MAX_ORDER){

			int current_order = order;

			//Point to the first block in the order specified
			PageDescriptor *block_pointer = _free_areas[current_order];	
	
			//Find the buddy of the block that was inserted
			PageDescriptor *block_inserted_buddy = buddy_of(*block_inserted, current_order);
	
			//While we traverse orders less than the highest one, and as continuously find the inserted block's buddy in every order to merge and insert into higher block with.
			while(block_pointer  && current_order < MAX_ORDER) {
				
				//If the block we are pointing to is not the block's buddy, we will iterate through the blocks in the current order to find the buddy.	
				//Loop ends if we reach NULL, which happens when we could not find the inserted block's buddy (perhaps it is not free)
				if (block_inserted_buddy != block_pointer) {
					
					//Point to the next block in the current order
					block_pointer = block_pointer->next_free;

					}
				//If we are pointing to the inserted block's buddy
				else {	
					//Merge with buddy in that order and update pointer
					//merge_block function takes care of merging the block with its buddy 
					block_inserted = merge_block(&block_pointer, current_order);
					//Increment to next order
					current_order++;

					//Find the new block's buddy that was merged in the higher order
					block_inserted_buddy = buddy_of(*block_inserted, current_order);
					//Reassign by pointing to the first block in the higher order
					block_pointer = _free_areas[current_order];
				}
			}

		}
		

		
	}
	
	/**
	 * Reserves a specific page, so that it cannot be allocated.
	 * @param pgd The page descriptor of the page to reserve.
	 * @return Returns TRUE if the reservation was successful, FALSE otherwise.
	 */
	bool reserve_page(PageDescriptor *pgd)
	{

		auto current_order = MAX_ORDER;
		//Point to the first block in the current order
		PageDescriptor* block_with_pgd = _free_areas[current_order];

		//First, find where the page descriptor is starting from max_order and by traversing every block in each order.
		//Loop ends once we find it, or if we reach order below 0 (meaning we probably havent found it)
		bool found_pgd = false;
		while(!found_pgd && current_order>=0){
			block_with_pgd = _free_areas[current_order];

			while(!found_pgd && block_with_pgd!=NULL){
				if(is_page_inside_block(block_with_pgd,current_order,pgd)){
					found_pgd = true; //Set the flag to true, which will break us out of the loop
				}
				else{
					block_with_pgd = block_with_pgd -> next_free; //Go to the next block in the current order
				}
			}
			//Traverse in lower orders if the page descriptor has not been found in any of the blocks in the current order
			if (block_with_pgd == NULL){
				current_order--;
			}

		}

		//If the block pointer is still NULL, that means the target page doesnt exist. It could be that it was already reserved
		if (block_with_pgd == NULL){
			return false;
		}
		//Following here, we assert we have found the block containing the page descriptor to be reserved
		assert(is_page_inside_block(block_with_pgd,current_order,pgd));


		//Then, once we have found the page descriptor in one of the blocks, we will start splitting
		while (current_order >= 0){
				//Once we reach 0 order, we should remove the block containing the singular target page descriptor from free_areas
				if (current_order==0 && block_with_pgd){
					auto desired_pgd = &_free_areas[current_order];

					while (*desired_pgd!=pgd){
						//We havent found our page descriptor - return false to indicate reservation has failed
						if (*desired_pgd == NULL){
							return false;
						}
						//Point to the next block in order 0 
						desired_pgd = &(*desired_pgd)->next_free;
					
					}
					//We should have found the block with our target pgd
					assert(*desired_pgd == pgd);

 					//Remove the block in 0th order to indicate that it was reserved
					remove_block(*desired_pgd,current_order);
					return true;
				}


				auto left_block = split_block(&block_with_pgd, current_order);
				auto lower_order = current_order - 1;

				//Find where pgd is in the lower order
				auto block_size = pages_per_block(lower_order);
				PageDescriptor* final_page_in_block = left_block + block_size;
				//If pgd is contained in the left block
				if ((pgd >= left_block) && (pgd < final_page_in_block)){
					//Continue splitting the left block returned by split_block
					block_with_pgd = left_block;
				}
				else{
					//It will be contained with the previously returned block's right-side buddy. 
					auto right_block = buddy_of(left_block,lower_order);
					assert(is_page_inside_block(right_block, lower_order, pgd));
					block_with_pgd = right_block;
				}


				//Assign to the lower order
				current_order = lower_order;
		}

		return false;
	}
	


	/**
	 * Helper Function to see if a page is inside a given block
	 * @param block_pointer A pointer to a block in free_area
	 * @param order The order of where the block is in
	 * @param pgd The page to check
	 * @return Returns True if a block contains the page; Otherwise, returns false
	 */
	bool is_page_inside_block(PageDescriptor* block_pointer, int order, PageDescriptor* pgd) {
		int blocksize = pages_per_block(order);
		PageDescriptor* final_page = block_pointer + blocksize;
		return (pgd >= block_pointer) && (pgd < final_page);
	}


	
	/**
	 * Initialises the allocation algorithm.
	 * @return Returns TRUE if the algorithm was successfully initialised, FALSE otherwise.
	 */
	bool init(PageDescriptor *page_descriptors, uint64_t nr_page_descriptors) override
	{
		
		//Initialise at the highest order
		int highest_order = MAX_ORDER ;

		int number_of_blocks = nr_page_descriptors / pages_per_block(highest_order); //Number of blocks in highest order
		int remainder = nr_page_descriptors %pages_per_block(highest_order); //To check whether the highest order can contain all the pages
		uint64_t pages_added =0; //To keep count of number of pages isnerted
		
		
		int block_index = 0;
		//Iterate through the number of blocks 
		while(block_index < number_of_blocks) {
	
			//Insert the block, by taking the first page descriptor for that block to be the initial page_descriptors incremented by number of pages added so far
			insert_block(page_descriptors+pages_added, highest_order);
			//Update the amount of pages added
			pages_added+= pages_per_block(highest_order);
			//Increment block index
			block_index++;
		}

		//If nr_page_descriptors is not divisible by pages_per_block in the highest order MAX_ORDER, then we will store the remaining pages accordingly in the lower orders
		if (remainder!=0){
			int current_order = highest_order;
			//Loop finishes once pages_added reaches the number of page descriptors to store or order value is now below 0 (an indication of failure to store all values) 
			while(pages_added < nr_page_descriptors && current_order>=0){
				//Check to see how many pages can fit in this order
				int difference = remainder - pages_per_block(current_order);
				//If the difference is positive, that means that we can store the pages in the current order
				if (difference>=0){
					insert_block(page_descriptors+pages_added, current_order);
					pages_added += pages_per_block(current_order);
					//Update the remainder to reflect the amount of pages left
					remainder = difference;
				}
				else{
					current_order--;
				}			

			}

		}
		
		//Return True if the number of inserted pages equals with the amount of page descriptors that we were initially told that we should store
		//Otherwise, returns false
		return pages_added == nr_page_descriptors;


	}

	/**
	 * Returns the friendly name of the allocation algorithm, for debugging and selection purposes.
	 */
	const char* name() const override { return "buddy"; }
	
	/**
	 * Dumps out the current state of the buddy system
	 */
	void dump_state() const override
	{
		// Print out a header, so we can find the output in the logs.
		mm_log.messagef(LogLevel::DEBUG, "BUDDY STATE:");
		
		// Iterate over each free area.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "[%d] ", i);
			
			// Iterate over each block in the free area.
			PageDescriptor *pg = _free_areas[i];
			while (pg) {
				// Append the PFN of the free block to the output buffer.
				snprintf(buffer, sizeof(buffer), "%s%lx ", buffer, sys.mm().pgalloc().pgd_to_pfn(pg));
				pg = pg->next_free;
			}
			
			mm_log.messagef(LogLevel::DEBUG, "%s", buffer);

		}
	}

	
private:
	PageDescriptor *_free_areas[MAX_ORDER+1];
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

/*
 * Allocation algorithm registration framework
 */
RegisterPageAllocator(BuddyPageAllocator);
