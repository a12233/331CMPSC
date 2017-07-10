#ifndef CART_FRAME_CACHE_INCLUDED
#define CART_FRAME_CACHE_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_frame_cache.h
//  Description    : This is the header file for the implementation of the
//                   frame cache for the cartridge memory system driver.
//
//  Author         : Patrick McDaniel
//  Last Modified  : Sun Oct 16 07:59:59 EDT 2016
//

// Includes
#include <cart_controller.h>

// Defines
#define DEFAULT_CART_FRAME_CACHE_SIZE 1024  // Default size for cache

///
struct cacheData{

	int time; 
	int frameNum; 
	int cartNum; 
	char frame[1024]; 
	int full; //if a particular frame is occupied
	//int exist;  

	

}*cache, temp;

struct cacheInfo {
	int size; 
	int gblTime; 
	int free; //any space left in cache
	int writeNum; 
}info; 
// Cache Interfaces

int set_cart_cache_size(uint32_t max_frames);
	// Set the size of the cache (must be called before init)

int init_cart_cache(void);
	// Initialize the cache 

int close_cart_cache(void);
	// Clear all of the contents of the cache, cleanup

int put_cart_cache(CartridgeIndex cart, CartFrameIndex frm, void *frame);
	// Put an object into the object cache, evicting other items as necessary

void * get_cart_cache(CartridgeIndex dsk, CartFrameIndex blk);
	// Get an object from the cache (and return it)
void * delete_cart_cache(CartridgeIndex cart, CartFrameIndex blk);

//
// Unit test

int cartCacheUnitTest(void);
	// Run a UNIT test checking the cache implementation

#endif
