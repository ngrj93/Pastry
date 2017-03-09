#ifndef CLIENT_H_INCLUDED
#define CLIENT_H_INCLUDED

#include "util.h"
#include "leafset.h"

#define COMMAND_SET_SIZE 11
#define CLIERR_STATUS 1005
#define CLIERR_CMDNTFND 1006
#define CLIERR_WTOPIPE 1007

#define MAXBUFF          20000
#define MAXLINE            100
#define LISTEN_QUEUE         5

#define CLIERR_CONN       2006
#define CLIERR_SELECT     2007
#define CLIERR_SUCCESS    2008
#define CLIERR_FAILURE    2009
#define CLIERR_WTOSOCK    2010
#define CLIERR_RFROMSOCK  2011

extern FILE *pipe_fp;

class Client
{
	private:

	typedef void (Client::*command_func)(std::stringstream &);
	typedef const std::pair<std::string, command_func > command_map_t;
	static command_map_t command_set[COMMAND_SET_SIZE];
	void parse(std::string);
	void port_func(std::stringstream &);
	void ip_func(std::stringstream &);
	void creat_func(std::stringstream &);
	void join_func(std::stringstream &);
	void put_func(std::stringstream &);
	void get_func(std::stringstream &);
	void lset_func(std::stringstream &);
	void rt_table_func(std::stringstream &);
	void nset_func(std::stringstream &);
	void shutdown_func(std::stringstream &);
	void help_func(std::stringstream &);
	void handle_error(int);
	void write_to_pipe(int, const char *, int);
	
	void handle_pipe_req ();
	int put_pipe_req (string s, string val, rt_tblent_t &res);
  int get_pipe_req (string s, string &val);
	int add_pipe_req (ip_t ip, port_t port, rt_tblent_t &res);
	int write_to_socket (int fd, const char * buff, int len);
	int read_from_socket (int fd, char * buff);
	
	public:

	void user_menu ();
	
};


#endif

	


