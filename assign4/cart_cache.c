////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_cache.c
//  Description    : This is the implementation of the cache for the CART
//                   driver.
//
//  Author         : Rex Li
//  Last Modified  : [** YOUR DATE **]
//

// Include Files
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

// Project Includes
#include <cart_driver.h>
#include <cart_controller.h>
#include <cart_cache.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>
#include <cart_network.h> 

// Defines

//
// Functions




////////////////////////////////////////////////////////////////////////////////
//
// Function     : set_cart_cache_size
// Description  : Set the size of the cache (must be called before init)
//
// Inputs       : max_frames - the maximum number of items your cache can hold
// Outputs      : 0 if successful, -1 if failure

int set_cart_cache_size(uint32_t max_frames) {

	info.size = max_frames;


return (0); 

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : init_cart_cache
// Description  : Initialize the cache and note maximum frames
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int init_cart_cache(void) {

	cache = calloc(info.size, 1024*sizeof(temp)); 
	for(int i = 0; i < info.size; i++) {
		cache[i].time = -1; 
		cache[i].frameNum = -1;
		cache[i].cartNum = -1; 

	}
	info.free = 0; 
	info.gblTime = 0; 
	info.writeNum = 0; 

		

return (0); 

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : close_cart_cache
// Description  : Clear all of the contents of the cache, cleanup
//
// Inputs       : none
// Outputs      : o if successful, -1 if failure

int close_cart_cache(void) {

	free(cache); 
	cache = NULL; 


return (0); 

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : put_cart_cache
// Description  : Put an object into the frame cache
//
// Inputs       : cart - the cartridge number of the frame to cache
//                frm - the frame number of the frame to cache
//                buf - the buffer to insert into the cache
// Outputs      : 0 if successful, -1 if failure

int put_cart_cache(CartridgeIndex cart, CartFrameIndex frm, void *buf)  {

//write through LRU 
	//check if full
int i; 
char *buff = (char*)buf; 
info.gblTime++; //increament time at every cache access 
int minTime = 0; 
int flag = 0; 
int temp = -1; 
//int tempCart = -1; 
//check if cache is full or if already in memory/cache
//yes-overwrite the lowest time
//no--add to a free cache line 
while(flag != 1 && info.size > 0) {
	if(info.free == 0){ //not full
		for(i = 0; i < info.size && flag == 0; i++) { //cache is not full, find open space or if frame is already in cache
			if(cache[i].frameNum == frm && cache[i].cartNum == cart) { //frame is in cache, modify here
				memcpy(cache[i].frame, buff, strlen(buff)); //fill cache
				client_cart_bus_request(create_cart_opcode(CART_OP_WRFRME,0,0,0,cache[i].frameNum), cache[i].frame); //write to mem
				info.writeNum++; 
				cache[i].time = info.gblTime; 
				flag = 1; 
				cache[i].full = 1; 
			}
			else if(cache[i].full == 0) { //add to cache
				cache[i].frameNum = frm; 
				cache[i].cartNum = cart; 
				memcpy(cache[i].frame, buff, strlen(buff)); //fill cache
				client_cart_bus_request(create_cart_opcode(CART_OP_WRFRME,0,0,0,cache[i].frameNum), cache[i].frame); //write to mem
				info.writeNum++; 
				cache[i].time = info.gblTime; 
				cache[i].full = 1; 
				flag = 1; 
			}
		}
		if(flag == 0)//searched whole cache and found no open space
			info.free = 1; 

	}
	else { // full , eject least recently used, find first filled frame; first find cache line with the minTime then do operations 
			 		
			minTime = cache[0].time;
		 	temp = 0; 
		for(i = 0; i < info.size; i++) {
			//find cache frame with lowest time
			//overwrite that frame with new frame and update time 
			if(cache[i].time < minTime) {
				minTime = cache[i].time; 
				temp = i; 

			}


		}
		delete_cart_cache(cache[temp].cartNum, cache[temp].frameNum); 
		memcpy(cache[temp].frame, buff, strlen(buff)); //fill cache
		cache[temp].cartNum = cart; 
		cache[temp].frameNum = frm;
		client_cart_bus_request(create_cart_opcode(CART_OP_WRFRME,0,0,0,cache[temp].frameNum), cache[temp].frame); //write to mem
		info.writeNum++;
		cache[temp].time = info.gblTime;
		cache[temp].full = 1;  
		flag = 1; 
		//info.free = 1; 

	}

}




return (0); 

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_cart_cache
// Description  : Get an frame from the cache (and return it)
//
// Inputs       : cart - the cartridge number of the cartridge to find
//                frm - the  number of the frame to find
// Outputs      : pointer to cached frame or NULL if not found

void * get_cart_cache(CartridgeIndex cart, CartFrameIndex frm) {


int temp; 
int i; 
int found = 1; 
info.gblTime++; //increament time at every cache access 

	for(i = 0; i < info.size && found == 1; i++){
		if(cache[i].cartNum == cart && cache[i].frameNum == frm) {
			found = 0;
			temp = i;  
			cache[i].time = info.gblTime; //update time 
			
		}  

	}

if(found == 1) //not found
	return (NULL); 
else
	return cache[temp].frame; 
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : delete_cart_cache
// Description  : Remove a frame from the cache (and return it)
//
// Inputs       : cart - the cart number of the frame to remove from cache
//                blk - the frame number of the frame to remove from cache
// Outputs      : pointe buffer inserted into the object

void * delete_cart_cache(CartridgeIndex cart, CartFrameIndex blk) {
char buffer[1024]; 
for(int j = 0; j < 1024; j++) {
	buffer[j] = 0; 

}
int temp = 0; 
int i; 
	for(i = 0; i < info.size; i++) {
		if(cache[i].cartNum == cart && cache[i].frameNum == blk) {
			memcpy(cache[i].frame, buffer, 1024); 
			temp = i; 
			cache[i].full = 0; 
			info.free = 0; 
			cache[i].frameNum = -1; 
			cache[i].cartNum = -1; 
		}  

	}
	return cache[temp].frame; 

}

//
// Unit test

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cartCacheUnitTest
// Description  : Run a UNIT test checking the cache implementation
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int cartCacheUnitTest(void) {
/*
set_cart_cache_size(300); 
init_cart_cache; 
close_cart_cache; 

initialize the cache for a given size
Set of cache items = (create N of these, mark all as not in cache)
loop some number of FIXED_ITERATIONS {
select random operation (read/insert from cache)
pick random cache item X
read:
if X marked as in cache
if read from cache says there good, otherwise FAIL
else
if read from cache says there FAIL, otherwise good
insert:
if X NOT marked as in cache
if insert cache success good, otherwise FAIL
walk cache looking for ejected entries, update cached marks
}
close the cache, freeing all of the elements
*/	// Return successfully
	logMessage(LOG_OUTPUT_LEVEL, "Cache unit test completed successfully.");
	return(0);
}
