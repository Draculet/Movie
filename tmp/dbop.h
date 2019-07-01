class Schedule
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
};

class DBoper
{
    public:
    static int insertMovie(string m_name, vector<Schedule> schedules);
    static int insertSeat(MYSQL *conn, int hallid, int sid);
};
