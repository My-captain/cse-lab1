#include "inode_manager.h"
#include "math.h"
#include "cstring"
#include "time.h"
#include "map"

// disk layer -----------------------------------------

disk::disk() {
    bzero(blocks, sizeof(blocks));
}

void disk::read_block(blockid_t id, char *buf) {
    if (id >= BLOCK_NUM || buf == NULL)
        return;
    // TODO 是否要考虑结束位置？
    memcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const char *buf) {
    if (id >= BLOCK_NUM || buf == NULL)
        return;
    memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t block_manager::alloc_block() {
    /*
     * your code goes here.
     * note: you should mark the corresponding bit in block bitmap when alloc.
     * you need to think about which block you can start to be allocated.
     */
    for (uint32_t i = BLOCK_NUM - 1; i >= 0; --i) {
        if (using_blocks.find(i) == using_blocks.end()) {
            using_blocks.insert(std::pair<uint32_t, int>(i, 1));
            --rest_block;
            return i;
        }
    }

    // 均已分配
    return 0;
}

void block_manager::free_block(uint32_t id) {
    /*
     * your code goes here.
     * note: you should unmark the corresponding bit in the block bitmap when free.
     */
    if (using_blocks.find(id) == using_blocks.end()) {
        return;
    } else {
        using_blocks.erase(id);
        ++rest_block;
        return;
    }
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager() {
    d = new disk();
    sb.size = BLOCK_SIZE * BLOCK_NUM;
    sb.nblocks = BLOCK_NUM;
    sb.ninodes = INODE_NUM;

    // boot sector（占一个）
    using_blocks[0] = 1;

    // SuperBlock所占用的block
    uint32_t sb_block = (uint32_t) ceil((double) sizeof(sb) / BLOCK_SIZE);
    for (uint32_t block_num = 1; block_num < sb_block + 1; ++block_num) {
        using_blocks[block_num] = 1;
    }
    // Bitmap所占用的block
    uint32_t bitmap_block = (uint32_t) ceil((double) BLOCK_NUM / (double) (BLOCK_SIZE * 8));
    for (uint32_t block_num = sb_block + 1; block_num < sb_block + bitmap_block + 1; ++block_num) {
        using_blocks[block_num] = 1;
    }
    // inodeTable所占用的block
    uint32_t inodeTable_block = (uint32_t) ceil((double) INODE_NUM * sizeof(inode_t) / (double) BLOCK_SIZE);
    for (uint32_t block_num = sb_block + bitmap_block + 1;
         block_num < sb_block + bitmap_block + inodeTable_block + 1; ++block_num) {
        using_blocks[block_num] = 1;
    }

    // |<-boot sector->|<-sb->|<-free block bitmap->|<-inode table->|<-data->|
    rest_block = sb.nblocks - (sb_block + bitmap_block + inodeTable_block + 1);
}

void block_manager::read_block(uint32_t id, char *buf) {
    d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf) {
    d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager() {
    bm = new block_manager();
    uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
    if (root_dir != 1) {
        printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
        exit(0);
    }
}

/* Create a new file.
 * Return its inum. */
uint32_t inode_manager::alloc_inode(uint32_t type) {
    for (uint32_t inum = 1; inum <= bm->sb.ninodes; ++inum) {
        inode_t *inode = get_inode(inum);
        if (inode == NULL) {
            timespec time;
            if (clock_getres(CLOCK_REALTIME, &time) != 0) {
                return 0;
            }
            inode = new inode_t();
            inode->type = type;
            inode->size = 0;
            inode->atime = time.tv_nsec;
            inode->ctime = time.tv_nsec;
            inode->mtime = time.tv_nsec;
            put_inode(inum, inode);
            delete inode;
            return inum;
        }
        inode = NULL;
    }
    return 0;
}

void inode_manager::free_inode(uint32_t inum) {
    /*
     * your code goes here.
     * note: you need to check if the inode is already a freed one;
     * if not, clear it, and remember to write back to disk.
     */
    inode_t *inode = get_inode(inum);
    if (inode == NULL) {
        return;
    }
    // TODO 待实现
    return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode *inode_manager::get_inode(uint32_t inum) {
    struct inode *ino;

    if (inum >= INODE_NUM) {
        return NULL;
    }
    // 先读取该inode所在block
    char buffer[BLOCK_SIZE];
    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buffer);
    // 从所在block中读取目标inode
    ino = (inode_t *) buffer + inum % IPB;
    if (ino->type == 0) {
        // disk底层用char数组来表示存储单元，所有byte均为\0，若type为0则表示该空间未被分配
        return NULL;
    }

    // attention 必须重新分配并赋值，char[] buffer会被释放
    inode_t *inode_obj = (inode_t*) malloc(sizeof(inode_t));
    *inode_obj = *ino;

    return inode_obj;
}

void inode_manager::put_inode(uint32_t inum, struct inode *ino) {
    char buf[BLOCK_SIZE];
    struct inode *ino_disk;

    printf("\tim: put_inode %d\n", inum);
    if (ino == NULL)
        return;

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode *) buf + inum % IPB;
    *ino_disk = *ino;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a, b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return allocated data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size) {
    timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0)
        return;
    inode_t *file_node = get_inode(inum);
    if (file_node == NULL) {
        return;
    }
    // 更新最近访问时间
    file_node->atime = now.tv_nsec;
    put_inode(inum, file_node);

    // 读文件size
    *size = file_node->size;
    // 读文件内容
    int nblocks = (int) ceil((double) *size / (double) BLOCK_SIZE);
    *buf_out = new char[nblocks * BLOCK_SIZE];
    char *buf_cursor = *buf_out;
    for (int block_idx = 0; block_idx < MIN(nblocks, NDIRECT); ++block_idx) {
        bm->read_block(file_node->blocks[block_idx], buf_cursor);
        buf_cursor += BLOCK_SIZE;
    }
    if (nblocks <= NDIRECT) {
        // 文件size未超过直接引用，已写完
        return;
    }
    // 最后一块存的是block id的间接引用
    char indirect_block_refs[BLOCK_SIZE];
    bm->read_block(file_node->blocks[NDIRECT], indirect_block_refs);
    // 将最后一块的内容转换为int数组
    int *indirect_block_id = (int *) indirect_block_refs;
    // 逐块读取间接引用的block内容
    uint32_t rest_block = nblocks - NDIRECT;
    for (uint32_t indirect_block_idx = 0; indirect_block_idx < rest_block; ++indirect_block_idx) {
        bm->read_block(indirect_block_id[indirect_block_idx], buf_cursor);
        buf_cursor += BLOCK_SIZE;
    }
    return;
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, int size) {
    /*
     * your code goes here.
     * note: write buf to blocks of inode inum.
     * you need to consider the situation when the size of buf
     * is larger or smaller than the size of original inode
     */
    timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0)
        return;
    inode_t *file_node = get_inode(inum);
    if (file_node == NULL) {
        return;
    }
    file_node->atime = now.tv_nsec;
    file_node->mtime = now.tv_nsec;

    // 释放原空间
    if (file_node->size > 0) {
        int ori_n_blocks = (int) ceil((double) (file_node->size) / (double) BLOCK_SIZE);
        if (ori_n_blocks > NDIRECT) {
            // 存在间接引用
            char indirect_block_refs[BLOCK_SIZE];
            bm->read_block(file_node->blocks[NDIRECT], indirect_block_refs);
            int *indirect_block_id = (int *) indirect_block_refs;
            uint32_t rest_block = ori_n_blocks - NDIRECT;
            for (uint32_t i = 0; i < rest_block; ++i) {
                bm->free_block(indirect_block_id[i]);
            }
            bm->free_block(file_node->blocks[NDIRECT]);
        }
        // 释放直接引用
        for (int i = 0; i < NDIRECT && i < ori_n_blocks; ++i) {
            bm->free_block(file_node->blocks[i]);
        }
    }

    int n_blocks = (int) ceil((double) (size) / (double) BLOCK_SIZE);
    if (n_blocks > bm->rest_space()) {
        // 剩余空间不足
        return;
    }
    file_node->size = size;

    // 写新内容
    for (int i = 0; i < MIN(n_blocks, NDIRECT); ++i) {
        file_node->blocks[i] = bm->alloc_block();
        bm->write_block(file_node->blocks[i], buf);
        buf+=BLOCK_SIZE;
    }
    if (n_blocks > NDIRECT) {
        // 文件size较大，需要间接引用
        file_node->blocks[NDIRECT] = bm->alloc_block();
        char indirect_block[BLOCK_SIZE];
        int *indirect_ref_idx = (int*)indirect_block;
        for (int i = 0; i < n_blocks - NDIRECT; ++i) {
            indirect_ref_idx[i] = bm->alloc_block();
            bm->write_block(indirect_ref_idx[i], buf);
            buf+=BLOCK_SIZE;
        }
        bm->write_block(file_node->blocks[NDIRECT], indirect_block);
    }
    put_inode(inum, file_node);
    return;
}

void inode_manager::get_attr(uint32_t inum, extent_protocol::attr &a) {
    timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0)
        return;
    inode_t *file_node = get_inode(inum);
    if (file_node == NULL)
        return;

    file_node->atime = now.tv_nsec;
    a.atime = file_node->atime;
    a.ctime = file_node->ctime;
    a.mtime = file_node->mtime;
    a.size = file_node->size;
    a.type = file_node->type;

    put_inode(inum, file_node);
    return;
}

void inode_manager::remove_file(uint32_t inum) {
    /*
     * your code goes here
     * note: you need to consider about both the data block and inode of the file
     */
    inode_t *file_node = get_inode(inum);
    if (file_node==NULL)
        return;

    int nblock = (int) ceil((double) (file_node->size) / (double) BLOCK_SIZE);
    if (nblock>NDIRECT){
        // 存在间接引用
        char indirect_block_refs[BLOCK_SIZE];
        bm->read_block(file_node->blocks[NDIRECT], indirect_block_refs);
        int *indirect_block_id = (int *) indirect_block_refs;
        uint32_t rest_block = nblock - NDIRECT;
        for (uint32_t i = 0; i < rest_block; ++i) {
            bm->free_block(indirect_block_id[i]);
        }
        bm->free_block(file_node->blocks[NDIRECT]);
    }
    for (int i = 0; i < MIN(nblock, NDIRECT); ++i) {
        bm->free_block(file_node->blocks[i]);
    }
    delete file_node;
    put_inode(inum, new inode());
    return;
}
