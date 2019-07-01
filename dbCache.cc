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
#include <fcntl.h>
#include <sys/stat.h>
#include "codec.h"

using namespace muduo;
using namespace muduo::net;
using namespace std;

//TODO load系列函数未上锁
//TODO 数据库查询可使用另一个锁
dbCache::dbCache(ThreadPool *threadPool):
        threadPoolptr_(threadPool),
        moviePtr_(new vector<pair<string, string> >())
{
	
}
    

int dbCache::getMovieCache(const muduo::net::TcpConnectionPtr& conn, LengthHeaderCodec* codecptr)
{
	shared_ptr<vector<pair<string, string> > > localptr;
	{
		MutexLockGuard lock(mutex_[4]);
		if ((*moviePtr_).size() == 0 || (*moviePtr_)[0] == pair<string, string>("m", "m"))
		{
			if ((*moviePtr_).size() == 0)
			{
				(*moviePtr_).push_back(pair<string, string>("0", "0"));
				threadPoolptr_->run(bind(&dbCache::loadMovieCacheAndSend, this, conn, codecptr));
				return 0;
			}
			else if ((*moviePtr_)[0] == pair<string, string>("m", "m"))
			{
				(*moviePtr_)[0] = pair<string, string>("0", "0");
				//FIXME 需要先将vector掏空再push_back
				threadPoolptr_->run(bind(&dbCache::loadMovieCacheAndSend, this, conn, codecptr));
				return 0;
			}
		}
		else if ((*moviePtr_)[0] == pair<string, string>("0", "0"))
		{
			cout << "正在读取缓存getMovieCache()" << endl;
            conn->getLoop()->runAfter(1, bind(&dbCache::getMovieCache, this, conn, codecptr));
            return 1;
		}
        else if ((*moviePtr_)[0] == pair<string, string>("k", "k"))
        {
            localptr = moviePtr_;
          	//FIXME 此处可能需要使用codec_.send()
            //TODO 发送同id图片和文本
        }
    }
        	for (int i = 1; i < localptr->size(); i++)
        	{
          		string msg = (*localptr)[i].first + "-" + (*localptr)[i].second;
    			codecptr->send(conn, msg);
    			struct stat buf;
    			string picpath = (*localptr)[i].first + ".jpg";
    			stat(picpath.c_str(), &buf);
    			char rbuf[buf.st_size];
    			char *ptr = rbuf;
    			int fd = open(picpath.c_str(), O_RDONLY);
    			int remain = buf.st_size;
    			int n = read(fd, ptr, buf.st_size);
    			remain -= n;
    			while (remain > 0)
    			{
    				n = read(fd, ptr + n, remain);
    				remain -= n;
    			}
    			msg = string(rbuf, buf.st_size);
    			codecptr->send(conn, msg);
    			close(fd);
    
    			string txtpath = (*localptr)[i].first + ".txt";
   				stat(txtpath.c_str(), &buf);
    			char rbuf2[buf.st_size];
    			ptr = rbuf2;
    			fd = open(txtpath.c_str(), O_RDONLY);
    			remain = buf.st_size;
    			n = read(fd, ptr, buf.st_size);
    			remain -= n;
    			while (remain > 0)
    			{
    				n = read(fd, ptr + n, remain);
    				remain -= n;
    			}
    			msg = string(rbuf2, buf.st_size);
    			codecptr->send(conn, msg);
    			close(fd);
          	}
            string msg = "E";
            codecptr->send(conn, msg);
          	cout << "发送电影信息成功getMovieCache()" << endl;
          	return 2;
}

int dbCache::loadMovieCacheAndSend(const muduo::net::TcpConnectionPtr& tcpconn, LengthHeaderCodec* codecptr)
{
	int res;//执行sql语句后的返回标志
   	MYSQL_RES *res_ptr;//指向查询结果的指针
    MYSQL_FIELD *field;//字段结构指针
    MYSQL_ROW result_row;//按行返回查询信息
    int resrow,rescolumn,length;//查询返回的行数和列数
    MYSQL *conn;//一个数据库链接指针
    
 {
    MutexLockGuard lock(sqlmutex_);
        
        
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
        string sql = "select M_ID, M_Name from Table_Movie";
        res = mysql_query(conn, sql.c_str());
  }
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
                	MutexLockGuard lock(mutex_[4]);
                	if (!moviePtr_.unique())
                	{
                		moviePtr_.reset(new vector<pair<string, string> >(*moviePtr_));
                	}
                	for (int i = 0; i < resrow; i++)
                	{
                    	result_row = mysql_fetch_row(res_ptr);
                    //cout << result_row[0] << " ";
                    	moviePtr_->push_back(pair<string, string>(string(result_row[0]), string(result_row[1])));
                	}
                	(*moviePtr_)[0] = pair<string, string>("k", "k");
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
            cout << "缓存读取完毕loadMovieCacheAndSend()" << endl;
            //for (auto &ch: cache[p])
            //{
            //    cout << ch << " ";
            //}
          	shared_ptr<vector<pair<string, string> > > localptr;
          	{
            	MutexLockGuard lock(mutex_[4]);
            	if ((*moviePtr_)[0] == pair<string, string>("k", "k"))
            	{
            		localptr = moviePtr_;
            	}
          	}
          	//FIXME 此处可能需要使用codec_.send()
            //TODO 发送同id图片和文本
          	for (int i = 1; i < localptr->size(); i++)
          	{
          		string msg = (*localptr)[i].first + "-" + (*localptr)[i].second;
          		cout << "WAINING SEND " << msg << endl;
    			codecptr->send(tcpconn, msg);
    			struct stat buf;
    			string picpath = (*localptr)[i].first + ".jpg";
    			stat(picpath.c_str(), &buf);
    			char rbuf[buf.st_size];
    			char *ptr = rbuf;
    			int fd = open(picpath.c_str(), O_RDONLY);
    			int remain = buf.st_size;
    			int n = read(fd, ptr, buf.st_size);
    			remain -= n;
    			while (remain > 0)
    			{
    				n = read(fd, ptr + n, remain);
    				remain -= n;
    			}
    			msg = string(rbuf, buf.st_size);
    			codecptr->send(tcpconn, msg);
    			close(fd);
    			

    			string txtpath = (*localptr)[i].first + ".txt";
   				stat(txtpath.c_str(), &buf);
    			char rbuf2[buf.st_size];
    			ptr = rbuf2;
    			fd = open(txtpath.c_str(), O_RDONLY);
    			remain = buf.st_size;
    			n = read(fd, ptr, buf.st_size);
    			remain -= n;
    			while (remain > 0)
    			{
    				n = read(fd, ptr + n, remain);
    				remain -= n;
    			}
    			msg = string(rbuf2, buf.st_size);
    			cout << "WAINING SEND " << msg.size() << endl;
    			codecptr->send(tcpconn, msg);
    			close(fd);
          	}
          
          	cout << "发送电影信息成功loadScheCacheAndSend()" << endl;
          	string msg = "E";
            codecptr->send(tcpconn, msg);
            
          	mysql_close(conn);
        	mysql_free_result(res_ptr);
          	return 0;
        }
            //FIXME 若考虑管理员修改则会有其他可能	
         if ((*moviePtr_)[0] == pair<string, string>("m", "m"))
         {
            	
         }
         else
         {
            cout << "读取数据错误loadScheCacheAndSend()" << endl;
            return -1;
         }
          //}
        mysql_close(conn);
        mysql_free_result(res_ptr);
}


//TODO 需要考虑下映后不存在座位的情况
int dbCache::getSeatCache(const TcpConnectionPtr& conn, LengthHeaderCodec* codecptr, string hallid, string time, int row, int col)
{
	int hallrow = 0;
    int hallcol = 0;
    //error!!!
    getHallInfo(conn, hallid, hallrow, hallcol);//必须同步执行
    pair<string, string> p(hallid, time);
        //mutexGuard
    {
        MutexLockGuard lock(seatmutex_);
        auto iter = seatCache_.begin();
        iter = seatCache_.find(p);
        if (iter == seatCache_.end())
        {
            seatCache_[p].push_back(0);
            cout << "读缓存getSeatCache()" << endl;
            //TODO 读取缓存,查询状态,回调?或者使用runAfter()
             threadPoolptr_->run(bind(&dbCache::loadSeatCacheAndSend, this, conn, codecptr, hallid, time, hallrow, hallcol, row, col));
            //runAfter(1, bind(&dbCache::getCache, this, hallid, time, row, col));
            return 0;
        }
        else
        {
            //数组第一个作为状态标记
            if (seatCache_[p][0] == 0)
            {
                cout << "正在读取缓存getSeatCache()" << endl;
                conn->getLoop()->runAfter(1, bind(&dbCache::getSeatCache, this, conn, codecptr, hallid, time, row, col));
                return 1;
            }
            //TODO 判断seatCache_[p][0]=='k'
            else
            {   
            	if (row == 0 && col == 0)
            	{
            		string responce;
            		//先发一行让对端知道列数
            		for (int i = 0; i < hallcol; i++)
            			responce += seatCache_[p][i];
            		codecptr->send(conn, responce);
            		//为方便，将所有座位信息发送
            		responce = "";
            		for (auto ch: seatCache_[p])
            			responce += ch;
            		codecptr->send(conn, responce);
            		
            		return 2;
            	}
                else if (seatCache_[p][hallcol * (row - 1) + col] == '1')
                {
                    cout << "票已售getSeatCache()" << endl;
                    string responce = "fail";
                    //TODO 优化:将发送移出临界区(send如果在本线程调用则会同步执行,延长缓冲区)
                    conn->send(responce);
                    return 3;
                }
                else if (seatCache_[p][hallcol * (row - 1) + col] == '0')
                {
                    seatCache_[p][hallcol * (row - 1) + col] = '1';
                    cout << "购票成功getSeatCache()" << endl;
                    string responce = "succ " + hallid + " " + time + " " + to_string(row) + " " + to_string(col) + "getSeatCache()" + to_string(hallrow) + to_string(hallcol);
                    //TODO 优化:将发送移出临界区(send如果在本线程调用则会同步执行,延长缓冲区)
                    conn->send(responce);
                    return 4;
                }
                else
                {
                	string responce = "error ";
                	conn->send(responce);
                	return 5;
                }
            }
        }
    }
}

int dbCache::loadSeatCacheAndSend(TcpConnectionPtr& tcpconn, LengthHeaderCodec* codecptr, string hallid, string time, int hallrow, int hallcol, int row, int col)
{
    int res;//执行sql语句后的返回标志
   	MYSQL_RES *res_ptr;//指向查询结果的指针
    MYSQL_FIELD *field;//字段结构指针
    MYSQL_ROW result_row;//按行返回查询信息
    int resrow,rescolumn,length;//查询返回的行数和列数
    MYSQL *conn;//一个数据库链接指针
    
    {
        MutexLockGuard lock(sqlmutex_);
        
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

	}
	
		pair<string, string> p(hallid, time);
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
                	MutexLockGuard lock(seatmutex_);
                	
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
  		}
            //for (auto &ch: cache[p])
            //{
            //    cout << ch << " ";
            //}
    	{
        	MutexLockGuard lock(seatmutex_);
        	
        	if (seatCache_[p][0] == 'k')
            {
            	if (row == 0 && col == 0)
            	{
            		string responce;
            		//先发一行让对端知道列数
            		for (int i = 0; i < hallcol; i++)
            			responce += seatCache_[p][i];
            		codecptr->send(tcpconn, responce);
            		//为方便，将所有座位信息发送
            		responce = "";
            		for (auto ch: seatCache_[p])
            			responce += ch;
            		codecptr->send(tcpconn, responce);
            	}
            	if (seatCache_[p][hallcol * (row - 1) + col] == '1')
            	{
                	cout << "票已售loadSeatCacheAndSend()" << endl;
                	string responce = "fail";
                	tcpconn->send(responce);
            	}
            	else if (seatCache_[p][hallcol * (row - 1) + col] == '0')
           		{
                	seatCache_[p][hallcol * (row - 1) + col] = '1';
                	cout << "购票成功loadSeatCacheAndSend()" << endl;
                //for (auto &ch: cache[p])
                //{
                //    cout << ch << " ";
                //}
                	string responce = "succ " + hallid + " " + time + " " + to_string(row) + " " + to_string(col) + "loadSeatCacheAndSend()" + to_string(hallrow) + to_string(hallcol);
                	tcpconn->send(responce);
             	}
             	else
             	{
             		string responce = "error ";
                	tcpconn->send(responce);
             	}
            }
            else
            {
            	//something else
            }
      }
      mysql_close(conn);
      mysql_free_result(res_ptr);
}


int dbCache::getHallInfo(const TcpConnectionPtr& conn, string& hallid, int& hallrow, int& hallcol)
{
   	{
    	MutexLockGuard lock(mutex_[1]);
    	auto iter = hallCache_.begin();
    	iter = hallCache_.find(hallid);
    	if (iter == hallCache_.end())
    	{
         	cout << "读缓存getHallInfo()" << endl;
        	//TODO 读取缓存,查询状态,回调?或者使用runAfter()
        	loadHallCacheWithLock(hallid, hallrow, hallcol);//同步执行
        	//runAfter(1, bind(&dbCache::getCache, this, hallid, time, row, col));
        	return 1;
		}
		else
        {
            if (hallCache_[hallid].first > 0 && hallCache_[hallid].second > 0)
            {
                hallrow = hallCache_[hallid].first;
                hallcol = hallCache_[hallid].second;
                cout << "直接读取缓存getHallInfo()" << endl;
                return 2;
            }
            else
            {
               cout << "缓存数据错误getHallInfo()" << endl;
                return -1;
            }
        }
    }
}


int dbCache::loadHallCacheWithLock(std::string& hallid, int& hallrow, int& hallcol)//FIXME MUTEX
{
		int res;//执行sql语句后的返回标志
    	MYSQL_RES *res_ptr;//指向查询结果的指针
        MYSQL_FIELD *field;//字段结构指针
        MYSQL_ROW result_row;//按行返回查询信息
        int row,column,length;//查询返回的行数和列数
        MYSQL *conn;//一个数据库链接指针
        
        {
        	MutexLockGuard lock(sqlmutex_);
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
        	string sql = "select SEAT_ROW, SEAT_COL from Table_Hall where H_ID=" + hallid;
        	res = mysql_query(conn, sql.c_str());
		}
		
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
                	if (hallrow > 0 && hallcol > 0)
                	{
                		hallCache_[hallid].first = hallrow;
                		hallCache_[hallid].second = hallcol;
                	}
                	else
                	{
                		hallrow = -1;
                		hallcol = -1;
                		cout << "读取缓存数据时,缓存数据错误getHallInfo()" << endl;
                		return -1;
                	}
                	cout << "缓存读取完成loadHallCache()" << endl;
            	}
            	else
            	{
                	cout << "Failed to Get Hall Info" << endl;
                	mysql_close(conn);
                	mysql_free_result(res_ptr);
                	return -1;
            	}
            }
            mysql_close(conn);
            mysql_free_result(res_ptr);
}


int dbCache::getScheCache(const muduo::net::TcpConnectionPtr& conn, std::string movie)
{
	{
    	MutexLockGuard lock(mutex_[2]);
    	auto iter = scheCache_.begin();
    	iter = scheCache_.find(movie);
    	if (iter == scheCache_.end())
    	{
       		 //mutex
         	scheCache_[movie].push_back("0");
         	//endmutex
         	cout << "读缓存getScheCache()" << endl;
        	//TODO 读取缓存,查询状态,回调?或者使用runAfter()
        	threadPoolptr_->run(bind(&dbCache::loadScheCacheAndSend, this, conn, movie));
        	//runAfter(1, bind(&dbCache::getCache, this, hallid, time, row, col));
        	return 0;
		}
		else
        {
            //数组第一个作为状态标记
            if (scheCache_[movie][0] == string("0"))
            {
                cout << "正在读取缓存getScheCache()" << endl;
                conn->getLoop()->runAfter(1, bind(&dbCache::getScheCache, this, conn, movie));
                return 1;
            }
            else if (scheCache_[movie][0] == "k")
            {
            	string tmp;
            	int i;
            	for (i = 1; i < scheCache_[movie].size() - 1; i++)
            	{
            		tmp += scheCache_[movie][i];
            		tmp += ",";
            	}
            	tmp += scheCache_[movie][i];
            	conn->send(tmp);
            	cout << "读取排片时间成功getScheCache()" << endl;
            	return 2;
            }
            else
            {
                cout << "读取数据错误getScheCache()" << endl;
               	return -1;
            }
        }
     }
}

int dbCache::loadScheCacheAndSend(muduo::net::TcpConnectionPtr& tcpconn, std::string& movie)
{
	int res;//执行sql语句后的返回标志
   	MYSQL_RES *res_ptr;//指向查询结果的指针
    MYSQL_FIELD *field;//字段结构指针
    MYSQL_ROW result_row;//按行返回查询信息
    int resrow,rescolumn,length;//查询返回的行数和列数
    MYSQL *conn;//一个数据库链接指针
    
  {
    MutexLockGuard lock(sqlmutex_);
        
        
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
        string sql = "select distinct S_TIME from Table_Schedule where M_ID in(select M_ID from Table_Movie where M_Name='" + movie + "') order by S_TIME";
        res = mysql_query(conn, sql.c_str());
  }
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
                	MutexLockGuard lock(mutex_[2]);
                	
                	for (int i = 0; i < resrow; i++)
                	{
                    	result_row = mysql_fetch_row(res_ptr);
                    //cout << result_row[0] << " ";
                    	scheCache_[movie].push_back(result_row[0]);
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
            MutexLockGuard lock(mutex_[2]);
            if (scheCache_[movie][0] == "k")
            {
            	//FIXME 此处可能需要使用codec_.send()
            	string tmp;
            	int i;
            	for (i = 1; i < scheCache_[movie].size() - 1; i++)
            	{
            		tmp += scheCache_[movie][i];
            		tmp += ",";
            	}
            	tmp += scheCache_[movie][i];
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

int dbCache::getHallchoiceCache(const muduo::net::TcpConnectionPtr& conn, std::string movie, std::string time)
{
	pair<string, string> p(movie, time);
    {
        MutexLockGuard lock(mutex_[3]);
        auto iter = hallchoiceCache_.begin();
        iter = hallchoiceCache_.find(p);
        if (iter == hallchoiceCache_.end())
        {
            //mutex
            hallchoiceCache_[p].push_back("0");
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
            if (hallchoiceCache_[p][0] == string("0"))
            {
                cout << "正在读取缓存getHallchoiceCache()" << endl;
                conn->getLoop()->runAfter(1, bind(&dbCache::getHallchoiceCache, this, conn, movie, time));
                return 1;
            }
            else if (hallchoiceCache_[p][0] == "k")
            {   
                string tmp;
            	for (int i = 1; i < hallchoiceCache_[p].size(); i++)
            	{
            		tmp += hallchoiceCache_[p][i];
            		tmp += " ";
            	}
            	conn->send(tmp);
            	cout << "读取指定时间内可选厅成功getHallchoiceCache()" << endl;
            	return 2;
            }
            else
            {
                cout << "读取数据错误getHallchoiceCache()" << endl;
               	return -1;
            }
        }
    }
}

int dbCache::loadHallChoiceAndSend(muduo::net::TcpConnectionPtr& tcpconn, std::string& movie, std::string& time)
{
	int res;//执行sql语句后的返回标志
   	MYSQL_RES *res_ptr;//指向查询结果的指针
    MYSQL_FIELD *field;//字段结构指针
    MYSQL_ROW result_row;//按行返回查询信息
    int resrow,rescolumn,length;//查询返回的行数和列数
    MYSQL *conn;//一个数据库链接指针
    
  {
    MutexLockGuard lock(sqlmutex_);
    
    
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
  }
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
                	MutexLockGuard lock(mutex_[3]);
                	for (int i = 0; i < resrow; i++)
                	{
                    	result_row = mysql_fetch_row(res_ptr);
                    //cout << result_row[0] << " ";
                    	hallchoiceCache_[p].push_back(result_row[0]);
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
            MutexLockGuard lock(mutex_[3]);
            if (hallchoiceCache_[p][0] == "k")
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
