#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<mysql/mysql.h>
#include <iostream>
#include <string.h>
using namespace std; 
int main(void)
{
  //char *sql;
  //sql="INSERT INTO Table_Order VALUE(1001,104,0012,10000)";
  //string sqlhead = "INSERT INTO Table_Order VALUE(";
  //string sqlend = ",104,0012,10000)";
  string sql = "SELECT * FROM Table_Ord";
  //string sql="INSERT INTO Table_Order VALUE(110,110,110,110)";
  int res;//执行sql语句后的返回标志
  MYSQL_RES *res_ptr;//指向查询结果的指针
  MYSQL_FIELD *field;//字段结构指针
  MYSQL_ROW result_row;//按行返回查询信息
  int row,column,length;//查询返回的行数和列数
  MYSQL *conn;//一个数据库链接指针
  int i,j;
 
  //初始化连接句柄
  conn = mysql_init(NULL);
 
  if(conn == NULL) { //如果返回NULL说明初始化失败
    printf("mysql_init failed!\n");
    return EXIT_FAILURE;
  }
 
  //进行实际连接
  //参数　conn连接句柄，host　mysql所在的主机或地址,user用户名,passwd密码,database_name数据库名,后面的都是默认
  conn = mysql_real_connect(conn,"127.0.0.1","root","335369376","Movie", 0 ,NULL,0);
  if (conn) {
    printf("Connection success!\n");
  } else {
    printf("Connection failed!\n");
    cout << mysql_error(conn) << endl;
    return -1;
  }
  mysql_query(conn,"set names UTF8");//防止乱码。设置和数据库的编码一致就不会乱码
  //for (int i = 0; i < 10; i++)
  //{
        //char ch;
        //sprintf(&ch, "%d", i);
        //string str = sqlhead + ch + sqlend;
        //cout << str << endl;
        //cout << sql << endl;
        res = mysql_query(conn,sql.c_str());//正确返回0
    //    break;
  //}
  //INSERT,SELECT等失败时res!=0
  //INSERT成功res==0,但没有返回
  if(res) {
    perror("my_query");
    cout << mysql_error(conn) << endl;
    mysql_close(conn);
    exit(0);
  } 
  else{
    //把查询结果给res_ptr
    //对于其他查询，不需要调用mysql_store_result()或mysql_use_result()，但是如果在任何情况下均调用了mysql_store_result()，它也不会导致任何伤害或性能降低。通过检查mysql_store_result()是否返回0，可检测查询是否没有结果
    res_ptr = mysql_store_result(conn);
    //如果结果不为空,则输出
    //如果希望了解查询是否应返回结果集，可使用mysql_field_count()进行检查。mysql_store_result()将查询的全部结果读取到客户端，分配1个MYSQL_RES结构，并将结果置于该结构中。如果查询未返回结果集，mysql_store_result()将返回Null指针（例如，如果查询是INSERT语句）
    
    if(res_ptr) {
      //cout << mysql_error(conn) << endl;
      column = mysql_num_fields(res_ptr);
      //一旦调用了mysql_store_result()并获得了不是Null指针的结果，可调用mysql_num_rows()来找出结果集中的行数。可以调用mysql_fetch_row()来获取结果集中的行，或调用mysql_row_seek()和mysql_row_tell()来获取或设置结果集中的当前行位置。
      //一旦完成了对结果集的操作，必须调用mysql_free_result()。
      // 返回具有多个结果的MYSQL_RES结果集合。如果出现错误，返回NULL。
      
      //@FROM https://blog.csdn.net/bwmwm/article/details/6080160 当执行insert语句的时候，是没有结果返回的，因此列的个数为0，且mysql_store_result返回NULL。因此可以通过mysql_field_count()是否返回0来判断是否有结果返回，而不需要执行mysql_store_result来判断是否返回了NULL。我想，mysql_field_count()的效率肯定要比mysql_store_result()高。
//在这种情况下，由于没有返回结果，因此mysql_store_result()返回NULL，也就是得不到res指针，于是mysql_num_fields()函数就无法执行，缺少必要的参数。
//当执行第一条select语句的时候，返回了结果，因此mysql_field_count()和mysql_num_fields()都返回了正确的列的个数2，mysql_num_rows()返回了记录的条数7.
//当执行第二条select语句，由于表中没有 id = 0 的记录，因此mysql_num_rows返回了0表示记录数为0，但是，我们发现mysql_store_result()并没有返回NULL，mysql_num_fields()和mysql_field_count()还是返回了2.
//因此我们可以得出这样的结论：执行结果有三种情况，第一是执行insert、update和delete这样的语句的时候，是不会有任何内容返回，因此mysql_store_result()会返回一个NULL。第二，执行select或show这样的语句时，一定会有内容返回，可以取得列信息，但是记录可以为0，也可以不为0。这就像一个表，表头一定存在，但是表中可以没有数据。

      row = mysql_num_rows(res_ptr);
      printf("查到%d行\n",row);
      //输出结果的字段名
      field = mysql_fetch_field(res_ptr);
      for(i = 0;i < column;i++) {
        printf("%20s",field[i].name);
        cout << " " << field[i].type;
      }
      puts("");
      //按行输出结果
      for(i = 0;i < row ;i++){
        result_row = mysql_fetch_row(res_ptr);
        for(j = 0;j< column; j++) {
          printf("%20s",result_row[j]);
          //cout << "结果长度: " << mysql_fetch_lengths(res_ptr)[j] << endl;
        }
        puts("");
      }
    }
    //释放结果集
    mysql_free_result(res);
  }
  //退出前关闭连接
  
  mysql_close(conn);
 
  return 0;
}
