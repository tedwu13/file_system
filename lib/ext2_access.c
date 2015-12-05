// ext2 definitions from the real driver in the Linux kernel.
#include "ext2fs.h"

// This header allows your project to link against the reference library. If you
// complete the entire project, you should be able to remove this directive and
// still compile your code.
// #include "reference_implementation.h"

// Definitions for ext2cat to compile against.
#include "ext2_access.h"



///////////////////////////////////////////////////////////
//  Accessors for the basic components of ext2.
///////////////////////////////////////////////////////////

// Return a pointer to the primary superblock of a filesystem.
struct ext2_super_block * get_super_block(void * fs) {
    return fs + SUPERBLOCK_OFFSET;
}


// Return the block size for a filesystem.
__u32 get_block_size(void * fs) {
    return EXT2_BLOCK_SIZE(get_super_block(fs));
}


// Return a pointer to a block given its number.
// get_block(fs, 0) == fs;
void * get_block(void * fs, __u32 block_num) {
    return fs + (block_num * get_block_size(fs));
}


// Return a pointer to the first block group descriptor in a filesystem. Real
// ext2 filesystems will have several of these, but, for simplicity, we will
// assume there is only one.
struct ext2_group_desc * get_block_group(void * fs, __u32 block_group_num) {
    return (struct ext2_group_desc *) (fs + SUPERBLOCK_OFFSET + SUPERBLOCK_SIZE);
}


// Return a pointer to an inode given its number. In a real filesystem, this
// would require finding the correct block group, but you may assume it's in the
// first one.
struct ext2_inode * get_inode(void * fs, __u32 inode_num) {
    // get inode table
    struct ext2_group_desc* desc = get_block_group(fs, 0);
    __u32 inode_table_block = desc->bg_inode_table;
    struct ext2_inode* inode_table = get_block(fs, inode_table_block);

    // get inode number
    return inode_table + inode_num - 1;
}



///////////////////////////////////////////////////////////
//  High-level code for accessing filesystem components by path.
///////////////////////////////////////////////////////////

// Chunk a filename into pieces.
// split_path("/a/b/c") will return {"a", "b", "c"}.
//
// This one's a freebie.
char ** split_path(char * path) {
    int num_slashes = 0;
    for (char * slash = path; slash != NULL; slash = strchr(slash + 1, '/')) {
        num_slashes++;
    }

    // Copy out each piece by advancing two pointers (piece_start and slash).
    char ** parts = (char **) calloc(num_slashes, sizeof(char *));
    char * piece_start = path + 1;
    int i = 0;
    for (char * slash = strchr(path + 1, '/');
         slash != NULL;
         slash = strchr(slash + 1, '/')) {
        int part_len = slash - piece_start;
        parts[i] = (char *) calloc(part_len + 1, sizeof(char));
        strncpy(parts[i], piece_start, part_len);
        piece_start = slash + 1;
        i++;
    }
    // Get the last piece.
    parts[i] = (char *) calloc(strlen(piece_start) + 1, sizeof(char));
    strncpy(parts[i], piece_start, strlen(piece_start));
    return parts;
}


// Convenience function to get the inode of the root directory.
struct ext2_inode * get_root_dir(void * fs) {
    return get_inode(fs, EXT2_ROOT_INO);
}


// Given the inode for a directory and a filename, return the inode number of
// that file inside that directory, or 0 if it doesn't exist there.
//
// name should be a single component: "foo.txt", not "/files/foo.txt".
__u32 get_inode_from_dir(void * fs, struct ext2_inode * dir,
        char * name) {
    // inodes per block is in the superblock
    int num_inode = get_super_block(fs)->s_inodes_per_group;

    // entry point to inode table
    struct ext2_dir_entry * entry = (struct ext2_dir_entry*)(get_block(fs, dir->i_block[0]));

    int counter = 0;
    while (counter < num_inode)
    {
        // unused entries
        if (entry->inode == 0)
        {
            continue;
        }

        // this is the inode we want
        if (strlen(name) == (unsigned char)(entry->name_len) && strncmp(name, entry->name, strlen(name)) == 0)
        {
            return entry->inode;
        }

        // otherwise, move on
        entry = (struct ext2_dir_entry*) (((void*) entry) + entry->rec_len);
        counter++;
    }

    // unable to Find
    return 0;

}


// Find the inode number for a file by its full path.
// This is the functionality that ext2cat ultimately needs.
__u32 get_inode_by_path(void * fs, char * path) {
    char** paths = split_path(path);

    // root inode
    __u32 inode_num = EXT2_ROOT_INO;

    // iterate through the subdirectories
    for (char** path = paths; *path != NULL; path++)
    {
        // get inode
        struct ext2_inode* inode = get_inode(fs, inode_num);

        // if not directory, we're at the file we want
        if (!LINUX_S_ISDIR(inode->i_mode))
        {
            break;
        }

        // get inode num from path
        inode_num = get_inode_from_dir(fs, inode, *path);
        if (inode_num == 0)
        {
            break;
        }
    }

    // if inode num doesn't change, then file wasn't found
    if (inode_num == EXT2_ROOT_INO)
    {
        return 0;
    }
    else
    {
        return inode_num;
    }
}
