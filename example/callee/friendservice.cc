#include <iostream>
#include <string>
#include <vector>

#include "friend.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"
#include "logger.h"

class FriendService : public fixbug::FriendServiceRpc
{
public:
    std::vector<std::string> GetFriendsList(uint32_t userid)
    {
        std::cout << "do GetFriendsList service!";
        std::vector<std::string> vec;
        vec.push_back("gao yang");
        vec.push_back("liu hong");
        vec.push_back("wang shuo");
        return vec;
    }

    void GetFriendsList(::google::protobuf::RpcController* controller,
                       const ::fixbug::GetFriendsListRequest* request,
                       ::fixbug::GetFriendsListResponse* response,
                       ::google::protobuf::Closure* done)
    {
        uint32_t userid = request->userid();

        std::vector<std::string> friendsList = GetFriendsList(userid);

        response->mutable_result()->set_errcode(0);
        response->mutable_result()->set_errmsg("");
        for (std::string &name : friendsList)
        {
            std::string *p = response->add_friends();
            *p = name;
        }
        done->Run();
        

    }
};

int main(int argc, char **argv)
{
    LOG_INFO("frist log message!");
    LOG_ERROR("%s:%s:%d", __FILE__, __FUNCTION__, __LINE__);

    //调用框架的初始化操作
    MprpcApplication::Init(argc, argv);

    //一个网络服务对象，把FriendService对象发布到rpc节点上
    RpcProvider provider;
    provider.NotifyService(new FriendService()); 

    //启动一个rpc服务发布节点，Run以后进入阻塞状态，等待远程rpc请求
    provider.Run();
    return 0;
} 