#include "muduo/net/TcpClient.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Thread.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/InetAddress.h"

#include <iostream>
#include <utility>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include "codec.h"
#include <set>

using namespace muduo;
using namespace muduo::net;
using namespace std;

const vector<muduo::string> vec = {"1,2019-4-16 17:00,5,10", "1,2019-4-16 17:00,5,1", "4,2019-4-16 10:00,3,1", "4,2019-4-16 10:00,3,5", "5,2019-4-16 17:00,1,10", "5,2019-4-16 17:00,1,1", "5,2019-4-16 10:00,2,3", "6,2019-4-16 21:00,4,1", "4,2019-4-16 21:00,4,4", "1,2019-4-18 17:00,3,1", "3,2019-4-19 21:00,2,4", "3,2019-4-19 21:00,3,4"};

class Client;

class Session : noncopyable
{
 public:
  Session(EventLoop* loop,
          const InetAddress& serverAddr,
          const string& name,
          Client* owner,
          LengthHeaderCodec *codec)
    : client_(loop, serverAddr, name),
      owner_(owner),
      codecptr_(codec)
  {
    client_.setConnectionCallback(
        std::bind(&Session::onConnection, this, _1));
    client_.setMessageCallback(
        std::bind(&Session::onMessage, this, _1, _2, _3));
  }

  void start()
  {
    client_.connect();
  }

  void stop()
  {
    client_.disconnect();
  }

 private:

  void onConnection(const TcpConnectionPtr& conn);

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
  {
    string msg(buf->retrieveAllAsString());
    cout << "Recieve " << msg << endl;
    int num = rand() % vec.size();
    muduo::string message = vec[num];
    codecptr_->send(get_pointer(conn), message);
  }

  TcpClient client_;
  Client* owner_;
  LengthHeaderCodec *codecptr_;
};

class Client : noncopyable
{
 public:
  Client(EventLoop* loop,
         const InetAddress& serverAddr,
         int sessionCount,
         int timeout,
         int threadCount
        )
    : loop_(loop),
      threadPool_(loop, "db-client"),
      sessionCount_(sessionCount),
      timeout_(timeout),
      codec_(bind(&Client::onStringMessage, this, _1, _2, _3))
  {
    //超时退出
    loop->runAfter(timeout, std::bind(&Client::handleTimeout, this));
    if (threadCount > 1)
    {
      threadPool_.setThreadNum(threadCount);
    }
    threadPool_.start();

    for (int i = 0; i < sessionCount; ++i)
    {
      char buf[32];
      snprintf(buf, sizeof buf, "C%05d", i);
      Session* session = new Session(threadPool_.getNextLoop(), serverAddr, buf, this, &codec_);
      session->start();
      sessions_.emplace_back(session);
    }
  }

  void onStringMessage(const TcpConnectionPtr& conn,
                       const string& message,
                       Timestamp)
                       {
                            return;
                       }
  const string& message() const
  {
    return vec[0];
  }

  void onConnect()
  {
    if (numConnected_.incrementAndGet() == sessionCount_)
    {
      LOG_WARN << "all connected";
    }
  }

  void onDisconnect(const TcpConnectionPtr& conn)
  {
    if (numConnected_.decrementAndGet() == 0)
    {
      LOG_WARN << "all disconnected";

      conn->getLoop()->queueInLoop(std::bind(&Client::quit, this));
    }
  }

 private:

  void quit()
  {
    loop_->queueInLoop(std::bind(&EventLoop::quit, loop_));
  }

  void handleTimeout()
  {
    LOG_WARN << "stop";
    for (auto& session : sessions_)
    {
      session->stop();
    }
  }

  EventLoop* loop_;
  EventLoopThreadPool threadPool_;
  int sessionCount_;
  int timeout_;
  std::vector<std::unique_ptr<Session>> sessions_;
  AtomicInt32 numConnected_;
  LengthHeaderCodec codec_;
};

void Session::onConnection(const TcpConnectionPtr& conn)
{
  if (conn->connected())
  {
    conn->setTcpNoDelay(true);
    int num = rand() % vec.size();
    muduo::string message = vec[num];
    cout << "send : " << message << endl;
    codecptr_->send(get_pointer(conn), message);
    owner_->onConnect();
  }
  else
  {
    owner_->onDisconnect(conn);
  }
}

int main(int argc, char* argv[])
{
    for(int i = 0; i < 5; i++)
    {
        int num = rand() % vec.size();
        muduo::string message = vec[num];
        cout << "message :" << message << endl;
    }
    
  if (argc != 6)
  {
    fprintf(stderr, "Usage: client <host_ip> <port> <threads> <blocksize> ");
    fprintf(stderr, "<sessions> <time>\n");
  }
  else
  {
    LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
    Logger::setLogLevel(Logger::WARN);

    const char* ip = argv[1];
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    int threadCount = atoi(argv[3]);
    int sessionCount = atoi(argv[4]);
    int timeout = atoi(argv[5]);

    EventLoop loop;
    InetAddress serverAddr(ip, port);
    
    /*string s = "2,2019-4-18 17:00,5,10";
    string s2 = "2,2019-4-18 17:00,3,1";
    string s3 = "2,2019-4-18 17:00,2,6";
    string s4 = "2,2019-4-18 17:00,1,10";
    
    vec.push_back(s);
    vec.push_back(s2);
    vec.push_back(s3);
    vec.push_back(s4);*/

    Client client(&loop, serverAddr, sessionCount, timeout, threadCount);
    loop.loop();
  }
}

