// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"

class extent_client {
private:
    extent_server *es;

public:
    extent_client();

    // 创建文件
    extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);

    // 写文件
    extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);

    // 读文件
    extent_protocol::status get(extent_protocol::extentid_t eid, std::string &buf);

    // 读inode props
    extent_protocol::status getattr(extent_protocol::extentid_t eid, extent_protocol::attr &a);

    // 删除文件
    extent_protocol::status remove(extent_protocol::extentid_t eid);
};

#endif 

