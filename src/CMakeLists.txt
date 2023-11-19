# aux_source_directory(. SRC_LIST)
set(SRC_LIST mprpcapplication.cc 
             mprpcconfig.cc 
             rpcheader.pb.cc
             rpcprovider.cc
             mprpcchannel.cc
             mprpccontroller.cc
             logger.cc
             zookeeperutil.cc)
# 在add_library 加入 SHARED 是生成动态库，不加会生成静态库
add_library(mprpc ${SRC_LIST})
# muduo_net是muduo库的核心，muduo_base依赖muduo_net
target_link_libraries(mprpc muduo_net muduo_base pthread zookeeper_mt)