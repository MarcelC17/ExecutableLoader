/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "exec_parser.h"

static so_exec_t *exec;
//file descriptor
static int fd;
//stores address of maped pages
static uintptr_t ISMAPPED[1000];
//counts maped addresses
static int mappedAddrCounter = 0;

//checks if the ellement is mapped
int findAddr(unsigned long int* mappedAddrArray, unsigned long int elemToFind){
	//looks for page address of current error in the array of mapped pages
	for (int iter = 0; iter <= mappedAddrCounter; iter++)
	{
		if (mappedAddrArray[iter] == elemToFind)
		return 1; 
	}
	return 0;
}



static void segv_handler(int sig, siginfo_t *info, void *degeaba)
{
	//page size
	uintptr_t pgsize = 4096;
	//to store address of address array 
	unsigned long int* mappedAddrArray =NULL; 
	//to store error segment
	so_seg_t* segment = NULL;
	//segment offset in pages
	uintptr_t page_offset = 0;
	//segment offset in addresses
	uintptr_t addr_offset = 0;
	//address of error page
	uintptr_t page_addr = 0;



	//finds segment containing error
	for (int segIterator = 0; segIterator < exec->segments_no; segIterator++){
		//checks if error address belongs to segment
		if ((unsigned long int)info->si_addr >= exec->segments[segIterator].vaddr && (unsigned long int)info->si_addr < exec->segments[segIterator].vaddr + exec->segments[segIterator].mem_size) 
		{
			//stores error segment
			segment = &exec->segments[segIterator];
			break;
		}
	}


	// finds offsets and error page
	page_offset = ((unsigned long int)info->si_addr - (unsigned long int)segment->vaddr) / pgsize;

	addr_offset = page_offset * pgsize;

	page_addr = segment->vaddr + addr_offset;

	// data field set to get information about mapped addresses
	segment->data = &ISMAPPED;

	//checks if error segment was found
	if (segment != NULL) { 
		//checks if the page was previously mapped
		if (!findAddr(segment->data, segment->vaddr+addr_offset))
		{
			//if not mapps the page
			mmap((void *)page_addr, pgsize, PERM_R | PERM_W, MAP_FIXED | MAP_PRIVATE, fd, segment->offset + addr_offset);
			//the whole page is in the file
			if (pgsize + addr_offset <= segment->file_size) { 
				//loads the page and sets protection
				lseek(fd, segment->offset + addr_offset, SEEK_SET);
				read(fd,(void *)page_addr, pgsize);
				mprotect((void *)page_addr, pgsize, segment->perm);
			} 
			//if page is partly in the file
			else if (addr_offset <= segment->file_size && segment->file_size < addr_offset + pgsize) { 
				//loads the page and sets protection
				lseek(fd,segment->offset + addr_offset,SEEK_SET);
				read(fd,(void *)addr_offset + segment->vaddr, segment->file_size - addr_offset);
				//sets addresses which dont fit in file to 0
				memset((void *)segment->vaddr + segment->file_size, 0, pgsize-(segment->file_size - addr_offset));
				mprotect((void *)page_addr, pgsize, segment->perm);
			} 
			//if page is not in the file
			else {
				//sets page addresses to 0
				memset((void *)page_addr, 0, pgsize);
				mprotect((void *)page_addr,pgsize, segment->perm);
			}
			//adds page to array
			mappedAddrArray = segment->data;
			mappedAddrArray[mappedAddrCounter] = page_addr;
			mappedAddrCounter++;
		} else exit(139);//page already mapped
	} else exit(139); //segment was not found
}


int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, NULL);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
	fd = open(path, O_RDONLY);
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	

	so_start_exec(exec, argv);

	return -1;
}