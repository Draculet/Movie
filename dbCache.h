
#include "muduo/base/Atomic.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Thread.h"
#include "muduo/base/ThreadPool.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpServer.h"

#include "codec.h"
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

class dbCache
{
    public:
    dbCache(ThreadPool *threadPool):
        threadPoolptr_(threadPool)
    {
    }
    int getCache(const TcpConnectionPtr& conn, string hallid, string time, int row, int col)
    {
        int hallrow;
        int hallcol;
        getHallInfo(hallid, hallrow, hallcol);
        pair<string, string> p(hallid, time);
        //mutexGuard
      {
        MutexLockGuard lock(mutex_);
        auto iter = cache.begin();
        iter = cache.find(p);
        if (iter == cache.end())
        {
            //mutex
            cache[p].push_back(0);
            //endmutex
            cout << "读缓存" << endl;
            //TODO 读取缓存,查询状态,回调?或者使用runAfter()
             threadPoolptr_->run(bind(&dbCache::loadCacheAndSend, this, conn, hallrow, hallcol, row, col));
            //runAfter(1, bind(&dbCache::getCache, this, hallid, time, row, col));
            return 0;
        }
        else
        {
            //数组第一个作为状态标记
            if (cache[p][0] == 0)
            {
                cout << "正在读取缓存" << endl;
                conn->getLoop()->runAfter(1, bind(&dbCache::getCache, this, hallid, time, row, col));
                return 1;
            }
            else
            {   
                if (cache[p][hallcol * (row - 1) + col] == '1')
                {
                    cout << "票已售" << endl;
                    string responce = "fail";
                    conn->send(responce);
                    return 2;
                }
                else
                {
                    cache[p][hallcol * (row - 1) + col] = '1';
                    cout << "购票成功" << endl;
                    //for (auto &ch: cache[p])
                    //{
                    //    cout << ch << " ";
                    //}
                    string responce = "succ";
                    conn->send(responce);
                    return 3;
                }
            }
        }
      }
    }
    private:
    
    int loadCacheAndSend(TcpConnectionPtr& tcpconn, string hallid, string time, int hallrow, int hallcol, int row, int col)
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
            cout << "mysql_init failed!" << endl;
            return -1;
        }
        conn = mysql_real_connect(conn,"127.0.0.1","root","13640358","Movie", 0 ,NULL,0);
        if (conn)
        {
            cout << "Connection success!" << endl;
        }
        else 
        {
            cout << "Connection failed!" << endl;
            cout << mysql_error(conn) << endl;
            return -1;
        }
        mysql_query(conn,"set names UTF8");
        //FIXME
        string sql = "select Issale from Table_Seat where S_ID=(select S_ID from Table_Schedule where H_ID=" + hallid + " AND S_TIME=\'" + time + "\')";
        res = mysql_query(conn, sql.c_str());

        if(res)
        {
            cout << mysql_error(conn) << endl;
            mysql_close(conn);
            return -1;
        }
        else
        {
            pair<string, string> p(hallid, time);
            res_ptr = mysql_store_result(conn);
            if (res_ptr)
            {
                resrow = mysql_num_rows(res_ptr);
              {
                MutexLockGuard lock(mutex_);
                for (int i = 0; i < resrow; i++)
                {
                    result_row = mysql_fetch_row(res_ptr);
                    //cout << result_row[0] << " ";
                    cache[p].push_back(*result_row[0]);
                }
                cache[p][0] = 'k';
              }
            }
            else
            {
                cout << "Failed to Get Seat Detail" << endl;
                mysql_close(conn);
                mysql_free_result(res_ptr);
                return -1;
            }
            cout << "缓存读取完毕" << endl;
            //for (auto &ch: cache[p])
            //{
            //    cout << ch << " ";
            //}
          {
            MutexLockGuard lock(mutex_);
            if (cache[p][hallcol * (row - 1) + col] == '1')
            {
                cout << "票已售" << endl;
                string responce = "fail";
                tcpconn->send(responce);
            }
            else
            {
                cache[p][hallcol * (row - 1) + col] = '1';
                cout << "购票成功" << endl;
                //for (auto &ch: cache[p])
                //{
                //    cout << ch << " ";
                //}
                string responce = "succ";
                tcpconn->send(responce);
             }
          } 
        }
        mysql_close(conn);
        mysql_free_result(res_ptr);
    }
    
    int getHallInfo(string &hallid, int &hallrow, int &hallcol)
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
            cout << "mysql_init failed!" << endl;
            return -1;
        }
        conn = mysql_real_connect(conn,"127.0.0.1","root","13640358","Movie", 0 ,NULL,0);
        if (conn)
        {
            cout << "Connection success!" << endl;
        }
        else 
        {
            cout << "Connection failed!" << endl;
            cout << mysql_error(conn) << endl;
            return -1;
        }
        mysql_query(conn,"set names UTF8");
        //FIXME
        string sql = "select SEAT_ROW, SEAT_COL from Table_Hall where H_ID=" + hallid;
        res = mysql_query(conn, sql.c_str());

        if(res) 
        {
            cout << mysql_error(conn) << endl;
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
                cout << "Failed to Get Hall Info" << endl;
                mysql_close(conn);
                mysql_free_result(res_ptr);
                return -1;
            }
            mysql_close(conn);
            mysql_free_result(res_ptr);
        }
    }
    
    map<pair<string, string>, vector<char> > cache GUARDED_BY(mutex_);
    MutexLock mutex_;
    ThreadPool *threadPoolptr_;
};
