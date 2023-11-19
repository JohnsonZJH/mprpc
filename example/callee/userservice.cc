#include <iostream>
#include <string>

#include "user.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"

// Uservice原来是一个本地服务，提供两个进行的本地方法
class UserService : public fixbug::UserServiceRpc   //使用在rpc服务发布端（rpc服务提供者）
{
public:
    bool Login(std::string name, std::string pwd)
    {
        std::cout << "doing local service:Login" << std::endl;
        std::cout << "name:" << name << " pwd:" << pwd << std::endl;
        return true;
    }

    bool Register(uint32_t id, std::string name, std::string pwd)
    {
        std::cout << "doing Register service:Login" << std::endl;
        std::cout << "id:" << id << " name:" << name << " pwd:" << pwd << std::endl;
        return true;
    }

    /*
    重写基类UserServiceRpc的虚函数，框架直接调用
    1.caller ==> Login(LoginRequest) ==> muduo ==> callee
    2.callee ==> Login(LoginRequest) ==>交到下面的Login方法
    */
    void Login(::google::protobuf::RpcController* controller,
                       const ::fixbug::LoginRequest* request,
                       ::fixbug::LoginResponse* response,
                       ::google::protobuf::Closure* done)
    {
        //1、框架给业务上报请求参数LoginRequest,应用程序获取数据
        std::string name = request->name();
        std::string pwd = request->pwd();

        //2、本地业务
        bool login_result = Login(name, pwd);
        
        //3、把响应给调用方返回
        fixbug::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_success(login_result);

        //4、执行回调  执行响应对象数据的序列化和网络发送（都是由框架完成）
        done->Run();
    }

    void Register(::google::protobuf::RpcController* controller,
                       const ::fixbug::RegisterRequest* request,
                       ::fixbug::RegisterResponse* response,
                       ::google::protobuf::Closure* done)
    {
        uint32_t id = request->id();
        std::string name = request->name();
        std::string pwd = request->pwd();

        bool ret = Register(id, name, pwd);

        response->mutable_resultcode()->set_errcode(0);
        response->mutable_resultcode()->set_errmsg("");
        response->set_sucess(ret);

        done->Run(); //这个回调作用是将response参数序列化并发回给调用方
    }

};

int main(int argc, char **argv)
{
    //调用框架的初始化操作
    MprpcApplication::Init(argc, argv);

    //一个网络服务对象，把UserService对象发布到rpc节点上
    RpcProvider provider;
    provider.NotifyService(new UserService()); 

    //启动一个rpc服务发布节点，Run以后进入阻塞状态，等待远程rpc请求
    provider.Run();
    return 0;
}