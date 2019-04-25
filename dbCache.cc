#include "muduo/base/Atomic.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Thread.h"
#include "muduo/base/ThreadPool.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpServer.h"

#include "dbCache.h"
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

//TODO load系列函数未上锁

dbCache::dbCache(ThreadPool *threadPool):
        threadPoolptr_(threadPool)
{
}
    
//TODO 需要考虑下映后不存在座位的情况
int dbCache::getSeatCache(const TcpConnectionPtr& conn, string hallid, string time, int row, int col)
{
	int hallrow;
    int hallcol;
    getHallInfo(hallid, hallrow, hallcol);
    pair<string, string> p(hallid, time);
        //mutexGuard
    {
        MutexLockGuard lock(mutex_);
        auto iter = seatCache_.begin();
        iter = seatCache_.find(p);
        if (iter == seatCache_.end())
        {
            //mutex
            seatCache_[p].push_back(0);
            //endmutex
            cout << "读缓存getSeatCache()" << endl;
            //TODO 读取缓存,查询状态,回调?或者使用runAfter()
             threadPoolptr_->run(bind(&dbCache::loadCacheAndSend, this, conn, hallid, time, hallrow, hallcol, row, col));
            //runAfter(1, bind(&dbCache::getCache, this, hallid, time, row, col));
            return 0;
        }
        else
        {
            //数组第一个作为状态标记
            if (seatCache_[p][0] == 0)
            {
                cout << "正在读取缓存getSeatCache()" << endl;
                conn->getLoop()->runAfter(1, bind(&dbCache::getSeatCache, this, conn, hallid, time, row, col));
                return 1;
            }
            //TODO 判断seatCache_[p][0]=='k'
            else
            {   
                if (seatCache_[p][hallcol * (row - 1) + col] == '1')
                {
                    cout << "票已售getSeatCache()" << endl;
                    string responce = "fail";
                    conn->send(responce);
                    return 2;
                }
                else
                {
                    seatCache_[p][hallcol * (row - 1) + col] = '1';
                    cout << "购票成功getSeatCache()" << endl;
                    string responce = "succ";
                    conn->send(responce);
                    return 3;
                }
            }
        }
    }
}

int dbCache::loadSeatCacheAndSend(TcpConnectionPtr& tcpconn, string hallid, string time, int hallrow, int hallcol, int row, int col)
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
        conn = mysql_real_connect(conn,"127.0.0.1","root","335369376","Movie", 0 ,NULL,0);
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
                    seatCache_[p].push_back(*result_row[0]);
                }
                seatCache_[p][0] = 'k';
              }
            }
            else
            {
                cout << "Failed to Get Seat Detail" << endl;
                mysql_close(conn);
                mysql_free_result(res_ptr);
                return -1;
            }
            cout << "缓存读取完毕loadCacheAndSend()" << endl;
            //for (auto &ch: cache[p])
            //{
            //    cout << ch << " ";
            //}
          {
            MutexLockGuard lock(mutex_);
            if (seatCache_[p][0] == 'k')
            {
            	if (seatCache_[p][hallcol * (row - 1) + col] == '1')
            	{
                	cout << "票已售loadCacheAndSend()" << endl;
                	string responce = "fail";
                	tcpconn->send(responce);
            	}
            	else
           		{
                	seatCache_[p][hallcol * (row - 1) + col] = '1';
                	cout << "购票成功loadCacheAndSend()" << endl;
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

int loadHallCache(std::string& hallid, int& hallrow, int& hallcol)//FIXME MUTEX
{
		int res;//执行sql语句后的返回标志
    	MYSQL_RES *res_ptr;//指向查询结果的指针
        MYSQL_FIELD *field;//字段结构指针
        MYSQL_ROW result_row;//按行返回查询信息
        int row,column,length;//查询返回的行数和列数
        MYSQL *conn;//一个数据库链接指针
        conn = mysql_init(NULL);
        int hallrow = 0;
        int hallcol = 0;

        if(conn == NULL) 
        { //如果返回NULL说明初始化失败
            cout << "mysql_init failed!" << endl;
            return -1;
        }
        conn = mysql_real_connect(conn,"127.0.0.1","root","335369376","Movie", 0 ,NULL,0);
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
                hallCache_[hallid] = pair<int, int>(hallrow, hallcol);
                cout << "缓存读取完成loadHallCache()" << endl;
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
            {
            	MutexLockGuard lock(mutex_);
            	if (hallCache_[hallrow].first > 0 && hallCache_[hallcol].second > 0)
            	{
                	hallrow = hallCache_[hallid].first;
                	hallcol = hallCache_[hallid].second;
                }
                else
                {
                	cout << "读取数据错误getHallInfo()" << endl;
                	return -1;
                }
            }
        }
}
    
int dbCache::getHallInfo(string& hallid, int& hallrow, int& hallcol)
{
   	{
    	MutexLockGuard lock(mutex_);
    	auto iter = hallCache_.begin();
    	iter = hallCache_.find(hallid);
    	if (iter != hallCache_.end())
    	{
       		 //mutex
         	hallCache_[hallid] = pair<int, int>(-1,-1);
         	//endmutex
         	cout << "读缓存getHallInfo()" << endl;
        	//TODO 读取缓存,查询状态,回调?或者使用runAfter()
        	threadPoolptr_->run(bind(&dbCache::loadHallCache, this, hallid, hallrow, hallcol));
        	//runAfter(1, bind(&dbCache::getCache, this, hallid, time, row, col));
        	return 0;
		}
		else
        {
            //数组第一个作为状态标记
            if (hallCache_[hallid] == pair<int, int>(-1,-1))
            {
                cout << "正在读取缓存getHallInfo()" << endl;
                conn->getLoop()->runAfter(1, bind(&dbCache::getHallInfo, this, hallid, hallrow, hallcol));
                return 1;
            }
            else
            {   
            	if (hallCache_[hallrow].first > 0 && hallCache_[hallcol].second > 0)
            	{
                	hallrow = hallCache_[hallid].first;
                	hallcol = hallCache_[hallid].second;
                	return 2;
                }
                else
                {
                	cout << "读取数据错误getHallInfo()" << endl;
                	return -1;
                }
            }
         }
      }
}

int dbCache::getScheCache(const muduo::net::TcpConnectionPtr& conn, std::string movie)
{
	{
    	MutexLockGuard lock(mutex_);
    	auto iter = scheCache_.begin();
    	iter = scheCache_.find(movie);
    	if (iter != scheCache_.end())
    	{
       		 //mutex
         	scheCache_[movie].push_back(0);
         	//endmutex
         	cout << "读缓存getScheCache()" << endl;
        	//TODO 读取缓存,查询状态,回调?或者使用runAfter()
        	threadPoolptr_->run(bind(&dbCache::loadScheCacheAndSend, this, movie));
        	//runAfter(1, bind(&dbCache::getCache, this, hallid, time, row, col));
        	return 0;
		}
		else
        {
            //数组第一个作为状态标记
            if (scheCache_[movie][0] == 0)
            {
                cout << "正在读取缓存getScheCache()" << endl;
                conn->getLoop()->runAfter(1, bind(&dbCache::getScheCache, this, movie));
                return 1;
            }
            else if (scheCache_[movie][0] == 'k')
            {
            	string tmp;
            	for (int i = 1; i < scheCache_[movie].size(); i++)
            	{
            		tmp += scheCache_[movie][i];
            		tmp += " ";
            	}
            	conn->send(tmp);
            	cout << "读取排片时间成功" << endl;
            }
            else
            {
                cout << "读取数据错误getScheCache()" << endl;
               	return -1;
            }
        }
     }
}

int loadScheCacheAndSend(muduo::net::TcpConnectionPtr& tcpconn, std::string& movie)
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
        conn = mysql_real_connect(conn,"127.0.0.1","root","335369376","Movie", 0 ,NULL,0);
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
        string sql = "select distinct S_TIME from Table_Schedule where M_ID in(select M_ID from Table_Movie where M_Name='" + movie + "')";
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
                resrow = mysql_num_rows(res_ptr);
              {
                MutexLockGuard lock(mutex_);
                for (int i = 0; i < resrow; i++)
                {
                    result_row = mysql_fetch_row(res_ptr);
                    //cout << result_row[0] << " ";
                    scheCache_[movie].push_back(*result_row[0]);
                }
                scheCache_[movie][0] = 'k';
              }
            }
            else
            {
            	//FIXME 细化此处报错
                cout << "Failed to Get Detail" << endl;
                mysql_close(conn);
                mysql_free_result(res_ptr);
                return -1;
            }
            cout << "缓存读取完毕loadScheCacheAndSend()" << endl;
            //for (auto &ch: cache[p])
            //{
            //    cout << ch << " ";
            //}
          {
            MutexLockGuard lock(mutex_);
            if (scheCache_[movie][0] == 'k')
            {
            	//FIXME 此处可能需要使用codec_.send()
            	string tmp;
            	for (int i = 1; i < scheCache_[movie].size(); i++)
            	{
            		tmp += scheCache_[movie][i];
            		tmp += " ";
            	}
            	tcpconn->send(tmp);
            	cout << "读取排片时间成功loadScheCacheAndSend()" << endl;
            }
            //FIXME 若考虑管理员修改则会有其他可能	
            else
            {
                cout << "读取数据错误loadScheCacheAndSend()" << endl;
               	return -1;
            }
          } 
        }
        mysql_close(conn);
        mysql_free_result(res_ptr);
}

int getHallchoiceCache(const muduo::net::TcpConnectionPtr& tcpconn, std::string movie, std::string time)
{
	pair<string, string> p(movie, time);
    {
        MutexLockGuard lock(mutex_);
        auto iter = hallchoiceCache_.begin();
        iter = hallchoiceCache_.find(p);
        if (iter == hallchoiceCache_.end())
        {
            //mutex
            hallchoiceCache_[p].push_back(0);
            //endmutex
            cout << "读缓存getHallchoiceCache()" << endl;
            //TODO 读取缓存,查询状态,回调?或者使用runAfter()
             threadPoolptr_->run(bind(&dbCache::loadHallChoiceAndSend, this, conn, movie, time));
            //runAfter(1, bind(&dbCache::getCache, this, hallid, time, row, col));
            return 0;
        }
        else
        {
            //数组第一个作为状态标记
            if (hallchoiceCache_[p][0] == 0)
            {
                cout << "正在读取缓存getHallchoiceCache()" << endl;
                conn->getLoop()->runAfter(1, bind(&dbCache::getHallchoiceCache, this, conn, movie, time));
                return 1;
            }
            else if (hallchoiceCache_[p][0] == 'k')
            {   
                string tmp;
            	for (int i = 1; i < hallchoiceCache_[p].size(); i++)
            	{
            		tmp += hallchoiceCache_[p][i];
            		tmp += " ";
            	}
            	tcpconn->send(tmp);
            	cout << "读取指定时间内可选厅成功getHallchoiceCache()" << endl;
            }
            else
            {
                cout << "读取数据错误getHallchoiceCache()" << endl;
               	return -1;
            }
        }
    }
}

int loadHallChoiceAndSend(muduo::net::TcpConnectionPtr& tcpconn, std::string& movie, std::string& time)
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
        conn = mysql_real_connect(conn,"127.0.0.1","root","335369376","Movie", 0 ,NULL,0);
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
        string sql = "select H_ID from Table_Schedule where S_TIME='" + time + "' and M_ID=(select M_ID from Table_Movie where M_Name='" + movie + "');";
        res = mysql_query(conn, sql.c_str());
        
		pair<string, string> p(movie, time);
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
                resrow = mysql_num_rows(res_ptr);
              {
                MutexLockGuard lock(mutex_);
                for (int i = 0; i < resrow; i++)
                {
                    result_row = mysql_fetch_row(res_ptr);
                    //cout << result_row[0] << " ";
                    hallchoiceCache_[p].push_back(*result_row[0]);
                }
                hallchoiceCache_[p][0] = 'k';
              }
            }
            else
            {
                cout << "Failed to Get Detail" << endl;
                mysql_close(conn);
                mysql_free_result(res_ptr);
                return -1;
            }
            cout << "缓存读取完毕loadHallChoiceAndSend()" << endl;
            //for (auto &ch: cache[p])
            //{
            //    cout << ch << " ";
            //}
          {
            MutexLockGuard lock(mutex_);
            if (hallchoiceCache_[p][0] == 'k')
            {
            	//FIXME 此处可能需要使用codec_.send()
            	string tmp;
            	for (int i = 1; i < hallchoiceCache_[p].size(); i++)
            	{
            		tmp += hallchoiceCache_[p][i];
            		tmp += " ";
            	}
            	tcpconn->send(tmp);
            	cout << "读取厅可选项成功loadHallChoiceAndSend()" << endl;
            }
            //FIXME 若考虑管理员修改则会有其他可能	
            else
            {
                cout << "读取数据错误loadHallChoiceAndSend()" << endl;
               	return -1;
            }
          }
        }
        mysql_close(conn);
        mysql_free_result(res_ptr);
}
