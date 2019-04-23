#include <iostream>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<mysql/mysql.h>
#include <utility>
#include <map>
#include <vector>

using namespace std;

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

string itos(int num)
{
    char tmp[20];
    for (int i = 0; i < 20; i++)
        tmp[i] = 0;
    sprintf(tmp, "%d", num);
    return string(tmp);
}


class dbCache
{
    public:
    char getCache(string hallid, string time, int row, int col)
    {
        int hallrow;
        int hallcol;
        getHallInfo(hallid, hallrow, hallcol);
        pair<string, string> p(hallid, time);
        //mutexGuard
        auto iter = cache.begin();
        if ((iter = cache.find(p)) == cache.end())
        {
            cache[p].push_back(0);
            cout << "读缓存" << endl;
            updateCache(hallid, time);
            return '2';
        }
        //endGuard
        else
        {
            //mutexGuard
            //数组第一个作为状态标记
            if (cache[p][0] == 0)
                cout << "runAfter()" << endl;
            //endmutexGuard
            else
            {   
                //mutexGuard
                if (cache[p][hallcol * (row - 1) + col] == '1')
                {
                    cout << "票已售" << endl;
                    return '1';
                }
                else
                {
                    cache[p][hallcol * (row - 1) + col] = '1';
                    cout << "购票成功" << endl;
                    for (auto &ch: cache[p])
                    {
                        cout << ch << " ";
                    }
                }
                //endmutexGuard
                return '0';
            }
        }
    }
    private:
    int updateCache(string hallid, string time)
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
                row = mysql_num_rows(res_ptr);
                for (int i = 0; i < row; i++)
                {
                    result_row = mysql_fetch_row(res_ptr);
                    cout << result_row[0] << " ";
                    cache[p].push_back(*result_row[0]);
                }
                cache[p][0] = 'k';
            }
            else
            {
                cout << "Failed to Get Seat Detail" << endl;
                mysql_close(conn);
                mysql_free_result(res_ptr);
                return -1;
            }
            cout << "缓存读取完毕" << endl;
            int count = 0;
            for (auto &ch: cache[p])
            {
                cout << ch << " ";
                count++;
            }
            cout << count << endl;
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
    
    map<pair<string, string>, vector<char> > cache;//mutexGuard
};
int main(void)
{
    string s = "2,2019-4-18 17:00,5,10";
    string s2 = "2,2019-4-18 17:00,3,1";
    string s3 = "2,2019-4-18 17:00,2,6";
    string s4 = "2,2019-4-18 17:00,1,10";
    vector<string> v = split(',', s);
    vector<string> v2 = split(',', s2);
    vector<string> v3 = split(',', s3);
    vector<string> v4 = split(',', s4);
    dbCache cache;
    cache.getCache(v[0], v[1], atoi(v[2].c_str()), atoi(v[3].c_str()));
    cache.getCache(v[0], v[1], atoi(v[2].c_str()), atoi(v[3].c_str()));
    cache.getCache(v2[0], v2[1], atoi(v2[2].c_str()), atoi(v2[3].c_str()));
    cache.getCache(v2[0], v2[1], atoi(v2[2].c_str()), atoi(v2[3].c_str()));
    cache.getCache(v3[0], v3[1], atoi(v3[2].c_str()), atoi(v3[3].c_str()));
    cache.getCache(v3[0], v3[1], atoi(v3[2].c_str()), atoi(v3[3].c_str()));
    cache.getCache(v4[0], v4[1], atoi(v4[2].c_str()), atoi(v4[3].c_str()));
    cache.getCache(v4[0], v4[1], atoi(v4[2].c_str()), atoi(v4[3].c_str()));
    
    return 0;
}
