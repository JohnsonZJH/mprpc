#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "logger.h"
#include "zookeeperutil.h"

//不能接收业务的具体服务对象，使用基类指针
void RpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info;
    //获取了服务对象的描述信息
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor();

    //获取服务的名字
    std::string service_name = pserviceDesc->name();
    //获取服务对象service的方法数量
    int methodCnt = pserviceDesc->method_count();

    // std::cout << "service_name: " << service_name << std::endl;
    LOG_INFO("service_name: %s", service_name.c_str());
    for (int i = 0; i < methodCnt; ++i)
    {
        //获取了服务对象指定下标的服务方法描述（抽象描述）
        const google::protobuf::MethodDescriptor* pmthodDesc = pserviceDesc->method(i);
        std::string method_name = pmthodDesc->name();
        service_info.m_methodMap.insert({method_name, pmthodDesc});

        // std::cout << "method_name: " << method_name << std::endl;
        LOG_INFO("method_name: %s", method_name.c_str());
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}

//启动rpc服务节点，开始提供rpc远程网络调用服务
void RpcProvider::Run()
{
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    //创建TcpServer对象
    // TcpServer(EventLoop* loop, const InetAddress& listenAddr, 
    //          const string& nameArg, Option option = kNoReuserPort)
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");
    
    //绑定链接回调和消息回调 分离了网络代码和业务代码
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, 
                            std::placeholders::_2, std::placeholders::_3));

    //设置muduo库的线程数量
    server.setThreadNum(4);

    //把当前rpc节点上要发布的服务全部注册到zk上面
    ZkClient zkCli;
    zkCli.Start();
    //service_name为永久节点 method_name为临时节点
    for (auto &sp : m_serviceMap)
    {
        //service_name
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0);
        for (auto &mp: sp.second.m_methodMap)
        {
            //method_name
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            //ZOO_EPHEMERAL表示临时节点
            zkCli.Create(method_path.c_str(), method_path_data,
                        strlen(method_path_data),
                        ZOO_EPHEMERAL);
        }

    }
    
    std::cout << "RpcProvider start service at ip:" << ip << " port:" << port << std::endl; 
    // LOG_INFO("RpcProvider start service at ip: %d port: %d", ip, port);

    //启动网络服务
    server.start();
    m_eventLoop.loop(); //以阻塞的方式等待远程连接 
}

void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        // 和rpc client连接断开了
        conn->shutdown();
    }
    
}

/*
在框架内部，RpcProvider和RpcConsumer协商好之间通信用额protobuf数据类型
例如：16UserServiceLoginzhangsan san123456
需要指定数据头的长度和参数的长度（针对Tcp传输的粘包问题）
数据长度需要直接转成二进制进行存储，不能转字符串格式进行存储
*/
//已建立连接用户的读写事件回调
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn, 
                            muduo::net::Buffer *buffer, 
                            muduo::Timestamp)
{
    //网络上接收的远程rpc调用请求的字符流 Login args 
    std::string recv_buf = buffer->retrieveAllAsString();

    //从字符串中读取前4个字节的内容
    uint32_t header_size = 0;
    recv_buf.copy((char*)&header_size, 4, 0);

    //根据header_size读取数据头的原始字符流，反序列化数据，得到rpc请求的详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if (rpcHeader.ParseFromString(rpc_header_str))
    {
        //数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else
    {
        //数据头反序列化失败
        std::cout << "rpc_header_str:" <<rpc_header_str << " parse error!" << std::endl;
        return;
    }

    //获取rpc方法参数的字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    //打印调试信息
    std::cout << "===================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "===================================" << std::endl;
    
    //获取service对象和method对象
    auto it = m_serviceMap.find(service_name);
    if (it == m_serviceMap.end())
    {
        std::cout << service_name << " is not exist!" << std::endl;
        return;
    }

    //判断方法是否存在
    auto mit = it->second.m_methodMap.find(method_name);
    if (mit == it->second.m_methodMap.end())
    {
        std::cout << service_name << ":" << method_name << " is not exist!" << std::endl;
        return;
    }

    google::protobuf::Service *service = it->second.m_service; // 获取service对象
    const google::protobuf::MethodDescriptor *method = mit->second; // 获取method方法

    //生成rpc方法调用的请求request和响应response参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New(); //产生一个服务对象的方法的请求类型
    if (!request->ParseFromString(args_str))
    {
        std::cout << "request parse error, content :" << args_str << std::endl;
        return;
    }
    
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    //给下面的method方法的调用，绑定一个Closure的回调函数
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider, 
                                            const muduo::net::TcpConnectionPtr&,
                                            google::protobuf::Message*>
                                            (this, &RpcProvider::SendRpcResponse, 
                                            conn, response);

    //在框架上根据远端rpc远程， 调用当前rpc节点上发布的方法
    service->CallMethod(method, nullptr, request, response, done); // 这里的回调会调用远程指定的方法
}

//Closure回调操作，用于序列化rpc的响应和网络发送
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, 
                                  google::protobuf::Message *response)
{
    std::string response_str;
    if (response->SerializeToString(& response_str)) //进行序列化成字符流
    {
        //序列化成功后，通过网络把rpc方法执行的结果发送回rpc的调用方
        conn->send(response_str);
    }
    else
    {
        std::cout << "serialize response_str error!" << std::endl;
    }
    conn->shutdown();   // 模拟http短链接服务，由rpcprovider主动断开连接
}