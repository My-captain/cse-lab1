// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>
#include "extent_protocol.h"

// 16M Bytes
#define DISK_SIZE  1024*1024*16
// 一个Block为512 Bytes
#define BLOCK_SIZE 512
// block数量
#define BLOCK_NUM  (DISK_SIZE/BLOCK_SIZE)

typedef uint32_t blockid_t;

// disk layer -----------------------------------------

// 提供按block id的读写服务
class disk {
private:
    unsigned char blocks[BLOCK_NUM][BLOCK_SIZE];

public:
    disk();

    void read_block(uint32_t id, char *buf);

    void write_block(uint32_t id, const char *buf);
};

// block layer -----------------------------------------

typedef struct superblock {
    uint32_t size;
    uint32_t nblocks;
    uint32_t ninodes;
} superblock_t;

class block_manager {
private:
    disk *d;
    // block id -> block num
    std::map<uint32_t, int> using_blocks;

    int rest_block;
public:
    block_manager();

    struct superblock sb;

    int rest_space() {return rest_block;}

    uint32_t alloc_block();

    void free_block(uint32_t id);

    void read_block(uint32_t id, char *buf);

    void write_block(uint32_t id, const char *buf);
};

// inode layer -----------------------------------------

// inode总数量
#define INODE_NUM               1024
// 每个block可以放多少个inode
#define IPB                     (BLOCK_SIZE / sizeof(struct inode))
// 包含第i个inode的block索引
// |<-boot sector->|<-super block->|<-Bitmap->|InodeTable|
// (nblocks)/BPB+1  Bitmap实际占用的block数量
#define IBLOCK(i, nblocks)      ((nblocks)/BPB + (i)/IPB + 3)
// 每个block共有多少个bit
#define BPB                     (BLOCK_SIZE*8)
// 包含block_b使用情况的Bitmap所在block的索引
#define BBLOCK(b)               ((b)/BPB + 2)
// inode直接引用的最大的block数量
#define NDIRECT                 100
#define NINDIRECT               (BLOCK_SIZE / sizeof(uint))
#define MAXFILE                 (NDIRECT + NINDIRECT)

// 每个inode代表一个文件，最大占用NDIRECT+1个block
typedef struct inode {
    short type;
    // 以字节为单位的文件size
    unsigned int size;
    // access time
    unsigned int atime;
    // modify time
    unsigned int mtime;
    // create time
    unsigned int ctime;
    // 最后一个块用于存储inode间接引用
    blockid_t blocks[NDIRECT + 1];   // Data block addresses
} inode_t;

class inode_manager {
    // inode的管理是通过查看是否type为0来识别空闲inode的
private:
    block_manager *bm;

    struct inode *get_inode(uint32_t inum);

    void put_inode(uint32_t inum, struct inode *ino);

public:
    inode_manager();

    // 根据类型（目录或文件）分配inode，并返回inode号
    uint32_t alloc_inode(uint32_t type);

    // 释放inode及其占用空间
    void free_inode(uint32_t inum);

    // 根据inode号读取文件content及其size
    void read_file(uint32_t inum, char **buf, int *size);

    // 根据inode号写文件content及其size
    // 文件超出直接索引范围时，最后一个盘块是索引块（存储着间接索引的盘块号）
    void write_file(uint32_t inum, const char *buf, int size);

    // 根据inode号删除文件
    void remove_file(uint32_t inum);

    // 获取inode的属性
    void get_attr(uint32_t inum, extent_protocol::attr &a);
};

#endif

