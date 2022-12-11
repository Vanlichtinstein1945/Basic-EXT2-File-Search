#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <ext2fs/ext2_fs.h>

//The size of our ext2 blocks (normally 1024 bytes)
unsigned int block_size = 0;

void traverse(int, const struct ext2_group_desc*, struct ext2_inode*, char*);
void find_file(int, const struct ext2_group_desc*, struct ext2_inode*, char*, char*);

//Function to grab an inode based on its number (2 is root directory)
void grab_inode(int fd, int inode_num, const struct ext2_group_desc *group_desc, struct ext2_inode *inode) {
	lseek(fd, (group_desc->bg_inode_table*block_size)+((inode_num-1)*sizeof(struct ext2_inode)), SEEK_SET);
	read(fd, inode, sizeof(struct ext2_inode));
}

//Function to read each entry in a directory block
void read_entries(int fd, void *block, const struct ext2_group_desc *group_desc, struct ext2_inode *inode, char *path) {
	struct ext2_dir_entry_2 *entry;
	unsigned int size = 0;
	
	entry = (struct ext2_dir_entry_2 *)block;
	//While looping to get all directories and files listed in this block
	while ((size < inode->i_size) && entry->inode) {
		//Grabbing current directory/file's name
		char file_name[EXT2_NAME_LEN+1];
		memcpy(file_name, entry->name, entry->name_len);
		file_name[entry->name_len] = 0;
		//Testing if current entry is a file
		if (entry->file_type == 1) {
			char *newPath = malloc(strlen(path) + strlen(file_name) + 2);
			strcpy(newPath, path);
			strcat(newPath, "/");
			strcat(newPath, file_name);
			//Printing file path
			printf("%s\n", newPath);
			free(newPath);
		//Testing if current entry is a directory
		} else if (entry->file_type == 2) {
			//Ensuring the current directory doesn't include a '.'
			if (strchr(file_name, '.') == NULL) {
				char *newPath = malloc(strlen(path) + strlen(file_name) + 2);
				strcpy(newPath, path);
				strcat(newPath, "/");
				strcat(newPath, file_name);
				//Grabbing the new inode and traversing it for files/directories
				struct ext2_inode newInode;
				grab_inode(fd, entry->inode, group_desc, &newInode);
				traverse(fd, group_desc, &newInode, newPath);
				free(newPath);
			}
		}
		//Incrementing to the next entry in this directory
		if (entry->rec_len != 0) {
			entry = (void*) entry + entry->rec_len;
			size += entry->rec_len;
		} else {
			size = inode->i_size;
		}
	}
}

//Function to start in the given inode and traverse downwards printing all files and directories
void traverse(int fd, const struct ext2_group_desc *group_desc, struct ext2_inode *inode, char *path) {
	printf("%s\n", path);
	//Testing if the current inode is a directory
	if (S_ISDIR(inode->i_mode)) {
		//Looping for all block pointers in directory except last garbage pointer
		for (int i=0; i<inode->i_blocks; i++) {
			//Ensuring block pointer is not empty
			if (inode->i_block[i] != 0) {
				void *block = malloc(block_size);
				//Read if block is direct pointer
				if (i < EXT2_NDIR_BLOCKS) {
					lseek(fd, block_size*inode->i_block[i], SEEK_SET);
					read(fd, block, block_size);
					read_entries(fd, block, group_desc, inode, path);
				//Read if block is single indirect pointer
				} else if (i == EXT2_IND_BLOCK) {
					lseek(fd, block_size*inode->i_block[i], SEEK_SET);
					for (int j=0; j<block_size; j++) {
						unsigned int block_num = 0;
						unsigned int *block_num_ptr = &block_num;
						read(fd, block_num_ptr, 1);
						lseek(fd, block_size*block_num, SEEK_SET);
						read(fd, block, block_size);
						read_entries(fd, block, group_desc, inode, path);
						lseek(fd, block_size*inode->i_block[i]+j, SEEK_SET);
					}
				}

				free(block);
			}
		}
	}
}

//Function to read the data blocks from a file's inode
void read_file(int fd, struct ext2_inode *inode) {
	char *block = malloc(block_size);
	for (int i=0; i<inode->i_blocks-1; i++) {
		if (inode->i_block[i] != 0) {
			if (i < EXT2_NDIR_BLOCKS) {
					lseek(fd, block_size*inode->i_block[i], SEEK_SET);
					read(fd, block, block_size);
					printf("%s", block);
			} else if (i == EXT2_IND_BLOCK) {
				lseek(fd, block_size*inode->i_block[i], SEEK_SET);
				for (int j=sizeof(int); j<=block_size; j+=sizeof(int)) {
					unsigned int block_num;
					read(fd, &block_num, sizeof(int));
					if (block_num != 0) {
						lseek(fd, block_size*block_num, SEEK_SET);
						read(fd, block, block_size);
						printf("%s", block);
					}
					lseek(fd, block_size*inode->i_block[i]+j, SEEK_SET);
				}
			}
		}
	}
}

void find_file_entries(int fd, void *block, const struct ext2_group_desc *group_desc, struct ext2_inode *inode, char *path, char *finalPath) {
	struct ext2_dir_entry_2 *entry;
	unsigned int size = 0;
	
	entry = (struct ext2_dir_entry_2 *)block;
	//While looping to get all directories and files listed in this block
	while ((size < inode->i_size) && entry->inode) {
		//Grabbing current directory/file's name
		char file_name[EXT2_NAME_LEN+1];
		memcpy(file_name, entry->name, entry->name_len);
		file_name[entry->name_len] = 0;
		//Testing if current entry is a file
		if (entry->file_type == 1) {
			char *newPath = malloc(strlen(path) + strlen(file_name) + 2);
			strcpy(newPath, path);
			strcat(newPath, "/");
			strcat(newPath, file_name);
			if (strcmp(newPath, finalPath) == 0) {
				grab_inode(fd, entry->inode, group_desc, inode);
				read_file(fd, inode);
				break;
			}
			free(newPath);
		//Testing if current entry is a directory
		} else if (entry->file_type == 2) {
			//Ensuring the current directory doesn't include a '.'
			if (strchr(file_name, '.') == NULL) {
				char *newPath = malloc(strlen(path) + strlen(file_name) + 2);
				strcpy(newPath, path);
				strcat(newPath, "/");
				strcat(newPath, file_name);
				//Grabbing the new inode and traversing it for files/directories
				struct ext2_inode newInode;
				grab_inode(fd, entry->inode, group_desc, &newInode);
				find_file(fd, group_desc, &newInode, newPath, finalPath);
				free(newPath);
			}
		}
		if (entry->rec_len != 0) {
			//Incrementing to the next entry in this directory
			entry = (void*) entry + entry->rec_len;
			size += entry->rec_len;
		} else {
			size = inode->i_size;
		}
	}
}

//Function to find a file's inode based on its absolute path
void find_file(int fd, const struct ext2_group_desc *group_desc, struct ext2_inode *inode, char *path, char *finalPath) {
	//Testing if the current inode is a directory
	if (S_ISDIR(inode->i_mode)) {
		void *block = malloc(block_size);
		
		for (int i=0; i<inode->i_blocks; i++) {
			if (inode->i_block[i] != 0) {
				if (i < EXT2_NDIR_BLOCKS) {
					lseek(fd, block_size*inode->i_block[i], SEEK_SET);
					read(fd, block, block_size);
					find_file_entries(fd, block, group_desc, inode, path, finalPath);
				}
			}
		}

		free(block);
	}
}

//Main function
int main(int argc, char *argv[])
{
	struct ext2_super_block super_block;
	struct ext2_group_desc group_desc;
	struct ext2_inode inode;
	int fd;

	//Opening given ext2 .img file
	fd = open(argv[1], O_RDONLY);

	//Skipping the boot block to read the superblock
	lseek(fd, 1024, SEEK_SET);
	read(fd, &super_block, sizeof(super_block));
	
	//Using superblock to determine size of blocks
	block_size = 1024 << super_block.s_log_block_size;

	//Skipping to read the group descriptor
	lseek(fd, block_size, SEEK_SET);
	read(fd, &group_desc, sizeof(group_desc));
	
	//Calling function to read the root directory's inode
	grab_inode(fd, 2, &group_desc, &inode);
	
	//Testing if we're -traversing or -printing
	if (argc == 3) {
		printf("/");
		traverse(fd, &group_desc, &inode, "");
	} else if (argc == 4) {
		find_file(fd, &group_desc, &inode, "", argv[3]);
	}

	close(fd);
	return 0;
}
