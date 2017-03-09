#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#include "util.h"
#include "pastry.h"

#define MAXBUFF          20000
#define MAXLINE            100
#define LISTEN_QUEUE         5

#define SVRERR_CONN       2006
#define SVRERR_SELECT     2007
#define SVRERR_SUCCESS    2008
#define SVRERR_FAILURE    2009
#define SVRERR_WTOSOCK    2010
#define SVRERR_RFROMSOCK  2011

/* For communication between client and server parts (threads)
 * of out program */
typedef std::map<std::string, std::string> map_t;
extern map_t pastry_store;

class Server
{
	private:
	
	void handle_error (int);
	int write_to_socket (int fd, const char * buff, int len);
	int read_from_socket (int fd, char * buff);
	void handle_sock_req (int clientfd); 
	
	int add_sock_req (hashkey_t , ip_t , port_t , char *, int *);
  int get_sock_req (string s, string &val);
	int put_sock_req (string s, string val, rt_tblent_t &res);

	public:

	void init ();

};

#endif
