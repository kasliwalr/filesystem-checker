#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include "fs.h"
#include "fileio.h"

#define FSSIZE 1000
#define NINODES 200
#define NINODEBLOCKS (NINODES/IPB + 1)
#define NBITMAP (FSSIZE/(BSIZE*8) + 1)

#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device

struct indirect_block {
	uint addrs[BSIZE/4];
};

struct superblock rsblock(void* filemap, int offset);
struct dinode rinode(void* inode_start, int inum);
int rbit(void* bitmap_start, int bitnum);
struct dirent rdirent(void* filemap, int block_num, int entry_num);


void pinode(struct dinode inode);
void pblock(void* filemap, int block_num);
void pidirblock(struct indirect_block block);
void chartobinary(char c, char cstr[]);

uint readint(int numval);
short readshort(short numval);

void run_check(int fd);
bool node_alloc_or_valid(struct dinode inode);
bool valid_direct_blocks(struct dinode inode, int min_block_num, int max_block_num);
bool valid_indirect_blocks(struct dinode inode, void * filemap, int min_block_num, int max_block_num);
struct indirect_block r_idirblock(char* block_start);
int dir_valid(struct dinode dir_inode); // todo
bool dir_formatted(struct dinode inode, void* filemap, int inum);
bool valid_inuse_blocks(void* filemap, int nb_bmap, struct dinode inode);
void update_ref_inodes(void* filemap, struct dinode inode, int* inodeRef, bool increment);


int main(int argc, char* argv[]){

	if (argc < 2){
		printf("Usage: xcheck <file_system_image>\n");	
		return 1;	
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd == -1){
		printf("image not found\n");
		return 1;
	}

	// open fs image using mmap
	// int file_sz;
	// void* filemap = (char*) get_memory_map(fd, &file_sz);
	// printf("file sz: %d\n", file_sz);

	// if (filemap == MAP_FAILED){
	// 	printf("map failed\n");
	// 	printf("map failed: %s\n", strerror(errno));	
	// 	close(fd);
	// 	return 1;
	// }

	// char cstr[8];
	// struct superblock sb = rsblock(filemap, 128);
	// struct dinode inode;
	// int offset = sb.inodestart*512;
	// for(int i=0; i<200; i++){
	// 	inode = rinode((void* )((char*) filemap + offset), 1);
	// 	offset += sizeof(struct dinode);
	// 	printf("inode %d\n", i);
	// 	pinode(inode);
	// }

	// close(fd);
	run_check(fd);
	return 0;
}


void run_check(int fd){
	// open fs image using mmap
	int file_sz;
	void* filemap = (char*) get_memory_map(fd, &file_sz);

	if (filemap == MAP_FAILED){
		printf("map failed\n");
		printf("map failed: %s\n", strerror(errno));	
		close(fd);
		exit(1);
	}
	// read super block
	struct superblock sb = rsblock(filemap, 128);
	

	// 1. Each inode is either unallocated or one of the valid types (T_FILE, T_DIR, T_DEV). If not, print ERROR: bad inode.
	struct dinode inode;
	int offset = sb.inodestart*512;
	for(int i=1; i<NINODES; i++){
		inode = rinode((void*)((char*) filemap + offset), i);
		if (!node_alloc_or_valid(inode)){
			printf("ERROR: bad inode\n");
			close(fd);
			exit(1);
		}
	}


	// 2. For in-use inodes, each address that is used by inode is valid (points to a valid datablock address within the image). 
	// If the direct block is used and is invalid, print ERROR: bad direct address in inode.; 
	// if the indirect block is in use and is invalid, print ERROR: bad indirect address in inode.

	for(int i=1; i<NINODES; i++){
		inode = rinode((void*) ((char*) filemap + offset), i);
		if (!valid_direct_blocks(inode, FSSIZE - sb.nblocks, FSSIZE-1)){
			printf("ERROR: bad direct address in inode.\n");
			close(fd);
			exit(1);
		}
		if (!valid_indirect_blocks(inode, filemap, FSSIZE - sb.nblocks, FSSIZE-1)){
			printf("ERROR: bad indirect address in inode.\n");
			close(fd);
			exit(1);
		}
	}


	// 3. Root directory exists, its inode number is 1, and the parent of the root directory is itself. 
	// If not, print ERROR: root directory does not exist.

	inode = rinode((void*)((char*) filemap + offset), 1);
	struct dirent entry;
	entry = rdirent(filemap, inode.addrs[0], 0);
	if (strcmp(entry.name, ".") != 0 || entry.inum != 1){
		printf("ERROR: root directory does not exist.\n");
		close(fd);
		exit(1);
	}

	entry = rdirent(filemap, inode.addrs[0], 1);
	if (strcmp(entry.name, "..") != 0 || entry.inum != 1){
		printf("ERROR: root directory does not exist.\n");
		close(fd);
		exit(1);
	}

	// 4. Each directory contains . and .. entries, and the . entry points to the directory itself. 
	// If not, print ERROR: directory not properly formatted.

	for(int i=1; i<NINODES; i++){
		inode = rinode((void*)((char*) filemap + offset), i);
		if (inode.type != T_DIR)
			continue;
		if (!dir_formatted(inode, filemap, i)){
			printf("ERROR: directory not properly formatted.\n");
			close(fd);
			exit(1);
		}
	}

	// 5. For in-use inodes, each address in use is also marked in use in the bitmap. 
	// If not, print ERROR: address used by inode but marked free in bitmap.

	for(int i=1; i<NINODES; i++){
		inode = rinode((void*)((char*) filemap + offset), i);
		if (inode.type == 0)
			continue;
		if (!valid_inuse_blocks(filemap, sb.bmapstart, inode)){
			printf("ERROR: address used by inode but marked free in bitmap.\n");
			close(fd);
			exit(1);
		}
	}

	// 6. For blocks marked in-use in bitmap, the block should actually be in-use in an inode or indirect block somewhere. 
	// If not, print ERROR: bitmap marks block in use but it is not in use. 

 	int markedInUse[FSSIZE] = {};
	for(int i=1; i<NINODES; i++){
		inode = rinode((void*)(char*) filemap + offset, i);
		for(int j=0; j<=NDIRECT; j++){
			markedInUse[inode.addrs[j]] = 1;
		}
		if (inode.addrs[NDIRECT] == 0)
			continue;
		struct indirect_block block;
		block = r_idirblock((char*) filemap + inode.addrs[NDIRECT]*BSIZE);
		for(int k=0; k<BSIZE/4; k++)
			markedInUse[block.addrs[k]] = 1;
	}

	for(int i=(FSSIZE-sb.nblocks); i<FSSIZE; i++){
		if (rbit((void*)((char*) filemap + sb.bmapstart*BSIZE), i) == 1 && markedInUse[i] == 0){
			printf("ERROR: bitmap marks block in use but it is not in use.\n");
			close(fd);
			exit(1);
		}
	}

	// 7. For in-use inodes, each direct address in use is only used once. 
	// If not, print ERROR: direct address used more than once.

	int inUse[FSSIZE] = {};
	for(int i=1; i<NINODES; i++){
		inode = rinode((void*)(char*) filemap + offset, i);
		if (!node_alloc_or_valid(inode))
			continue;
		for(int j=0; j<NDIRECT; j++){
			if (inode.addrs[j] == 0)
				continue;
			if (inUse[inode.addrs[j]] == 1){ // already marked in use
				printf("ERROR: direct address used more than once.\n");
				close(fd);
				exit(1);
			}
			// printf("%d\n", inode.addrs[j]);
			inUse[inode.addrs[j]] = 1;

		}
		if (inode.addrs[NDIRECT] == 0)
			continue;
		struct indirect_block block;
		block = r_idirblock((char*) filemap + inode.addrs[NDIRECT]*BSIZE);
		for(int k=0; k<BSIZE/4; k++){
			if (block.addrs[k] == 0)
				continue;
			if(inUse[block.addrs[k]] == 1){
				printf("ERROR: direct address used more than once.\n");
				close(fd);
				exit(1);
			}
			inUse[block.addrs[k]] = 1;
		}
	}


	// 8. For in-use inodes, each indirect address in use is only used once. 
	// If not, print ERROR: indirect address used more than once.	
	for(int i=0; i<FSSIZE; i++)
		inUse[i] = 0;
	for(int i=1; i<NINODES; i++){
		inode = rinode((void*)(char*) filemap + offset, i);
		if (!node_alloc_or_valid(inode))
			continue;
		if (inode.addrs[NDIRECT] == 0)
			continue;
		if (inUse[inode.addrs[NDIRECT]] == 1){
			printf("ERROR: indirect address used more than once.\n");
			close(fd);
			exit(1);
		}
		inUse[inode.addrs[NDIRECT]] = 1;
	}

	// 9. For all inodes marked in use, each must be referred to in at least one directory. 
	// If not, print ERROR: inode marked use but not found in a directory.

	int inodeRefs[NINODES] = {}; // all inodes referred to in dirs

	for(int i=1; i<NINODES; i++){
		inode = rinode((void*)((char*)filemap + sb.inodestart*BSIZE), i);
		update_ref_inodes(filemap, inode, inodeRefs, false);
	}

	for(int i=1; i<NINODES; i++){
		inode = rinode((void*)((char*)filemap + sb.inodestart*BSIZE), i);
		if (node_alloc_or_valid(inode))
			continue;
		if (inodeRefs[i] == 0){
			printf("ERROR: inode marked use but not found in a directory.\n");
			close(fd);
			exit(1);
		}
	}

	// 10. For each inode number that is referred to in a valid directory, it is actually marked in use. 
	// If not, print ERROR: inode referred to in directory but marked free.
	for(int i=1; i<NINODES; i++){
		if (inodeRefs[i] == 0)
			continue;
		inode = rinode((void*)((char*)filemap + sb.inodestart*BSIZE), i);
		if (!node_alloc_or_valid(inode)){
			printf("ERROR: inode referred to in directory but marked free.\n");
			close(fd);
			exit(1);
		}
	}

	// 11. Reference counts (number of links) for regular files match the number of times file is referred to in directories 
	// (i.e., hard links work correctly). If not, print ERROR: bad reference count for file.

	int numLinks[NINODES] = {};
	for(int i=1; i<NINODES; i++){
		inode = rinode((void*)((char*)filemap + sb.inodestart*BSIZE), i);
		update_ref_inodes(filemap, inode, numLinks, true);
	}

	for(int i=1; i<NINODES; i++){
		if (numLinks[i] == 0)
			continue;
		inode = rinode((void*)((char*) filemap + sb.inodestart*BSIZE), i);
		if (inode.type == T_FILE){
				if (inode.nlink != numLinks[i]){
					printf("ERROR: bad reference count for file.\n");
					close(fd);
					exit(1);
				}
		} 
		// if (inode.type == T_DIR) {
		// 	if (i == 1 && numLinks[i] == 1 && inode.nlink == 1)
		// 		continue;
		// 	if (numLinks[i] != 2 || inode.nlink != numLinks[i]){
		// 			printf("ERROR: directory appears more than once in file system.\n");
		// 			printf("%d %d %d\n", i, inode.nlink, numLinks[i]);
		// 			close(fd);
		// 			exit(1);
		// 	}
		// } 
	}

	printf("done\n");
	close(fd);
}


void update_ref_inodes(void* filemap, struct dinode inode, int* inodeRefs, bool increment){
	if (inode.type != T_DIR)
		return;

	struct dirent entry;
	int dirent_index;
	for(int i=0; i<NDIRECT; i++){
		dirent_index = 0;
		while((entry = rdirent(filemap, inode.addrs[i], dirent_index++)).inum != 0){
			// printf("%s\n", entry.name);
			if (increment)
				*(inodeRefs + entry.inum) += 1;
			else	
				*(inodeRefs + entry.inum) = 1;
		}
	}

	if (inode.addrs[NDIRECT] == 0)
		return;
	struct indirect_block block;
	block = r_idirblock((char*) filemap + inode.addrs[NDIRECT]*BSIZE);
	for(int i=0; i<BSIZE/4; i++){
		if (block.addrs[i] == 0)
			break;
		dirent_index = 0;
		while((entry = rdirent(filemap, block.addrs[i], dirent_index++)).inum != 0){
			// printf("%s\n", entry.name);
			if (increment)
				*(inodeRefs + entry.inum) += 1;
			else
				*(inodeRefs + entry.inum) = 1;
		}
	}
}





void chartobinary(char c, char cstr[]){
	for(int i=0; i<8;i++){
		cstr[7-i] = (c & 1) == 0? '0': '1';
		c = c>>1;
	}
}

struct dinode rinode(void* inode_start, int inum) {
	short* shortptr = (short *) ((char*) inode_start + sizeof(struct dinode)*inum);
	uint* uintptr;
	struct dinode inode;
	inode.type = readshort(*shortptr++);
	inode.major = readshort(*shortptr++);
	inode.minor = readshort(*shortptr++);
	inode.nlink = readshort(*shortptr++);
	uintptr = (uint*) shortptr;
	inode.size = readint(*uintptr++);
	for(int j=0; j<= NDIRECT; j++)
		inode.addrs[j] = readint(*uintptr++);
	return inode;
}

struct superblock rsblock(void* filemap, int offset){
	struct superblock sb;
	int* pointer = (int*)filemap+offset;
	sb.size = readint(*pointer++);
	sb.nblocks = readint(*pointer++);
	sb.ninodes = readint(*pointer++);
	sb.nlog = readint(*pointer++);
	sb.logstart = readint(*pointer++);
	sb.inodestart = readint(*pointer++);
	sb.bmapstart = readint(*pointer++);
	return sb;
}

int rbit(void* bitmap_start, int blocknum){
	char* pointer = (char*) bitmap_start;
	pointer += blocknum/8;
	char byte = *pointer;
	return (int) ((byte >> (blocknum%8))) & 1;
}


bool valid_inuse_blocks(void* filemap, int nb_bmap, struct dinode inode){
	for(int i=0; i<=NDIRECT; i++){
		// if (inode.addrs[i] != 0)
		// 	printf("%d\n", inode.addrs[i]);
		if (inode.addrs[i] == 0)
			continue;
		if (rbit((void*)((char*) filemap + nb_bmap*BSIZE), inode.addrs[i]) == 0)
			return false;
	}

	if (inode.addrs[NDIRECT] == 0)
		return true;

	struct indirect_block block = r_idirblock((char*) filemap + inode.addrs[NDIRECT]*BSIZE);

	for(int i=0; i<BSIZE/4; i++){
		// if (block.addrs[i] != 0)
		// 	printf("%d\n", block.addrs[i]);
		if (block.addrs[i] == 0)
			continue;
		if (rbit((void*)((char*) filemap + nb_bmap*BSIZE), block.addrs[i]) == 0)
			return false;
	}
	return true;
}

void pinode(struct dinode inode){
	switch(inode.type){
		case T_DIR: 
			printf("type: %s \n", "dir");
			break;
		case T_FILE:
			printf("type: %s\n", "file");
			break;
		case T_DEV:
			printf("type: %s\n", "dev");
			break;
		default:
			printf("type: %s\n", "none");
			break;
	}
	printf("major: %hu \n", inode.major);
	printf("minor: %hu \n", inode.minor);
	printf("nlink: %hu \n", inode.nlink);
	printf("size: %u bytes\n", inode.size);
	printf("data block addresses: ");
	for(int i=0; i<NDIRECT; i++)
		printf("%u|", inode.addrs[i]);
	printf("%u\n", inode.addrs[NDIRECT]);
}

void pidirblock(struct indirect_block block){
	for(int i=0; i<BSIZE/4; i++)
		printf("addr %d: %d\n", i, block.addrs[i]);
}


void pblock(void* filemap, int block_num){
	char* pointer = (char*) filemap + block_num*BSIZE;
	char cstr[8];
	for(int i=0; i<BSIZE; i++){
		chartobinary(*pointer++, (char*) cstr);
		printf("%s ", cstr);
	}
	printf("\n");
}


uint readint(int numval){
	uint num = 0;
	char* numptr = (char*) &num;

	for(int i=0; i<4; i++){
		*numptr++ = numval & 255;	
		numval = numval >> 8;
	}
	return num;
}


short readshort(short numval){
	short num = 0;
	char* numptr = (char*) &num;

	for(int i=0; i<2; i++){
		*numptr++ = numval & 255;
		numval = numval >> 8;
	}
	return num;
}

bool node_alloc_or_valid(struct dinode inode){
	if (inode.type == T_DEV)
		return true;
	if (inode.type == T_DIR)
		return true;
	if (inode.type == T_FILE)
		return true;
	if (inode.type == 0)
		return true;
	return false;
}

bool valid_direct_blocks(struct dinode inode, int min_block_num, int max_block_num){
	for(int i=0; i<NDIRECT; i++){
		if (inode.addrs[i] == 0)
			continue;
		if (inode.addrs[i] < min_block_num  || inode.addrs[i] >= max_block_num)
			return false;
	}
	return true;
}


bool valid_indirect_blocks(struct dinode inode, void * filemap, int min_block_num, int max_block_num) {
	if (inode.addrs[NDIRECT] == 0)
		return true;
	struct indirect_block block;
	char* pointer = (char*) filemap + inode.addrs[NDIRECT]*BSIZE;
	block = r_idirblock(pointer);
	for(int i=0; i<BSIZE/4; i++){
		if (block.addrs[i] == 0)
			continue;
		if (block.addrs[i] < min_block_num || block.addrs[i] >= max_block_num)
			return false;
	}
	return true;
}

bool dir_formatted(struct dinode inode, void* filemap, int inum){
	struct dirent entry;
	
	entry = rdirent(filemap, inode.addrs[0], 0);
	if (strcmp(entry.name, ".") != 0 || entry.inum != inum)
		return false;

	entry = rdirent(filemap, inode.addrs[0], 1);
	if (strcmp(entry.name, "..") != 0)
		return false;

	return true;
}

struct indirect_block r_idirblock(char* block_start){
	int* pointer = (int*) block_start;	
	struct indirect_block block;
	for(int i=0; i<BSIZE/4; i++)
		block.addrs[i] = readint(*pointer++);
	return block;

}


struct dirent rdirent(void* filemap, int block_num, int entry_num) {

	char* start = (char*) filemap + block_num*BSIZE + entry_num*(DIRSIZ + sizeof(ushort));
	char* namepointer;
	ushort* inodenumptr;
	
	inodenumptr = (ushort*) start;
	struct dirent dir_entry;
	dir_entry.inum = readshort(*inodenumptr++);
	namepointer  = (char*) inodenumptr;
	for(int i=0; i<DIRSIZ; i++)
		dir_entry.name[i] = *namepointer++;

	return dir_entry;
}



// check list

// 1. Each inode is either unallocated or one of the valid types (T_FILE, T_DIR, T_DEV). If not, print ERROR: bad inode.
//
// iterate over all inodes, check type. check allocation, last block is meant for indirect address


// 2. For in-use inodes, each address that is used by inode is valid
//
// check block validity. block is within range, and is indicated to be in use in bitmap

// 3. Root directory exists, its inode number is 1, and the parent of the root directory is itself.
//
// read inode 1, nav to data block, read dirent and confirm inode numbers of ., .. entries


// 4. Each directory contains . and .. entries, and the . entry points to the directory itself. 
//
// interate over all inodes, for dirs, nav to data block, read dirent, and confirm inode numbers of ., .. entries


// 5. For in-use inodes, each address in use is also marked in use in the bitmap
//
// interate over all inodes, for each inode iterate over all data block address, if non zero, check bitmap entry for specific block number
// if there is indirect block, read that block and iterate over it and check for each address entry


// 6. For blocks marked in-use in bitmap, the block should actually be in-use in an inode or indirect block somewhere.
//
// build a hash table structure, where one can quickly check if a data block is in use. worst case it will take up NBLOCKS*sizeof(char) bytes
// then iterate over bits in bitmap, and check against this data structure


// 7. For in-use inodes, each direct address in use is only used once.
//
// same as 6, except that when one can check this when building the data structure


// 8. For in-use inodes, each indirect address in use is only used once
//
// same as 7


// 9. For all inodes marked in use, each must be referred to in at least one directory.
//
// build a data structure, that indicates all inodes referred to in a dir
// then iteratre over all inodes, check if in use. if so, then check presence in data structure

// 10. For each inode number that is referred to in a valid directory, it is actually marked in use. 
//
// build a data structure, that indicates all inodes referred to in a valid dir
// check iteratre over the data structure, read in inode and verify that it is in use

// 11. Reference counts (number of links) for regular files match the number of times file is referred to in directories (i.e., hard links work correctly)
//
// create a data structure, that references inode num against the number of references
// iterate over all inode, if file inode, check num of links against those store in data structure


// 12. No extra links allowed for directories  (each directory only appears in one other directory).
//
// iterate over all inodes, if a dir inode, verify that num links is exactly 2



