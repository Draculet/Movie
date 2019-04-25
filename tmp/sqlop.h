#include <stdlib.h>
#include <mysql/mysql.h>
#include <utility>
#include <map>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

class dbSql
{
	public:
	dbSql(MYSQL *conn):conn_(conn)
	{
	}
	dbSql()
	{
		conn = mysql_init(NULL);
	}
	sqlQuery()
	private:
	MYSQL *conn_;
}
