////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the CRUD storage system.
//
//  Author         : Rex Li
//  Last Modified  :10/14/16
//

// Includes
#include <stdlib.h>
#include <string.h>


// Project Includes
#include <cart_driver.h>
#include <cart_controller.h>
#include <stdio.h>
#include <cart_cache.h> 
#include <cart_network.h> 
#define SIZE CART_MAX_TOTAL_FILES

  
// Type definitions
typedef uint64_t CartXferRegister; // This is the value passed through the 
                        	       // controller register
typedef uint16_t  CartridgeIndex;   // Index number of cartridge
typedef uint16_t CartFrameIndex;   // Frame index in current cartridge
typedef char CartFrame[CART_FRAME_SIZE];              // A Frame
typedef CartFrame CartCartridge[CART_CARTRIDGE_SIZE]; // A Cartridge

//prototypes 
uint16_t extract_cart_opcode(CartXferRegister resp, int regNum);  //regNum = 1 == KY1
CartXferRegister create_cart_opcode(uint8_t KY1, uint8_t KY2,  uint8_t RT, uint16_t CT1, uint16_t FM1);

struct fileData { //data structure to keep track of state of file system 

	int open; 
	int exist; 
	char name[CART_MAX_PATH_LENGTH]; 
	int fileDescriptor; 
	int size; 
	int contents[64][1024]; //stores the cartNum and frameNum of all the parts of a given file
	int position; //position within file, 0 to fileData[i].size
	int startFrame; //position of first frame in MS of file
	int startCart; 
	int hitNum; 
	int lastFrame;  
	int lastCart; 
	


}fileArray[SIZE];  

int memoryArray[64][1024]; //0 if open, 1 if partially filled, 2 if full


//create op code
CartXferRegister create_cart_opcode(uint8_t KY1, uint8_t KY2,  uint8_t RT, uint16_t CT1, uint16_t FM1) {

CartXferRegister newOp = 0; 
uint64_t blank = 0; 

newOp = newOp | KY1;
newOp = newOp << 56; 

blank = blank | KY2; 
blank = blank << 48; 
newOp = newOp | blank;

blank = 0; 
blank = blank | RT; 
blank = blank << (47+7); //since RT is defined as 8 bits 
newOp = newOp | blank;

blank = 0; 
blank = blank | CT1; 
blank = blank << 31; 
newOp = newOp | blank;

blank = 0; 
blank = blank | FM1; 
blank = blank << 15; 
newOp = newOp | blank;

return newOp; 
}

//extract op code
uint16_t extract_cart_opcode(CartXferRegister resp, int regNum) { //regNum = 1 == KY1

uint16_t opcode = 0; 
CartXferRegister temp = resp; 


switch(regNum) {
	case 1: 
		temp = temp >> 56; 
		opcode = temp | opcode; 
		break; 
	case 2: 
		temp = temp << 8; 
		temp = temp >> 56; 
		opcode = temp | opcode; 
		break; 
	case 3: 
		temp = temp << 16; 
		temp = temp >> 47; 
		opcode = temp | opcode; 
		break;
	case 4: 
		temp = temp << 17; 
		temp = temp >> 31; 
		opcode = temp | opcode; 
		break;
	case 5: 
		temp = temp << 33; 
		temp = temp >> 15; 
		opcode = temp | opcode; 
		break;

}

return opcode; 

}



//
// Implementation
//write position should be 
//set to the first byte. 
////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweron
// Description  : Startup up the CART interface, initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t cart_poweron(void) {

uint16_t check1, check2, check3 = 0;


check1 = client_cart_bus_request(create_cart_opcode(CART_OP_INITMS, 0,0,0,0), NULL); //init memory system

if(extract_cart_opcode(check1, 3) == -1) //check if init failed 
	return(-1); 


for(int i = 0; i < CART_MAX_CARTRIDGES; i++) {//for all cartridges 

//  1) CART_OP_BZERO - this opcode zeros the _current_ cartridge, as set by the CART_OP_LDCART opcode.  
//You should ignore the CT1 field in the CART_OP_BZERO opcode, instead calling the load cartridge to the one you wish to zero.


	check2 = client_cart_bus_request(create_cart_opcode(CART_OP_LDCART,0,0,i,0), NULL); //load
	if(extract_cart_opcode(check2, 3) == -1) //check if load failed 
		return(-1); 

	check3 = client_cart_bus_request(create_cart_opcode(CART_OP_BZERO,0,0,0,0), NULL); //zero
	if(extract_cart_opcode(check3, 3) == -1) //check if zero failed 
		return(-1); 


}
//setup struct 
for(int i = 0; i < SIZE; i++) {

	fileArray[i].exist = 1; //doesnt exist
	fileArray[i].name[0] = '\0'; //no name
	fileArray[i].open = 1; 
	fileArray[i].fileDescriptor = 2000; 


}
for(int a = 0; a < 64; a++) {
	for(int b = 0; b < 1024; b++){
		memoryArray[a][b] = 0; //zero memoryArray 
	}

}

init_cart_cache(); //init cache





	// Return successfully
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweroff
// Description  : Shut down the CART interface, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t cart_poweroff(void) {

uint16_t check = 0; 

//close all files
for(int i = 0; i < SIZE; i++) {

	fileArray[i].open = 1; 

}

check = client_cart_bus_request(create_cart_opcode(CART_OP_POWOFF,0,0,0,0), NULL);

if(extract_cart_opcode(check, 3) == -1) //check if power off failed 
	return(-1); 

close_cart_cache(); //close cache

	// Return successfully
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure
/*
int16_t cart_open(char *path); - This function will open a file (named path) in the filesystem. If the file does not exist, it should be created and set to zero length. If it does exist, it should be opened and its read/write postion should be set to the first byte. Note that there are no subdirectories in the filesystem, just files (so you can treat the path as a filename). The function should return a unique file handle used for subsequent operations or -1 if a failure occurs.


*/

int16_t cart_open(char *path) {

int i;

int fileDes = 0;  
int created = 1; 
 
//see if file exists
	
for(i = 0; i < SIZE && created == 1; i++) { //search struct array for any element named path


	if(strncmp(fileArray[i].name, path, CART_MAX_PATH_LENGTH) == 0 && fileArray[i].open == 1) {//if names match and not already open

		//set read/write pos to 0
		fileArray[i].position = 0; 
		fileArray[i].open = 0; 
	
		}
	else if(strncmp(fileArray[i].name, path, CART_MAX_PATH_LENGTH) == 0 && fileArray[i].open == 0) //already open
		return(-1); 
		
	else { //if no name match found
		//create file 
		//keep track of new file data, add to end of file array

			if(fileArray[i].exist == 1) { //check if array element is free to use
				fileDes = i; 
				fileArray[i].fileDescriptor = i; 
				strncpy(fileArray[i].name, path, CART_MAX_PATH_LENGTH); 
				fileArray[i].size = 0; 
				fileArray[i].open = 0; 
				fileArray[i].exist = 0; 
				fileArray[i].position = 0; 
				created = 0; 
				fileArray[i].startFrame = 0; 
				fileArray[i].startCart = 0; 
				fileArray[i].hitNum = 0; 
				fileArray[i].lastFrame = 0; 
				fileArray[i].lastCart = 0; 
				
			}//if
	

	}//else
		



}//loop
		

	

	// THIS SHOULD RETURN A FILE HANDLE
	return (fileDes);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure 	
//This function closes the file referenced by the file handle that was previously open. The function should fail (and return -1) if the file handle is bad or the file was not previously open.

int16_t cart_close(int16_t fd) {

if(fileArray[fd].exist == 0) {
	fileArray[fd].open = 1; 
	
}
else {

	return (-1); 
}
	// Return successfully
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_read
// Description  : Reads "count" bytes from the file handle "fh" into the 
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t cart_read(int16_t fd, void *buf, int32_t count) {

char *buff = (char *) buf;
char buffer[1024];
for(int i = 0; i < 1024; i++) {
	buffer[i] = 0; 

}  

int i, h ,k;//loops  



int cartNum = 0;
int frameNum = 0;
int remaining = 0; 
int counter = 0;
int partialFrame = 0;  
int partialPos = 0; 
uint16_t check = 0;
int finished = 1; 
char *cacheBuff; 
int tempCart = -1; 
//char checkBuff[1024]; 


for(i = 0; i < SIZE && finished == 1; i++) { //check file descriptor exists 
	//fail is bad file handle
	if(fileArray[i].fileDescriptor == fd && fileArray[i].open == 1) //file not open 
		return (-1); 
	else if(fileArray[i].fileDescriptor != fd && finished == 1 && i  == SIZE -1) //bad file handle 
		return(-1); 
	//else if((fileArray[i].fileDescriptor == fd && fileArray[i].open == 0) || finished == 1) { //match or unfinished 
	else if(fileArray[i].fileDescriptor == fd && fileArray[i].open == 0) {
		if(fileArray[i].position > fileArray[i].size)
			return (-1); //cannot read past the size of the file 

		cartNum = fileArray[i].startCart;  
		frameNum = fileArray[i].startFrame; //gives frame to read from 
		

				
		partialPos = fileArray[i].position %1024; //end of partially filled frame  
		partialFrame = 1024 - (partialPos); //remaining size in frame 
		
		//read to end of file
		if(fileArray[i].position != fileArray[i].size && (fileArray[i].position + count) > fileArray[i].size) {

				count = fileArray[i].size - fileArray[i].position; 
		
		}
		
		//check cache 
		cacheBuff = get_cart_cache(cartNum, frameNum);
		if(cacheBuff != NULL) { //in cache
			
			memcpy(buffer, cacheBuff, 1024);  

		} 
		
		else { 
			check = client_cart_bus_request(create_cart_opcode(CART_OP_LDCART,0,0,cartNum,0), NULL); //load cartridge, not in cache
			tempCart = cartNum; 
			if(extract_cart_opcode(check, 3) == -1) //check if load failed 
					return(-1); 

		
			//read frame
			check = client_cart_bus_request(create_cart_opcode(CART_OP_RDFRME,0,0,0,frameNum), buffer);
			//cart_client_bus_request(create_cart_opcode(CART_OP_RDFRME,0,0,0,21), checkBuff);
			if(extract_cart_opcode(check, 3) == -1) //check if read failed 
					return(-1); 
	
		}
		if(count <= 1024) {
			memcpy(buff, &buffer[partialPos], count); 

			fileArray[i].position += count;
		
			counter = count; 
			finished = 0; 
		}
		else {
		
			memcpy(buff, &buffer[partialPos], partialFrame);//read until end of frame


			fileArray[i].position += (partialFrame);
			remaining = count-(partialFrame);
			counter = partialFrame; 

		}//one frame finished, remaining bytes to go
		frameNum++; //move to next frame 
		
		//if additional more data is to be read; however, file data might not be in the sequential frame  
		for(h = cartNum; h < CART_MAX_CARTRIDGES  && counter != count && finished == 1; h++) {
		 if(h > fileArray[i].startCart) frameNum = 0; 
			for(k = frameNum; k < CART_CARTRIDGE_SIZE && counter != count && finished == 1; k++) { 

				//make sure to read from the correct file if not in sequential frames
				while(fileArray[i].contents[h][k] == 0) {
					k++;
					if(k >= 1024) {
						k = 0; 
						h++; 
						 
						
												
					}
				}
				frameNum = k;
				cartNum = h; 
				cacheBuff = get_cart_cache(cartNum, frameNum); 
				if(cacheBuff != NULL) { //in cache
			
					memcpy(buffer, cacheBuff, 1024);  

				} 
				else {
					if(cartNum < 64 && tempCart != cartNum) {
						client_cart_bus_request(create_cart_opcode(CART_OP_LDCART,0,0,cartNum,0), NULL);
					}
					client_cart_bus_request(create_cart_opcode(CART_OP_RDFRME,0,0,0,frameNum), buffer); //load next frame 
				}
				//read one frame
				if(remaining / 1024 > 0) {
					memcpy(&buff[counter], buffer, 1024); 
					fileArray[i].position += 1024; 
					counter += 1024; 
					remaining -= 1024;
					if(remaining == 0)
						finished = 0;  

				}
				//read a partial frame 
				else if(remaining / 1024 == 0) {
					memcpy(&buff[counter], buffer, remaining); 
					fileArray[i].position += (remaining % 1024); 
					counter += (remaining % 1024); 
					finished = 0; 
				}
			}//frame

		}//cart		
	
		
 	}//else if
	
	
}//for, check file


	// Return successfully
	return (counter);
}
////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_write
// Description  : Writes "count" bytes to the file handle "fh" from the 
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t cart_write(int16_t fd, void *buf, int32_t count) {

char *buff = (char *) buf; 
char frame[1024];
for(int i = 0; i < 1024; i++) {
	frame[i] = 0; 

}

int i, k , h; //loop indexes


int cartNum = 0; 
int frameNum = 0;


int partialFrame = 0; 
int partialPos = 0;
int counter = 0;  
uint16_t check = 0;
int finished = 1; 
int hit = 1; 
int flag = 0; 
//int flag2 = 0; 

//number of frames to fill, used when accessing an empty frame in memory 


for(i = 0; i < SIZE && finished == 1; i++) {
	if(fileArray[i].fileDescriptor == fd && fileArray[i].open == 1) //file not open 
		return (-1); 
	else if(fileArray[i].fileDescriptor != fd && finished == 1 && i  == SIZE -1) //bad file handle 
		return(-1); 
	else if(fileArray[i].fileDescriptor == fd && fileArray[i].open == 0) {
		
		frameNum = fileArray[i].startFrame; //find initial frame of write
		cartNum = fileArray[i].startCart; 

		for(h = cartNum; h < CART_MAX_CARTRIDGES  && counter != count; h++) {
		 
			for(k = 0; k < CART_CARTRIDGE_SIZE && counter != count; k++) { 
				
				if(fileArray[i].size % 1024 == 0 && counter ==0 && fileArray[i].size > 1024 && fileArray[i].size == fileArray[i].position) { //special case, new write, new frame 

					k = fileArray[i].lastFrame; 
					if(fileArray[i].lastCart > fileArray[i].startCart)
						h = fileArray[i].lastCart; 
					flag = 1; 
				}

				//only write to frames of the same file or empty frames
				while(fileArray[i].contents[h][k] == 0  && memoryArray[h][k] != 0) { //find empty frame to write to: cases: first write, continue write to end of file after fill frame

					k++;

					if(k >= 1024) {
						k = 0; 
						h++;
						  
											
					}
					
				}

				
				while(fileArray[i].position >= hit*1024 && flag == 0 && counter == 0) { //continue writing on file, new write, 
					k++; 
					//flag2 = 1; 
					if(fileArray[i].contents[h][k] > 0)  //hit
						hit++; 


					if(k >= 1024) {
						k = 0; 
						h++;
						 		
					}

				} 

				while(fileArray[i].position > hit*1024 && flag == 0 && counter > 0) { //continue writing on file, continue write, writes to more than one frame
					k++; 
					//flag2 = 1; 
					if(fileArray[i].contents[h][k] > 0)  //hit
						hit++; 


					if(k >= 1024) {
						k = 0; 
						h++;
						  		
					}

				}
				frameNum = k;
				cartNum = h; 
				
				if((frameNum > fileArray[i].lastFrame || cartNum > fileArray[i].lastCart ) && fileArray[i].position == fileArray[i].size)  {
					fileArray[i].lastCart = cartNum; 
					fileArray[i].lastFrame = frameNum;
				} 
				if(cartNum < 64) {
					uint64_t temp = create_cart_opcode(CART_OP_LDCART,0,0,cartNum,0);
					client_cart_bus_request(temp, NULL); //load cart 
				}
				else
					return (-1); //out of bounds 
				if(frameNum >= 1024) 
					return (-1); 	
						//write logic 
						if(fileArray[i].size == 0) {
								if(count <= 1024) {
									memcpy(frame, buff, count);
									if(info.size == 0) {
										client_cart_bus_request(create_cart_opcode(CART_OP_WRFRME,0,0,0,frameNum), frame); 
																			
									}
									put_cart_cache(cartNum,frameNum, frame); //insert into cache
									if(count < 1024) {
										memoryArray[h][k] = 1; 
										fileArray[i].contents[h][k] = 1; 
									}
									else {
										memoryArray[h][k] = 2; 
										fileArray[i].contents[h][k] = 2; 
									}
									fileArray[i].size = fileArray[i].size + count; 
									fileArray[i].position = fileArray[i].size;
									counter = count;  
									finished = 0; 
									fileArray[i].startFrame = frameNum; 
									fileArray[i].startCart = cartNum; 
								}
								else {
					
									memcpy(frame, buff, 1024);
									if(info.size == 0) {
										client_cart_bus_request(create_cart_opcode(CART_OP_WRFRME,0,0,0,frameNum), frame); 
									}
									put_cart_cache(cartNum,frameNum, frame); //insert into cache
									fileArray[i].size = fileArray[i].size + 1024; 
									fileArray[i].position = fileArray[i].size; 
									memoryArray[h][k] = 2; 
									fileArray[i].contents[h][k] = 2; 
									counter += 1024; 
									fileArray[i].startFrame = frameNum; 
									fileArray[i].startCart = cartNum; 

								} 

						}
						else if(fileArray[i].position < fileArray[i].size) {
							//read previous 
							partialPos = fileArray[i].position %1024; //end of partially filled frame  
							partialFrame = 1024 - (partialPos); //remaining size in frame 

							if(counter > 0) {
								for(int i = 0; i < 1024; i++) {
									frame[i] = 0; 

								} //if loop, then zero frame to prepare for write to next frame
							}
							//. On writes, you will need to insert into the cache if it doesn't exist or update if it does exist. 
							if(get_cart_cache(cartNum, frameNum) != NULL) { //in cache
								memcpy(frame, get_cart_cache(cartNum, frameNum), 1024); //load local buffer

							}
							else {
								
								//read frame
								check = client_cart_bus_request(create_cart_opcode(CART_OP_RDFRME,0,0,0,frameNum), frame);//load frame to prepare for overwrite
								if(extract_cart_opcode(check, 3) == -1) //check if read failed 
										return(-1); 
 
							}

							if(partialPos + count-counter <=1024) {
								memcpy(&frame[partialPos], &buff[counter], count-counter); 
								if(info.size == 0) {
										client_cart_bus_request(create_cart_opcode(CART_OP_WRFRME,0,0,0,frameNum), frame);  
								}
								put_cart_cache(cartNum,frameNum, frame); //update cache
								if(fileArray[i].position + count > fileArray[i].size) { 
									memoryArray[h][k] = 1; 
									fileArray[i].contents[h][k] = 1; 
								}
								fileArray[i].position += (count-counter); 
								if(fileArray[i].position > fileArray[i].size)
									fileArray[i].size = fileArray[i].position; 
								counter = count; 
								finished = 0; 

							}
							else {
								memcpy(&frame[partialPos], &buff[counter], partialFrame); //else
								if(info.size == 0) {
										client_cart_bus_request(create_cart_opcode(CART_OP_WRFRME,0,0,0,frameNum), frame);  
								}
								put_cart_cache(cartNum,frameNum, frame); //update cache
								if(fileArray[i].position + count > fileArray[i].size) { 
									memoryArray[h][k] = 1; 
									fileArray[i].contents[h][k] = 1; 
								}
								fileArray[i].position += partialFrame;
								if(fileArray[i].position > fileArray[i].size)
									fileArray[i].size = fileArray[i].position;  
								counter += partialFrame; 


							}

						}
						else if(fileArray[i].position == fileArray[i].size) {
							//read previous 
							partialPos = fileArray[i].position %1024; //end of partially filled frame  
							partialFrame = 1024 - (partialPos); //remaining size in frame +1?

							//move fileArray[i].position to the beginning of the frame 
							//fileArray[i].position -= partialPos; 

							if(counter > 0) {
								for(int i = 0; i < 1024; i++) {
									frame[i] = 0; 

								} //if loop, then zero frame to prepare for write to next frame
							}

							//cart_read(fileArray[i].fileDescriptor, frame, partialPos); //read the partially filled frame 
							//client_cart_bus_request(create_cart_opcode(CART_OP_RDFRME,0,0,0,0), checkFrame);
							client_cart_bus_request(create_cart_opcode(CART_OP_RDFRME,0,0,0,frameNum), frame);
							if(partialPos + count-counter <=1024) {
								memcpy(&frame[partialPos], &buff[counter], count-counter); 
								if(info.size == 0) {
										client_cart_bus_request(create_cart_opcode(CART_OP_WRFRME,0,0,0,frameNum), frame);  
								}
								put_cart_cache(cartNum,frameNum, frame); //update cache
								if( partialPos + count - counter < 1024) {
										memoryArray[h][k] = 1; 
										fileArray[i].contents[h][k] = 1; 
										fileArray[i].lastFrame = frameNum; 
									
								}
								else {
									memoryArray[h][k] = 2; 
									fileArray[i].contents[h][k] = 2; 
									fileArray[i].lastFrame = frameNum + 1; //special case: write after previous write completely filled a frame
									if(fileArray[i].lastFrame >=1024) {
										fileArray[i].lastFrame = 0;
										fileArray[i].lastCart++; 
									}
									
								}
								fileArray[i].size = fileArray[i].size + (count-counter); 
								fileArray[i].position = fileArray[i].size; 
								counter = count; 
								finished = 0; 
							}
							else {
								memcpy(&frame[partialPos], &buff[counter], partialFrame); //fill remaining frame and loop
								put_cart_cache(cartNum,frameNum, frame); //update cache
								if(info.size == 0) {
										client_cart_bus_request(create_cart_opcode(CART_OP_WRFRME,0,0,0,frameNum), frame); 
								}
								fileArray[i].size = fileArray[i].size + partialFrame; 
								fileArray[i].position = fileArray[i].size; 
								memoryArray[h][k] = 2; 
								fileArray[i].contents[h][k] = 2; 
								counter += partialFrame; 



							}


						}//currently if trying to write more to a frame that is filled, not writing and moving to next frame
					


					 
			}//for k, frame
		}//for h, cart 
	}//if
	//else
	//	return(-1); 

}//for i


						
						
	return (count);
}//write

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t cart_seek(int16_t fd, uint32_t loc) {


/*
move file pointer
check file descriptor
move pointer to x offset
if pos is alrger than file _> fail
return position/zero
The function should fail (and return -1) if the loc is beyond the end of the file, the file handle is bad or the file was not previously open.
*/

int i; 
int found = 1; 

for(i = 0; i <SIZE && found == 1; i++) {

	if(fileArray[i].fileDescriptor == fd && fileArray[i].open == 0) {
		fileArray[i].position = loc; 
		found = 0; 

	}
	else if(fileArray[i].fileDescriptor == fd && fileArray[i].open != 0)
		return(-1); 
	else if(fileArray[i].fileDescriptor == fd && loc > fileArray[i].size) 
		return(-1); 
	
	
}
if(found == 1)//bad file handle 
	return(-1); 


	// Return successfully
	return (0);
}





