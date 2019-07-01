#include <iostream>
#include <mysql/mysql.h>
#include <vector>

class Schedule
{
    public:
    explicit Schedule(std::string name, int h_id, int price, std::string time):_name(name), hall_id(h_id), _price(price), _time(time)
    {
    }
    std::string getName(){return _name;}
    int getHallid(){return hall_id;}
    int getPrice(){return _price;}
    std::string getTime(){return _time;}
    
    private:
    std::string _name;
    int hall_id;
    int _price;
    std::string _time;
};

class DBoper
{
    public:
    static int insertMovie(std::string m_name, std::vector<Schedule> schedules);
    private:
    static int insertSeat(MYSQL *conn, int hallid, int sid);
};
