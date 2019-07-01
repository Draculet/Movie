#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <mysql/mysql.h>
#include <iostream>
#include <string.h>
#include <vector>
#include "dbop.h"

using namespace std;

string itos(int num)
{
    char tmp[20];
    for (int i = 0; i < 20; i++)
        tmp[i] = 0;
    sprintf(tmp, "%d", num);
    return string(tmp);
}
/*class Schedule
{
    public:
    explicit Schedule(string name, int h_id, int price, string time):_name(name), hall_id(h_id), _price(price), _time(time)
    {
    }
    string getName(){return _name;}
    int getHallid(){return hall_id;}
    int getPrice(){return _price;}
    string getTime(){return _time;}
    
    private:
    string _name;
    int hall_id;
    int _price;
    string _time;
};*/

//class DBoper
//{
//    public:
    int DBoper::insertMovie(string m_name, vector<Schedule> schedules)
    {
        string toMovieHead = "INSERT INTO Table_Movie VALUE(";
        string toMovieTail = ",\'" + m_name + "\')";
        string toScheduleHead = "INSERT INTO Table_Schedule VALUE(";
        
        int res;//执行sql语句后的返回标志
        MYSQL_RES *res_ptr;//指向查询结果的指针
        MYSQL_FIELD *field;//字段结构指针
        MYSQL_ROW result_row;//按行返回查询信息
        int row,column,length;//查询返回的行数和列数
        MYSQL *conn;//一个数据库链接指针
        conn = mysql_init(NULL);
        string movieid;

        
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
        //获取Table_Movie M_id适合值
        res = mysql_query(conn,"select MAX(M_ID) from Table_Movie");
        if(res) 
        {
            cout << mysql_error(conn) << endl;
            mysql_close(conn);
            return -1;
        }
        else
        {
            res_ptr = mysql_store_result(conn);
            //插入Table_Movie
            if (res_ptr)
            {
                result_row = mysql_fetch_row(res_ptr);
                if (result_row[0] == 0)
                {
                    row = 1;
                    mysql_free_result(res_ptr);
                }
                else
                {
                    row = atoi(result_row[0]);
                    mysql_free_result(res_ptr);
                    row++;
                }
                string sql = toMovieHead + itos(row) + toMovieTail;
                cout << "WARNING:" << " " << sql << endl;
                res = mysql_query(conn, sql.c_str());
                //M_NAME已存在,重复插入会有错误
                //FIXME M_NAME unique(FIN)
                if(res)
                {
                    cout << mysql_error(conn) << endl;
                    res = mysql_query(conn,string("select M_ID from Table_Movie WHERE M_Name=\'" + m_name + "\'").c_str());
                    if(res)
                    {
                        cout << mysql_error(conn) << endl;
                        cout << "Get Movie id error "<< endl;
                        return -1;
                    }
                    res_ptr = mysql_store_result(conn);
                    if(res_ptr)
                    {
                        result_row = mysql_fetch_row(res_ptr);
                        movieid = result_row[0];
                        mysql_free_result(res_ptr);
                        cout << "Insert Error and Get Movie id Test: " << movieid << endl;
                    }
                    else
                    {
                        cout << "Failed to get Movie id"<< endl;
                        return -1;
                    }
                }
                else
                {
                    movieid = itos(row);
                }
            }
            else
            {
                cout << "Failed to Get Movie row num" << endl;
                mysql_close(conn);
                mysql_free_result(res_ptr);
                return -1;
            }
        }
        //FIXME 未处理重复插入后的M_id错误(FIN)
        
        cout << "row num " << row << endl;
        //获取Table_Schedule信息(获取目前最大的S_ID,加1即为新插入的S_ID)
        res = mysql_query(conn,"select MAX(S_ID) from Table_Schedule");
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
                 //MAX(S_ID)返回一行,返回的是null(0)
                 //cout << mysql_num_rows(res_ptr) << endl;
                 result_row = mysql_fetch_row(res_ptr);
                 if (result_row[0] == 0)
                 {
                    mysql_free_result(res_ptr);
                    row = 1;
                 }
                 else
                 {
                    row = atoi(result_row[0]);
                    mysql_free_result(res_ptr);
                    row++;
                 }
            }
            else
            {
                cout << "Failed to Get Schedule row num" << endl;
                mysql_close(conn);
                mysql_free_result(res_ptr);
                return -1;
            }
        }
        for (int i = 0; i < schedules.size(); i++)
        {
            //FIXME
            if (m_name == schedules[i].getName())
            {
                string sql = toScheduleHead + itos(row) + "," + movieid + "," + itos(schedules[i].getHallid()) + "," + itos(schedules[i].getPrice()) + ",\'" + schedules[i].getTime() + "\')";
                cout << "WARNING:" << " " << sql << endl;
                res = mysql_query(conn, sql.c_str());
                if(res) 
                {
                    cout << "Insert Schedule error" << endl;
                    cout << mysql_error(conn) << endl;
                    //row--;//插入失败先自减,防止S_ID浪费
                }
                else
                {
                    if (insertSeat(conn, schedules[i].getHallid(), row) == -1)
                    {
                        cout << "Failed to insertSeat" << endl;
                    }
                    row++;
                }
            }
        }
        mysql_close(conn);
    }
    //private:
    int DBoper::insertSeat(MYSQL *conn, int hallid, int sid)
    {
        int res;//执行sql语句后的返回标志
        MYSQL_RES *res_ptr;//指向查询结果的指针
        MYSQL_FIELD *field;//字段结构指针
        MYSQL_ROW result_row;//按行返回查询信息
        int row,column,length;//查询返回的行数和列数
        int seat_num = 0;
        int seat_row = 0;
        int seat_col = 0;
        //FIXME SEAT_ROW SEAT_COL
        res = mysql_query(conn,string("select Seat_Num, SEAT_ROW, SEAT_COL from Table_Hall where H_ID=" + itos(hallid)).c_str());
        if(res) 
        {
            cout << mysql_error(conn) << endl;
            mysql_close(conn);
            return -1;
        }
        else
        {
            res_ptr = mysql_store_result(conn);
            //插入Table_Movie
            if (res_ptr)
            {
                result_row = mysql_fetch_row(res_ptr);
                seat_num = atoi(result_row[0]);
                seat_row = atoi(result_row[1]);
                seat_col = atoi(result_row[2]);
            }
            else
            {
                cout << "Get Hall Information Error" << endl;
                mysql_close(conn);
                mysql_free_result(res_ptr);
                return -1;
            }
            //FIXME SEAT_ID显然不足,需重复利用
            //获取Seat_ID的起始值
            int beginseat;
            res = mysql_query(conn,"select MAX(Seat_ID) from Table_Seat");
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
                    if (result_row[0] == 0)
                    {
                        beginseat = 1;
                        mysql_free_result(res_ptr);
                    }
                    else
                    {
                        beginseat = atoi(result_row[0]);
                        beginseat++;
                    }
                }
                else
                {
                    cout << "Failed to Get Schedule row num" << endl;
                    mysql_close(conn);
                    mysql_free_result(res_ptr);
                    return -1;
                }
            }
            int j = 1;
            int i = 1;
            int seatid = beginseat;
            for (int n = 0; n < seat_num; n++)
            {
                string sql = "INSERT INTO Table_Seat VALUE(" + itos(seatid) + "," + itos(sid) + "," + itos(j) + "," + itos(i) + "," + itos(0) + ")";
                res = mysql_query(conn,sql.c_str());
                if(res)
                {
                    cout << mysql_error(conn) << endl;
                    mysql_close(conn);
                    cout << "INSERT Table_Seat Error" << endl;
                    return -1;
                }
                if (i % seat_col == 0)
                {
                    j++;
                    i = 1;
                    seatid++;
                }
                else
                {
                    i++;
                    seatid++;
                }
            }
        }
    }
    
    /*int insertHall(int id, string loc, int seatnum, int seatrow, int seatcol)
    {
        
    }
};*/

/*int main(void)
{
    Schedule s1("A gay life style", 1, 35, "2019-4-18 17:00");
    Schedule s2("A gay life style", 1, 35, "2019-4-18 10:00");
    Schedule s3("A gay life style", 2, 35, "2019-4-18 17:00");
    Schedule s4("A gay life style", 2, 35, "2019-4-18 10:00");
    Schedule s5("Dead Pool", 1, 35, "2019-4-19 21:00");
    Schedule s6("Dead Pool", 3, 35, "2019-4-19 21:00");
    Schedule s7("Die and Live", 1, 40, "2019-4-16 17:00");
    Schedule s8("Die and Live", 4, 40, "2019-4-16 10:00");
    Schedule s9("Die and Live", 5, 40, "2019-4-16 17:00");
    Schedule s10("Die and Live", 5, 45, "2019-4-16 10:00");
    Schedule s11("Die and Live", 6, 45, "2019-4-16 21:00");
    Schedule s12("Dead Pool", 4, 45, "2019-4-16 21:00");
    Schedule s13("A gay life style", 1, 35, "2019-4-17 21:00");
    Schedule s14("A gay life style", 3, 35, "2019-4-17 21:00");
    vector<Schedule> v;
    vector<Schedule> v2;
    vector<Schedule> v3;
    vector<Schedule> v4;
    vector<Schedule> v5;
    v.push_back(s1);
    v.push_back(s2);
    v.push_back(s3);
    v.push_back(s4);
    v4.push_back(s5);
    v4.push_back(s6);
    v2.push_back(s7);
    v2.push_back(s8);
    v2.push_back(s9);
    v2.push_back(s10);
    v2.push_back(s11);
    v5.push_back(s12);
    v3.push_back(s13);
    v3.push_back(s14);
    DBoper::insertMovie("A gay life style", v);
    DBoper::insertMovie("Die and Live", v2);
    DBoper::insertMovie("A gay life style", v3);
    DBoper::insertMovie("Dead Pool", v4);
    DBoper::insertMovie("Dead Pool", v5);
}*/
