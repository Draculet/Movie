#include "muduo/base/Atomic.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Thread.h"
#include "muduo/base/ThreadPool.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpServer.h"

#include "codec.h"
#include <stdlib.h>
#include <mysql/mysql.h>
#include <utility>
#include <map>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

void loadCacheAndSend(muduo::net::TcpConnectionPtr& tcpconn, muduo::string hallid, muduo::string time, int hallrow, int hallcol, int row, int col, std::map<std::pair<muduo::string, muduo::string>, std::vector<char> > *cache, muduo::MutexLock  *mutex_)
    {
        int res;//执行sql语句后的返回标志
        MYSQL_RES *res_ptr;//指向查询结果的指针
        MYSQL_FIELD *field;//字段结构指针
        MYSQL_ROW result_row;//按行返回查询信息
        int resrow,rescolumn,length;//查询返回的行数和列数
        MYSQL *conn;//一个数据库链接指针
        conn = mysql_init(NULL);

        if(conn == NULL) 
        { //如果返回NULL说明初始化失败
            std::cout << "mysql_init failed!" << std::endl;
            return;
        }
        conn = mysql_real_connect(conn,"127.0.0.1","root","13640358","Movie", 0 ,NULL,0);
        if (conn)
        {
            std::cout << "Connection success!" << std::endl;
        }
        else 
        {
            std::cout << "Connection failed!" << std::endl;
            std::cout << mysql_error(conn) << std::endl;
            return;
        }
        mysql_query(conn,"set names UTF8");
        //FIXME
        muduo::string sql = "select Issale from Table_Seat where S_ID=(select S_ID from Table_Schedule where H_ID=" + hallid + " AND S_TIME=\'" + time + "\')";
        res = mysql_query(conn, sql.c_str());

        if(res)
        {
            std::cout << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return;
        }
        else
        {
            std::pair<muduo::string, muduo::string> p(hallid, time);
            res_ptr = mysql_store_result(conn);
            if (res_ptr)
            {
                resrow = mysql_num_rows(res_ptr);
              {
                muduo::MutexLockGuard lock(*mutex_);
                for (int i = 0; i < resrow; i++)
                {
                    result_row = mysql_fetch_row(res_ptr);
                    //cout << result_row[0] << " ";
                    (*cache)[p].push_back(*result_row[0]);
                }
                (*cache)[p][0] = 'k';
              }
            }
            else
            {
                std::cout << "Failed to Get Seat Detail" << std::endl;
                mysql_close(conn);
                mysql_free_result(res_ptr);
                return;
            }
            std::cout << "缓存读取完毕" << std::endl;
            //for (auto &ch: cache[p])
            //{
            //    cout << ch << " ";
            //}
          {
            muduo::MutexLockGuard lock(*mutex_);
            if ((*cache)[p][hallcol * (row - 1) + col] == '1')
            {
                std::cout << "票已售" << std::endl;
                muduo::string responce = "fail";
                tcpconn->send(responce);
            }
            else
            {
                (*cache)[p][hallcol * (row - 1) + col] = '1';
                std::cout << "购票成功" << std::endl;
                //for (auto &ch: cache[p])
                //{
                //    cout << ch << " ";
                //}
                muduo::string responce = "succ";
                tcpconn->send(responce);
             }
          } 
        }
        mysql_close(conn);
        mysql_free_result(res_ptr);
    }
    
    void getCacheAndSend(muduo::net::TcpConnectionPtr& tcpconn, muduo::string hallid, muduo::string time, int hallrow, int hallcol, int row, int col, std::map<std::pair<muduo::string, muduo::string>, std::vector<char> > *cache, muduo::MutexLock  *mutex_)
    {
          std::pair<muduo::string, muduo::string> p(hallid, time);
          {
            muduo::MutexLockGuard lock(*mutex_);
             if ((*cache)[p][0] == 0)
             {
                tcpconn->getLoop()->runAfter(1, bind(&getCacheAndSend, tcpconn, hallid, time, hallrow, hallcol, row, col, cache, mutex_));
                return;
             }
             else if ((*cache)[p][0] == 'k')
             {
                if ((*cache)[p][hallcol * (row - 1) + col] == '1')
                {
                    std::cout << "票已售get()" << std::endl;
                    muduo::string responce = "fail";
                    tcpconn->send(responce);
                }
                else
                {
                    (*cache)[p][hallcol * (row - 1) + col] = '1';
                    std::cout << "购票成功get()" << std::endl;
                    muduo::string responce = "succ";
                    tcpconn->send(responce);
                 }
            } 
        }
    }
    
    
class dbCache
{
    public:
    dbCache(muduo::ThreadPool *threadPool):
        threadPoolptr_(threadPool)
    {
    }
    int getCache(const muduo::net::TcpConnectionPtr& conn, muduo::string hallid, muduo::string time, int row, int col)
    {
        int hallrow;
        int hallcol;
        getHallInfo(hallid, hallrow, hallcol);
        std::pair<muduo::string, muduo::string> p(hallid, time);
        //mutexGuard
      {
        muduo::MutexLockGuard lock(mutex_);
        auto iter = cache.begin();
        iter = cache.find(p);
        if (iter == cache.end())
        {
            //mutex
            cache[p].push_back(0);
            //endmutex
            std::cout << "读缓存" << std::endl;
            //TODO 读取缓存,查询状态,回调?或者使用runAfter()
            sleep(5);
            threadPoolptr_->run(bind(&loadCacheAndSend, conn, hallid, time, hallrow, hallcol, row, col, &cache, &mutex_));
            
            return 0;
        }
        else
        {
            //数组第一个作为状态标记
            if (cache[p][0] == 0)
            {
                std::cout << "正在读取缓存" << std::endl;
                conn->getLoop()->runAfter(5, bind(&getCacheAndSend, conn, hallid, time, hallrow, hallcol, row, col, &cache, &mutex_));
                return 1;
            }
            else
            {   
                if (cache[p][hallcol * (row - 1) + col] == '1')
                {
                    std::cout << "票已售" << std::endl;
                    muduo::string responce = "fail";
                    conn->send(responce);
                    return 2;
                }
                else
                {
                    cache[p][hallcol * (row - 1) + col] = '1';
                    std::cout << "购票成功" << std::endl;
                    //for (auto &ch: cache[p])
                    //{
                    //    cout << ch << " ";
                    //}
                    muduo::string responce = "succ";
                    conn->send(responce);
                    return 3;
                }
            }
        }
      }
    }
    private:
    
    int getHallInfo(muduo::string &hallid, int &hallrow, int &hallcol)
    {
        int res;//执行sql语句后的返回标志
        MYSQL_RES *res_ptr;//指向查询结果的指针
        MYSQL_FIELD *field;//字段结构指针
        MYSQL_ROW result_row;//按行返回查询信息
        int row,column,length;//查询返回的行数和列数
        MYSQL *conn;//一个数据库链接指针
        conn = mysql_init(NULL);

        if(conn == NULL) 
        { //如果返回NULL说明初始化失败
            std::cout << "mysql_init failed!" << std::endl;
            return -1;
        }
        conn = mysql_real_connect(conn,"127.0.0.1","root","13640358","Movie", 0 ,NULL,0);
        if (conn)
        {
            std::cout << "Connection success!" << std::endl;
        }
        else 
        {
            std::cout << "Connection failed!" << std::endl;
            std::cout << mysql_error(conn) << std::endl;
            return -1;
        }
        mysql_query(conn,"set names UTF8");
        //FIXME
        muduo::string sql = "select SEAT_ROW, SEAT_COL from Table_Hall where H_ID=" + hallid;
        res = mysql_query(conn, sql.c_str());

        if(res) 
        {
            std::cout << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return -1;
        }
        else
        {
            res_ptr = mysql_store_result(conn);
            if (res_ptr)
            {
                result_row = mysql_fetch_row(res_ptr);
                hallrow = atoi(result_row[0]);
                hallcol = atoi(result_row[1]);
            }
            else
            {
                std::cout << "Failed to Get Hall Info" << std::endl;
                mysql_close(conn);
                mysql_free_result(res_ptr);
                return -1;
            }
            mysql_close(conn);
            mysql_free_result(res_ptr);
        }
    }
    
    std::map<std::pair<muduo::string, muduo::string>, std::vector<char> > cache /*GUARDED_BY(mutex_)*/;
    muduo::MutexLock mutex_;
    muduo::ThreadPool *threadPoolptr_;
};
