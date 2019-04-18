#include "muduo/base/Atomic.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Thread.h"
#include "muduo/base/ThreadPool.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpServer.h"

#include "codec.h"

#include <utility>

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

class dbServer
{
 public:
  dbServer(EventLoop* loop, const InetAddress& listenAddr, int numThreads)
    : server_(loop, listenAddr, "dbServer"),
      codec_(std::bind(&ChatServer::onStringMessage, this, _1, _2, _3));
      numThreads_(numThreads)
  {
    server_.setConnectionCallback(
        std::bind(&dbServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
  }

  void start()
  {
    LOG_INFO << "starting " << numThreads_ << " threads.";
    threadPool_.start(numThreads_);
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_TRACE << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  }


  bool processRequest(const TcpConnectionPtr& conn, const string& request)
  {
    
  }
  
  void onStringMessage(const TcpConnectionPtr& conn,
                       const string& message,
                       Timestamp)
  {
    
  }

  TcpServer server_;
  ThreadPool threadPool_;
  int numThreads_;
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  int numThreads = 0;
  if (argc > 1)
  {
    numThreads = atoi(argv[1]);
  }
  EventLoop loop;
  InetAddress listenAddr(9981);
  SudokuServer server(&loop, listenAddr, numThreads);

  server.start();

  loop.loop();
}

