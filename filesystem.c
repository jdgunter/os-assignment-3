#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "filesystem.h"

#define SUCCESS 0
#define FAILURE 1

#define BLOCK_SIZE 1024

#define ROOT_DIR "/"

#define READ 0
#define WRITE 1
#define FILE_READ 0
#define FILE_WRITE 1
#define DIR_READ 3
#define DIR_WRITE 4

// global variables for the file system partition and superblock
char **fs_partition; // array of pointers to char[1024]
superblock *fs_superblock;

void set_block(int block)
{
    int index = block >> 3;
    int bitmask = (1 << 7) >> (block & 0x7);
    // check if bit is not set so we can decrement num_free_blocks
    if ( !(fs_superblock->block_map[index] & bitmask) ) {
	printf("map %x\n", fs_superblock->block_map[index]);
        fs_superblock->block_map[index] |= bitmask;
        printf("mask %x\n", bitmask);
        fs_superblock->num_free_blocks--;
    }
}

void clear_block(int block)
{
    int index = block >> 3;
    int bitmask = (1 << 7) >> (block & 0x7);
    // check if bit is set so we know whether to increment num_free_blocks
    if (fs_superblock->block_map[index] & bitmask) {
        fs_superblock->block_map[index] &= ~bitmask;
        fs_superblock->num_free_blocks++;
    }
}

/*
 * Finds the first block set to be `free` by the superblock
 */
int first_free_block()
{
    if (fs_superblock->num_free_blocks == 0) {
        return -1;
    }
    int i, j;
    for (i = 0; i < fs_superblock->num_blocks/8; ++i) {
        if (fs_superblock->block_map[i] == 0xFF) {
            continue;
        }
        for (j = 0; j < 8; ++j) {
            if (~fs_superblock->block_map[i] & ((1 << 7) >> j)) {
		printf("Block %d is first free\n", 8*i + j);
                return 8*i + j;
            }
        }
    }
    return -1;
}

/*
 * Read a superblock from the partition
 */
superblock *read_super()
{
    return (superblock*) fs_partition[0];
}

/*
 * Write a superblock to the partition
 */
int write_super(superblock *super)
{
    memcpy(fs_partition[0], super, BLOCK_SIZE);
    return SUCCESS;
}

/*
 * Read an inode at a given block from the partition
 */
inode *read_inode(int block)
{
    return (inode*) fs_partition[block];
}

/*
 * Write an inode to a given block in the partition
 */
int write_inode(inode *node, int block)
{
    char *block_ptr = fs_partition[block];
    memcpy(block_ptr, node, BLOCK_SIZE);
    return SUCCESS;
}

/*
 * Reads a block of raw data from the partition
 */
void *read_data(int block)
{
    return (void *) (fs_partition[block]);
}

/*
 * Writes a 1KB block of raw data into the partition
 */
int write_data(char *data, int block)
{
    char *block_ptr = fs_partition[block];
    memcpy(block_ptr, data, BLOCK_SIZE);
    return SUCCESS;
}

/*
 * Creates a file partition with root directory "name" of size "num_blocks"
 */
void *format(char *name, char flags, int num_blocks)
{
    /* assert that num_blocks is within the valid range */
    assert(num_blocks >= 32);
    assert(num_blocks <= 6048);

    /* initializing the superblock */
    fs_partition = malloc(num_blocks * sizeof(char**));
    int i;
    for (i = 0; i < num_blocks; ++i) {
	fs_partition[i] = malloc(BLOCK_SIZE);
    }

    fs_superblock = read_super(); // the superblock takes up the first block,
    strcpy(fs_superblock->name, name);
    fs_superblock->num_blocks = num_blocks;
    fs_superblock->flags = WRITE;
    fs_superblock->root_block = 1;
    fs_superblock->num_free_blocks = num_blocks - 2;
    memset(fs_superblock->block_map, 0, 756);
    fs_superblock->block_map[0] = 0xc0; // mask first two bits

    /* initializing the root directory */
    inode *root = read_inode(1);
    strcpy(root->name, ROOT_DIR);
    root->flags = DIR_WRITE;
    memset(root->direct_refs, 0, sizeof(int)*190);
    root->file_size = 0;
    root->indirect_refs = 0;
    
    return fs_partition;
}

/*
 * writes partition to a text file, character by character as to avoid endianness issues
 */
int dump_to_disk(void *partition, char *text_file)
{
    FILE *f = fopen(text_file, "wb+");
    superblock *super = (superblock*)partition;
    char *outchar = (char*)partition; // cast to a character so we can actually write out the data
    int i;
    for (i = 0; i < super->num_blocks; ++i)
    {
        int j;
        for(j = 0; j < BLOCK_SIZE; ++j)
        {
            if (fputc(*outchar, f) == EOF) { return FAILURE; }
            outchar++;
        }
    }
    fclose(f);
    return SUCCESS;
}

/*
 * read/load the partition from a file on disk, 1 character at a time
 */
void *load_from_disk(char *text_file)
{
    // first create a superblock, reading the first block in to get information on
    // the size of the partition
    superblock *super = malloc(sizeof(super));
    char *inchar = (char*)super; // cast to character pointer so we can write to it
    FILE *f = fopen(text_file, "rb");
    if (!f) { return NULL; } // check for error opening file
    int i;
    // read the first block (superblock)
    for (i = 0; i < BLOCK_SIZE; ++i)
    {
        *inchar = fgetc(f);
        if (feof(f)) { return NULL; } // make sure we haven't reached the end of the file
        inchar++;
    }
    char *partition = malloc(1024 * super->num_blocks);
    memcpy(partition, super, BLOCK_SIZE);
    inchar = partition + BLOCK_SIZE; // reset position of character pointer
    // read the rest of the blocks
    for (i = 0; i < (super->num_blocks - 1); ++i)
    {
        int j;
        for (j = 0; j < BLOCK_SIZE; ++j)
        {
            *inchar = fgetc(f);
            if (feof(f)) { return NULL; }
            inchar++;
        }
    }
    fclose(f);
    return (void*)partition;
}

/*
 * Count how often the char `c` appears in the string `s`
 */
int char_count(char *s, char c)
{
    int i, count = 0;
    int length = strlen(s);
    for (i = 0; i <= length; ++i) {
        if (s[i] == c) {
            ++count;
        }
    }
    return count;
}

int *get_subdirs(inode *dir)
{
    return (int*)read_data(dir->indirect_refs);
}

/*
 * Gets the inode that is (or would be) the parent of `name`
 * returns NULL if a directory on the path does not have write privileges
 * (if writeable set) or is a file
 */
inode *find_parent_inode(char *name, int writeable)
{
    char *namecpy = malloc(strlen(name) + 1);
    strcpy(namecpy, name);
    int depth = char_count(name, '/');
    
    inode *current_dir = read_inode(1); // get the root inode
    char current_name[255] = "/";
    
    int i, j = 1;
    char *next_dir = strtok(namecpy, "/");
    while (j < depth) {
        strcat(current_name, "/");
        strcat(current_name, next_dir);
        
        int *subdirs = get_subdirs(current_dir);
        for (i = 0; i < current_dir->file_size; ++i) 
        {
            inode *node = read_inode(subdirs[i]);
            if (strcmp(current_name, node->name) == 0) 
            {
                current_dir = node;
                next_dir = strtok(NULL, "/");
                ++j;
                break;
            }
            // if no directory found, return NULL
            return NULL;
        }
        if ((current_dir->flags == FILE_READ) ||
            (current_dir->flags == FILE_WRITE)) 
        {
            return NULL;
        }
        if (writeable && (current_dir->flags != DIR_WRITE)) {
            return NULL;
        }
    }
    if ((current_dir->flags == FILE_READ) ||
        (current_dir->flags == FILE_WRITE)) 
    {
        return NULL;
    }
    if (writeable && (current_dir->flags != DIR_WRITE)) {
        return NULL;
    }
    return current_dir;
}

/*
inode *find_inode(char *name)
{
    
}
*/

int mkdir(char *name, char flags)
{
    inode *parent = find_parent_inode(name, 1);
    if (!parent) {
        return FAILURE;
    }

    // find a block to put the inode for the new dir in
    int block = first_free_block();
    set_block(block);
    printf("block: %d\n", block);

    inode *new_node = malloc(sizeof(new_node));
    strcpy(new_node->name, name);
    new_node->flags = flags;
    new_node->file_size = 0;
    memset(new_node->direct_refs, 0, 190*sizeof(int));
    new_node->indirect_refs = first_free_block();
    set_block(new_node->indirect_refs);

    int *subdirs = get_subdirs(parent);
    int i;
    while (subdirs[i] && (i < 256)) ++i;
    if (i == 256) { // no space left
        free(new_node);
        return FAILURE;
    } else {
        subdirs[i] = block;
        return SUCCESS;
    }
}

int rmdir(char *name)
{
    inode *parent = find_parent_inode(name, 0);
    
}

void print_superblock(superblock *super)
{
    printf("\n%s\n", super->name);
    if (super->flags) {
        printf("read/write\n");
    } else {
        printf("read only\n");
    }
    printf("Numblocks: %d\n", super->num_blocks);
    printf("Root block: %d\n", super->root_block);
    printf("Free blocks: %d\n", super->num_free_blocks);
    int i;
    for (i = 0; i < super->num_blocks/8; ++i)
    {
        printf("%x", super->block_map[i]);
        if ((i+1) % 16 == 0) { printf("\n"); }
    }
    printf("\n");
}

void print_inode(inode *node)
{
    printf("\n%s\n", node->name);
    switch (node->flags) {
    case FILE_READ:
        printf("File, read only\n");
        break;
    case FILE_WRITE:
        printf("File, read/write\n");
        break;
    case DIR_READ:
        printf("Directory, read only\n");
        break;
    case DIR_WRITE:
        printf("Directory, read/write\n");
        break;
    }
    printf("File size: %d\n", node->file_size);
    printf("Indirect refs: %d\n", node->indirect_refs);
}

// TODO: implement rmdir, copy_file, remove_file, print_file
// finish mkdir so it works properly
// currently struggling with the bitset code - once that gets ironed out
// rest should be fairly straightforward
//
// will submit another version once more progress made
//


// testing main
int main(void) 
{ 
    format("Test Superblock", 1, 32);
    print_superblock(fs_superblock);
    if (mkdir("/test", DIR_WRITE) == SUCCESS) {
        print_superblock(fs_superblock);
        print_inode(read_inode(3));
    } else {
        printf("Failed to make directory\n");
    }

    return 0;
}
