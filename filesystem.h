#ifndef FILESYS_H_
#define FILESYS_H_

typedef struct superblock {
    char name[255];
    char flags; /* 0 - read; 1 - write */
    int num_blocks; /* number of blocks in the partition */
    int root_block; /* block number of the root inode */
    int num_free_blocks; /* number of blocks that are free */
    char block_map[756]; /* bit map of used(1)/free(0) blocks */
} superblock;

typedef struct inode {
    char name[255];
    char flags; /* file: 0 - read; 1 - write; dir: 3 - read; 4 - write */
    int file_size; /* number of bytes in the file */
    int direct_refs[190]; /* direct references */
    int indirect_refs; /* indirect references */
} inode;

/* superblock read/write functions */
superblock *read_super();

int write_super(superblock *super);


/* inode read/write functions */
inode *read_inode(int block);

int write_inode(inode *node, int block);


/* data block read/write functions */
void *read_data(int block);

int write_data(char *data, int block);


/* supported syscalls */
void *format(char *name, char flags, int num_blocks);

int dump_to_disk(void *partition, char *text_file);

void *load_from_disk(char *text_file);

int mkdir(char *name, char flags);

int rmdir(char *name);

int copy_file(char *name, char flags, char *local_file);

int remove_file(char *name);

int print_file(char *name);

/* some additional helpful functions */
int *find_inode(char *name);

void print_superblock(superblock *super);

void print_inode(inode *node);


#endif 
