#ifndef DBCACHE_H
#define DBCACHE_H

#include "muduo/base/Atomic.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Thread.h"
#include "muduo/base/ThreadPool.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpServer.h"

#include <stdlib.h>
#include <mysql/mysql.h>
#include <utility>
#include <map>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include "codec.h"

class dbCache
{
    public:
    dbCache(muduo::ThreadPool *threadPool);
    int getMovieCache(const muduo::net::TcpConnectionPtr& conn, LengthHeaderCodec* codecptr);
    int getSeatCache(const muduo::net::TcpConnectionPtr& conn, LengthHeaderCodec* codecptr, std::string hallid, std::string time, int row, int col);
    //int getHallCache(const muduo::net::TcpConnectionPtr& conn, std::string movie);
    int getScheCache(const muduo::net::TcpConnectionPtr& conn, std::string movie);
    int getHallchoiceCache(const muduo::net::TcpConnectionPtr& conn, std::string movie, std::string time);
    muduo::MutexLock *getsqllock(){return &sqlmutex_;}
    void movieCacheClear(){moviePtr_.reset(new std::vector<std::pair<std::string, std::string> >());}
    private:
    //warn
    int loadMovieCacheAndSend(const muduo::net::TcpConnectionPtr& tcpconn, LengthHeaderCodec* codecptr);
    int loadSeatCacheAndSend(muduo::net::TcpConnectionPtr& tcpconn, LengthHeaderCodec* codecptr, std::string hallid, std::string time, int hallrow, int hallcol, int row, int col);
    int getHallInfo(const muduo::net::TcpConnectionPtr& conn, std::string& hallid, int& hallrow, int& hallcol);
    int loadHallCacheWithLock(std::string& hallid, int& hallrow, int& hallcol);
    int loadScheCacheAndSend(muduo::net::TcpConnectionPtr& tcpconn, std::string& movie);
    int loadHallChoiceAndSend(muduo::net::TcpConnectionPtr& tcpconn, std::string& movie, std::string& time);
    
    //使用标识位方便在管理员修改表时,同步查询结果
    //TODO 需要实现的识别情况可能有
    //1. 'd'删除,删除缓存前使用此标记,防止读取
    //2. 'm'修改,用于删除缓存写回数据库时防止读取
    //3. 'k'正常情况
    //4. '0'表示在访问数据库
    std::shared_ptr<std::vector<std::pair<std::string, std::string> > > moviePtr_;
    //使用copy on write实现读不堵塞写,高效率并发读
    //使用*moviePtr_[0] == pair<string, string>("m","m")表示修改,pair("k","k")表示正常访问,pair("0","0")表示访问数据库
    std::map<std::pair<std::string, std::string>, std::vector<char> > seatCache_;//mutex[0]
    //厅和时间->座位情况 使用char(0)标识正在访问数据库,char('k')表示已访问完成
    //正常数组数据是char('0', '1'),可新增'-1'表示不可用
    std::map<std::string, std::pair<int, int> > hallCache_;//mutex[1]
    //厅->行列数 同步执行不需要标识
    std::map<std::string, std::vector<std::string> > scheCache_;//mutex[2]
    //电影名->时间表 使用char(0)表示正在访问数据库,char('k')表示已访问完成
	std::map<std::pair<std::string, std::string>, std::vector<std::string> > hallchoiceCache_;//mutex[3]
	//电影名和时间->厅 使用string("0")表示正在访问数据库,string("k")表示已访问完成
    muduo::MutexLock mutex_[5];//4把锁分别同步4个同步数据map
    muduo::MutexLock sqlmutex_;
    muduo::MutexLock seatmutex_;
    //mysql_init()和mysql_real_connect()似乎不是线程安全的,需要加锁
    muduo::ThreadPool *threadPoolptr_;
};

#endif
