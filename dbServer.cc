#include "muduo/base/Atomic.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Thread.h"
#include "muduo/base/ThreadPool.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpServer.h"

#include "codec.h"
#include "dbCache.h"
#include "dbop.h"
#include <stdlib.h>
#include <mysql/mysql.h>
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
        //printf("OnConnection In EventLoop pid = %d\n", muduo::CurrentThread::tid());
        LOG_INFO << conn->peerAddress().toIpPort() << " -> "
            << conn->localAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
        //connCount++;
        //cout << "Connections Count : " << connCount << endl;
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
    	//增加发送现有电影名及图片和文本
    	string header = message.substr(0, 3);
    	string mes;
    	if (header == "GMI")
    	{
    		int res = cache_.getMovieCache(conn, &codec_);
    	}
    	if (header == "GSC")
    	{
    		mes = message.substr(4);
        	vector<string> v = split(',', mes);
        	//TODO 判断合法
        	int res = cache_.getSeatCache(conn, &codec_, v[0], v[1], atoi(v[2].c_str()), atoi(v[3].c_str()));
       	}
       	if (header == "GST")
       	{
       		mes = message.substr(4);
        	int res = cache_.getScheCache(conn, mes);
       	}
       	if (header == "GHC")
       	{
       		mes = message.substr(4);
       		vector<string> v = split(',', mes);
       		int res = cache_.getHallchoiceCache(conn, v[0], v[1]);
       	}
       	if (header == "ADD")
       	{
       		string mes = message.substr(4);
       	    vector<string> v = split(',', mes);
       	    Schedule s(v[0], atoi(v[1].c_str()), atoi(v[2].c_str()), v[3]);
       		vector<Schedule> vs;
       		vs.push_back(s);
       		DBoper op;
       		{
       			MutexLockGuard lock(*(cache_.getsqllock()));
       			op.insertMovie(v[0], vs);
       			cache_.movieCacheClear();
       		}
       		
       		//写入数据库后把电影内存数据清空.
       		//其他内存缓存会自动读取
       	}
       	if (header == "ADDSCHE")
       	{
       		
       	}
    }

 
  TcpServer server_;
  ThreadPool threadPool_;
  LengthHeaderCodec codec_;
  dbCache cache_;
  int numThreads_;
  //int connCount = 0;
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

