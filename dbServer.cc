
#include "muduo/base/Atomic.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Thread.h"
#include "muduo/base/ThreadPool.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpServer.h"

#include "codec.h"
#include "dbCache.h"
#include<stdlib.h>
#include<mysql/mysql.h>
#include <utility>
#include <map>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

using namespace muduo;
using namespace muduo::net;
using namespace std;


class dbServer
{
 public:
    dbServer(EventLoop* loop, const InetAddress& listenAddr, int numThreads)
        : server_(loop, listenAddr, "dbServer"),
         codec_(std::bind(&dbServer::onStringMessage, this, _1, _2, _3)),
         numThreads_(numThreads),
         cache_(&threadPool_)
    {
        server_.setConnectionCallback(
            std::bind(&dbServer::onConnection, this, _1));
        server_.setMessageCallback(
            std::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
    }

    void start()
    {
        LOG_INFO << "starting " << numThreads_ << " threads.";
        server_.setThreadNum(numThreads_);
        threadPool_.start(numThreads_);
        server_.start();
    }
    
    private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        printf("In EventLoop pid = %d\n", muduo::CurrentThread::tid());
        LOG_TRACE << conn->peerAddress().toIpPort() << " -> "
            << conn->localAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
    }


    vector<string> split(char c, string s)
    {
        vector<string> v;
        string tmp;
        for(auto ch: s)
        {
            if (ch != c)
                tmp += ch;
            else
            {
                v.push_back(tmp);
                tmp = "";
            }
        }
        if (tmp != "")
            v.push_back(tmp);
        return v;
    }
    
    void onStringMessage(const TcpConnectionPtr& conn,
                       const string& message,
                       Timestamp)
    {
        vector<string> v = split(',', message);
        int res = cache_.getCache(conn, v[0], v[1], atoi(v[2].c_str()), atoi(v[3].c_str()));
        if (res == 1)
            printf("getCache() return 1\n");
        else if (res == 2)
            printf("getCache() return 2\n");
        else if (res == 3)
            printf("getCache() return 3\n");
        else if (res == 0)
            printf("getCache() return 0\n");
        else 
            printf("getCache() return error\n");
    }

 
  TcpServer server_;
  ThreadPool threadPool_;
  LengthHeaderCodec codec_;
  dbCache cache_;
  int numThreads_;
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  int numThreads = 4;
  if (argc > 1)
  {
    numThreads = atoi(argv[1]);
  }
  EventLoop loop;
  InetAddress listenAddr(9981);
  dbServer server(&loop, listenAddr, numThreads);

  server.start();

  loop.loop();
}

