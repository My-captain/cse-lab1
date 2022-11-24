// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "inode_manager.h"

class extent_server {
protected:
#if 0
    typedef struct extent {
      std::string data;
      struct extent_protocol::attr attr;
    } extent_t;
    std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
    inode_manager *im;

public:
    extent_server();

    // 创建文件
    int create(uint32_t type, extent_protocol::extentid_t &id);

    // 写文件
    int put(extent_protocol::extentid_t id, std::string, int &);

    // 读文件
    int get(extent_protocol::extentid_t id, std::string &);

    // 读inode props
    int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);

    // 删除文件
    int remove(extent_protocol::extentid_t id, int &);
};

#endif 







