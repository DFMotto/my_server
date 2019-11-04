# my_server
## 目的  
本项目是一个简单的静态http服务器，是在学习完csapp和apue后的一个练习项目，采用多线程主从reactor模型，由一个监听线程处理处理服务器端和客户端的连接和对任务的分发，具体的套接字处理交由线程池内的工作线程完成，线程池的数量可以通过my_server.conf来进行改动
## 使用的主要模型
·线程池
·epoll多路复用
·非阻塞io
·带有缓冲区的io
·HTTP 1.1
## 运行
linux 2.6以上版本
```c++
mkdir build 
cd build
cmake .. 
make
cd .. 
./build/my_server -c my_server.conf
```
## 并发测试
使用webbench来进行测试


