# my_server
## 目的  
这是一个简单的静态http服务器，是在学习完csapp和apue后的一个练习项目，实现中参考了不少dalao的blog。服务器采用多线程主从reactor模型，由一个监听线程处理处理服务器端和客户端的连接和对任务的分发，具体的套接字处理交由线程池内的工作线程完成，线程池的数量可以通过my_server.conf来进行改动。各文件的具体实现功能见源码内的注释。
## 主要特性
·线程池  
·epoll多路复用  
·非阻塞io  
·应用级缓冲区io（rio包） 
## 整体架构及部分实现
基础架构是由csapp第11章的示例服务器拓展而来。  
服务器采用多线程主从reactor模型，即有一个acceptor线程来进行监听，处理连接请求等工作，具体的通信和数据交换交由线程池内的工作线程来执行。
参考资料：https://www.cnblogs.com/winner-0715/p/8733787.html
### 线程池的部分实现细节
根据配置文件my_server.config中的数据进行初始化，创建一个线程池对象，并且维护条件变量，互斥锁，一个任务队列以及所有工作线程。任务队列用以存放待运行的任务函数入口。当任务队列不为空时会通过条件变量唤醒阻塞中的线程。对于公共资源的读写要注意加锁，避免产生竞争。
### epoll的模式选择
https://blog.csdn.net/eyucham/article/details/86502117 参考这位大佬的博客，为什么要用ET和ONESHOT已经说的很明白了。
### RIO包相关
参考自csapp第11章。注意要区分应用级缓冲和tcp socket缓冲区的区别。在通过socket进行数据传输时，首先是通过read/write将数据写入位于内核中的socket缓冲区，再通过socket进行发送/接收。
### http解析
为每一个http请求维护一个自定义的http请求结构体，使用http报文来进行初始化，支持长连接。具体实现可见相关源代码。
## 运行
linux 2.6以上版本  
在文件根目录打开终端  
```c++
mkdir build 
cd build
cmake .. 
make
cd .. 
./build/my_server -c my_server.conf
```  
成功运行服务器后打开浏览器，输入localhost:端口号 进入预设的网页  
端口号可以通过my_server.conf来进行设置
## 并发测试
使用webbench测试

