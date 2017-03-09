#include "client.h"
#include "pastry.h"
#include "server.h"

using namespace std;
FILE *pipe_fp;

/* User input obtained and forwarded for parsing */
void
Client::user_menu ()
{
  time_t cur_time = time (NULL);
  printf ("%.24s : Client Module Started\n", 
          asctime (localtime (&cur_time)));
  
  while (true) {
    printf (PASTRY_PROMPT);
    string input;
    getline (cin, input);
    
    /* Transform user entered string to all lower case */
    transform (input.begin (), input.end (), input.begin (), ::tolower);
    parse (input);
  }
}

/* Set of commands available to the user */
Client::command_map_t Client::command_set[] = 
{
  make_pair ("port", &Client::port_func),
  make_pair ("ip", &Client::ip_func),
  make_pair ("create", &Client::creat_func),
  make_pair ("join", &Client::join_func),
  make_pair ("put", &Client::put_func),
  make_pair ("get", &Client::get_func),
  make_pair ("lset", &Client::lset_func),
  make_pair ("dump", &Client::rt_table_func),
  make_pair ("nset", &Client::nset_func),
  make_pair ("shutdown", &Client::shutdown_func),
  make_pair ("help", &Client::help_func)
};

/* General error handling function */
void
Client::handle_error (int errsv) 
{
  cout << "Error: ";
  if (errsv == CLIERR_STATUS)
    cout << "Incorrect sequence of commands!" << endl;
  else if (errsv == CLIERR_CMDNTFND)
    cout << "No such command found!" << endl;
  else if (errsv == CLIERR_WTOPIPE)
    cout << "Could not write to pipe!" << endl;
}

/* Parse user input string for command requested */
void
Client::parse (string input)
{
  bool not_found = true;
  stringstream token (input);
  string command;
  token >> command;
  /* Check for matching command */
  for (int i = 0; i < COMMAND_SET_SIZE; i++) {
    if (command == command_set[i].first) {
      command_func fp = command_set[i].second;
      (this->*fp) (token); 
      not_found = false;
    }
  }
  if (not_found)
    handle_error (CLIERR_CMDNTFND);
}

/* Read server module port */
void
Client::port_func (stringstream &input)
{
  if (pastry_status == INITIAL)
    input >> pastry_port;
  else
    handle_error (CLIERR_STATUS); //Not Fatal; Complain
}

/* Read server module IP */
void
Client::ip_func (stringstream &input)
{
  if (pastry_status == INITIAL) 
    input >> pastry_ip;
  else
    handle_error (CLIERR_STATUS); //Not fatal; Complain
}
        
/* Initialize server module at corresponding port and IP.
 * Server is started in a new thread.
 */
void
Client::creat_func (stringstream &input)
{
  if (pastry_status == INITIAL) {
    // Construct new node_t object
    mk_node (pastry_ip, pastry_port, &pastry_node);
    pipe (svr_cli_conn);
  	pipe_fp = fdopen (svr_cli_conn[0], "r");
  	assert (pipe_fp != NULL);
    Server s;
    // Server thread running init
    thread server (&Server::init, &s);
    server.detach();
    // Synchronization with server 
    pastry_sema.wait ();
  }
  else
    handle_error(CLIERR_STATUS);
}

void
Client::join_func(stringstream &input)
{
  string ip, port;

  /* Read a user-string key and a user-string value */
  input >> ip >> port;
  int ip_len = ip.size();
  int port_len = port.size();
  
  string msg = "add " + to_string (ip_len) + " " + 
                to_string (port_len) + "\n" + ip + port;
  
  D( printf ("join_func (): [%s %s:%d] pushing add request for"
    "joining [%s:%d] to pipe.\n", pastry_node.hexid, pastry_node.addr.ip,
    pastry_node.addr.port, ip, port); )

  write_to_pipe (svr_cli_conn[1], msg.c_str (), msg.size ());
	handle_pipe_req ();
}

/* Serves PUT request from user */
void
Client::put_func (stringstream &input)
{
  string key, value;

  /* Read a user-string key and a user-string value */
  input >> key >> value;
  int key_len = key.size ();
  int value_len = value.size ();
  
  string msg = "put " + to_string (key_len) + " " + 
                to_string (value_len) + "\n" + key + value;
  
  write_to_pipe (svr_cli_conn[1], msg.c_str (), msg.size ());
	handle_pipe_req ();
}

/* Serves GET request by user */
void
Client::get_func (stringstream &input)
{
  string key;

  /* Read a user-string key */
  input >> key;
  int key_len = key.size ();

  string msg = "get " + to_string (key_len) + " 0\n" + key;

  write_to_pipe (svr_cli_conn[1], msg.c_str (), msg.size ());
	handle_pipe_req ();
}

/* Print leaf set of current node */
void
Client::lset_func (stringstream &input)
{
  printf ("============ CW-SET =============\n");
  for (int i = 0; i < pastry_node.leafset.cw_set_sz; i++) {
    printf ("%2d.", i);
    pr_leafset_entry (pastry_node.leafset.cw_set + i);
    printf ("\n");
  }
  printf ("\n");
  printf ("============ CCW-SET =============\n");
  for (int i = 0; i < pastry_node.leafset.ccw_set_sz; i++) {
    printf ("%2d.", i);
    pr_leafset_entry (pastry_node.leafset.ccw_set + i);
    printf ("\n");
  }
}

/* Print routing table of current node */
void
Client::rt_table_func (stringstream &input)
{
  printf ("============== ROUTING-TABLE ================\n");
  for (int r = 0; r < KEYS_STR_SZ; r++) {
    printf ("ROW: %2d\n", r);
    for (int c = 0; c < BASE; c++) {
      printf ("\t%c ", (c <= 9) ? (c + '0') : (c - 10 + 'A'));
      if (pastry_node.rt_tbl[r][c].status == EMPTY)
        printf ("EMPTY");
      else
        pr_leafset_entry (&(pastry_node.rt_tbl[r][c]));
      printf ("\n");
    }
  }
  printf ("\n");
}

/* Print neighbourhood set of current node */
void
Client::nset_func (stringstream &input)
{
  int ct = 0, i;
  printf ("============== NEIGHBOURING-SET =============\n");
  for (i = 0; i < pastry_node.leafset.cw_set_sz; i++) {
    printf ("%2d.", ct + i);
    pr_leafset_entry (pastry_node.leafset.cw_set + i);
    printf ("\n");
  }
  ct += i;
  for (i = 0; i < pastry_node.leafset.ccw_set_sz; i++) {
    printf ("%2d.", ct + i);
    pr_leafset_entry (pastry_node.leafset.ccw_set + i);
    printf ("\n");
  }
}

void
Client::shutdown_func (stringstream &input)
{

}

/* General help function */
void
Client::help_func (stringstream &input)
{
  cout << "List of commands that may be entered : " << endl;
  cout << "1.  port" << endl;
  cout << "2.  ip" << endl;
  cout << "3.  create" << endl;
  cout << "4.  get" << endl;
  cout << "5.  get" << endl;
  cout << "6.  join" << endl;
  cout << "7.  lset" << endl;
  cout << "8.  nset" << endl;
  cout << "9.  dump" << endl;
  cout << "10. shutdown" << endl;
}

void 
Client::write_to_pipe (int fd, const char * buff, int len)
{
  int cw = 0, w = 0;
  while (cw < len) {
    if ((w = write (fd, buff + cw, len - cw)) < 0)
      handle_error (CLIERR_WTOPIPE);
    cw += w;
  }
}


/* This code handles all requests that were received 
 * on the pipe. It parses the received message and
 * performs the appropriate actions
 */
void
Client::handle_pipe_req ()
{
  // Parse command name
  char cmd_str[MAXLINE];
  fscanf (pipe_fp, "%s", cmd_str);

  if (!strcmp (cmd_str, "get")) {
    //Parse get packet contents
    int key_len = 0;
    fscanf (pipe_fp, "%d%*d", &key_len);
    fgetc (pipe_fp);
    char buff[key_len + 1];
    assert (fread (buff, 1, key_len, pipe_fp) == key_len);
    buff[key_len] = '\0';
    string s = string (buff), val;

    //Process get request
    int retval = get_pipe_req (s, val);
    
    if (retval == CLIERR_SUCCESS) {
      cout << val << endl;
    }
    else 
      handle_error (CLIERR_FAILURE); 

  } 
  else if (!strcmp (cmd_str, "put")) {
    //Parse put packet contents
    int key_len = 0, val_len = 0;
    fscanf (pipe_fp, "%d%d", &key_len, &val_len);
    fgetc (pipe_fp);
    string s, val;
    {
      char buff[key_len + 1];
      assert (fread (buff, 1, key_len, pipe_fp) == key_len);
      buff[key_len] = '\0';
      s = string (buff);
    }
    {
      char buff[val_len + 1];
      assert (fread (buff, 1, val_len, pipe_fp) == val_len);
      buff[val_len] = '\0';
      val = string (buff);
    }
  
    //Result of processing stored in res
    rt_tblent_t res;

    //Process put request
    int retval = put_pipe_req (s, val, res);

    if (retval == CLIERR_SUCCESS) {
      printf ("Successfully stored at : ");
      pr_leafset_entry (&res);
      printf ("\n");
    } 
    else 
      handle_error (CLIERR_FAILURE); 

  }
  else if (!strcmp (cmd_str, "add")) {
    //Parse join packet contents 
    int ip_len = 0, port_len = 0;
    fscanf (pipe_fp, "%d%d", &ip_len, &port_len);
    fgetc (pipe_fp);
    ip_t ip;
    port_t port;
    {
      char buff[ip_len + 1];
      assert (fread (buff, 1, ip_len, pipe_fp) == ip_len);
      buff[ip_len] = '\0';
      strcpy (ip, buff);
    }
    
    {
      char buff[port_len + 1];
      assert (fread (buff, 1, port_len, pipe_fp) == port_len);
      buff[port_len] = '\0';
      port = atoi (buff);
    }
  
    //Result of processing stored in res
    rt_tblent_t res;

    D ( printf("PIPE request: ADD [%s %s:%d]\n", 
        pastry_node.hexid, ip, port); )

    int retval = add_pipe_req (ip, port, res);

    if (retval == CLIERR_SUCCESS) {
      
      D( printf ("[%s %s:%d]'s ADD request succeeded\n", 
         pastry_node.hexid, pastry_node.addr.ip, pastry_node.addr.port); )
      D( printf ("\tNeighbour is [%s %s:%d]\n", hash2hex (res.id), 
         res.addr.ip, res.addr.port); )
      
      /* Send leafset-update message to all nodes in our
       * leafset */
      hashkey_t key;
      HASH_COPY (key, pastry_node.id);
      string port_temp = to_string (pastry_node.addr.port);

      D( printf ("LEAF-UPDATE: [%s %s:%d] is issuing leafset updates\n", 
         pastry_node.hexid, pastry_node.addr.ip, pastry_node.addr.port); )

      string req = string ("REQ\nLEAF\n") + to_string (BASE) + 
                    " 16 " + to_string (port_temp.size ()) + "\n";

      req.append (key, BASE);
      req.append (pastry_node.addr.ip, 16);
      req += port_temp;

      int len = req.size();

      for (int i = 0; i < pastry_node.leafset.cw_set_sz; i++) {
        int client_sockfd;
        struct sockaddr_in server_addr = {0};

        if ((client_sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
          handle_error(CLIERR_CONN);

        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = 
          inet_addr (pastry_node.leafset.cw_set[i].addr.ip);
        server_addr.sin_port = 
          htons (pastry_node.leafset.cw_set[i].addr.port);
        if (connect (client_sockfd, (struct sockaddr *) &server_addr, 
            sizeof (server_addr)) < 0)
          handle_error(CLIERR_CONN);

        write_to_socket (client_sockfd, req.c_str(), len);
        close (client_sockfd);
      }
      
      for (int i = 0; i < pastry_node.leafset.ccw_set_sz; i++) {
        int client_sockfd;
        struct sockaddr_in server_addr = {0};

        if ((client_sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
          handle_error(CLIERR_CONN);

        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = 
          inet_addr (pastry_node.leafset.ccw_set[i].addr.ip);
        server_addr.sin_port = 
          htons (pastry_node.leafset.ccw_set[i].addr.port);
        
        if (connect (client_sockfd, (struct sockaddr *) &server_addr, 
            sizeof (server_addr)) < 0)
          handle_error(CLIERR_CONN);

        write_to_socket (client_sockfd, req.c_str(), len);
        close (client_sockfd);
      }
      
      // TODO: Send hash redistribution request to neighbours
      req = string ("REQ\nHASH\n") + to_string (BASE) + 
                    " 16 " + to_string (port_temp.size ()) + "\n";

      req.append (key, BASE);
      req.append (pastry_node.addr.ip, 16);
      req += port_temp;

      len = req.size();

      for (int i = 0; i < pastry_node.leafset.cw_set_sz && i < 1; i++) {
        int client_sockfd;
        struct sockaddr_in server_addr = {0};

        if ((client_sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
          handle_error(CLIERR_CONN);

        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = 
          inet_addr (pastry_node.leafset.cw_set[i].addr.ip);
        server_addr.sin_port = 
          htons (pastry_node.leafset.cw_set[i].addr.port);
        if (connect (client_sockfd, (struct sockaddr *) &server_addr, 
            sizeof (server_addr)) < 0)
          handle_error(CLIERR_CONN);

        write_to_socket (client_sockfd, req.c_str(), len);
        close (client_sockfd);
      }
      
      for (int i = 0; i < pastry_node.leafset.ccw_set_sz && i < 1; i++) {
        int client_sockfd;
        struct sockaddr_in server_addr = {0};

        if ((client_sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
          handle_error(CLIERR_CONN);

        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = 
          inet_addr (pastry_node.leafset.ccw_set[i].addr.ip);
        server_addr.sin_port = 
          htons (pastry_node.leafset.ccw_set[i].addr.port);
        
        if (connect (client_sockfd, (struct sockaddr *) &server_addr, 
            sizeof (server_addr)) < 0)
          handle_error(CLIERR_CONN);

        write_to_socket (client_sockfd, req.c_str(), len);
        close (client_sockfd);
      }
			printf ("Getting Hash Table: Done\n");

      printf ("Joined beside : ");
      pr_leafset_entry (&res);
      printf ("\n");
    } 
    else 
      handle_error (CLIERR_FAILURE); 

  } 
  else 
    assert (0); /* Error: Request not one of add/get/put */
  
  pastry_sema.post ();
}


int 
Client::get_pipe_req (string s, string &val)
{
  int client_sockfd;
  struct sockaddr_in server_addr = {0};
  
  if ((client_sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    handle_error(CLIERR_CONN);

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr (pastry_node.addr.ip);
  server_addr.sin_port = htons (pastry_node.addr.port);
  if (connect (client_sockfd, (struct sockaddr *) &server_addr, 
      sizeof (server_addr)) < 0)
    handle_error(CLIERR_CONN);

  string req = string ("REQ\nGET\n") + to_string (s.size()) + " 0\n";
  req += s;
  
	int len = req.size();
  write_to_socket (client_sockfd, req.c_str(), len);

  shutdown (client_sockfd, SHUT_WR);

  char rbuf[MAXBUFF];
  read_from_socket (client_sockfd, rbuf);
  close (client_sockfd);
  
  int r;
  char success_code[MAXLINE], val_len_str[MAXLINE];
  sscanf (rbuf, "%*[^\n]\n%*[^\n]\n%[^\n]\n%[^\n]\n%n", 
          success_code, val_len_str, &r);

	int val_len = atoi (val_len_str);
 	
	val = string (rbuf + r, val_len);

  if (!strcmp (success_code, "SUCCESS"))
    return CLIERR_SUCCESS;
  else
    return CLIERR_FAILURE;
}

int 
Client::put_pipe_req (string s, string val, rt_tblent_t &res)
{
  int client_sockfd;
  struct sockaddr_in server_addr = {0};
  
  if ((client_sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    handle_error(CLIERR_CONN);

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr (pastry_node.addr.ip);
  server_addr.sin_port = htons (pastry_node.addr.port);
  if (connect (client_sockfd, (struct sockaddr *) &server_addr, 
      sizeof (server_addr)) < 0)
    handle_error(CLIERR_CONN);

  string req = string ("REQ\nPUT\n") + to_string (s.size()) + 
		+ " " + to_string (val.size ()) + "\n";
  req += s;
	req += val;
  
	int len = req.size();
  write_to_socket (client_sockfd, req.c_str(), len);

  shutdown (client_sockfd, SHUT_WR);

  char rbuf[MAXBUFF];
  read_from_socket (client_sockfd, rbuf);
  close (client_sockfd);
  
  int r;
  char success_code[MAXLINE], val_len_str[MAXLINE];
  sscanf (rbuf, "%*[^\n]\n%*[^\n]\n%[^\n]\n%[^\n]\n%n", 
          success_code, val_len_str, &r);
	int key_len, ip_len, port_len;
	sscanf (val_len_str, "%d%d%d", &key_len, &ip_len, &port_len);
	HASH_COPY (res.id, rbuf + r);
	memcpy (res.addr.ip, rbuf + r + key_len, ip_len);
	string port_str = string (rbuf + r + key_len + ip_len, port_len);
	res.addr.port = atoi (port_str.c_str ());

  if (!strcmp (success_code, "SUCCESS"))
    return CLIERR_SUCCESS;
  else
    return CLIERR_FAILURE;
}

/* Handles the join request initiated by the client 
 * (called by handle_pipe_req).
 * Sets res with details of the nearest neighbout discovered by the
 * join algorithm */
int 
Client::add_pipe_req (ip_t ip, port_t port, rt_tblent_t &res)
{
  int client_sockfd;
  struct sockaddr_in server_addr = {0};
  
  if ((client_sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    handle_error(CLIERR_CONN);

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr (ip);
  server_addr.sin_port = htons (port);
  if (connect (client_sockfd, (struct sockaddr *) &server_addr, 
      sizeof (server_addr)) < 0)
    handle_error(CLIERR_CONN);

  hashkey_t key;
  HASH_COPY(key, pastry_node.id);
  string port_temp = to_string (pastry_node.addr.port);
  string req = string ("REQ\nADD\n") + to_string (BASE) + " 16 " + 
    to_string (port_temp.size ()) + "\n";
  req.append (key, BASE);
  req.append (pastry_node.addr.ip, 16);
  req += port_temp;
  
  int len = req.size();
  write_to_socket (client_sockfd, req.c_str(), len);

  shutdown (client_sockfd, SHUT_WR);

  char rbuf[MAXBUFF];
  read_from_socket (client_sockfd, rbuf);
  close (client_sockfd);
  
  int r;
  char success_code[MAXLINE];
  sscanf (rbuf, "%*[^\n]\n%*[^\n]\n%[^\n]\n%*[^\n]\n%n", 
          success_code, &r);
  
  node_t node;
  deserialize (rbuf + r, &node);
  
  res = node.leafset.owner;
  cp_rt_tbl_noclobvalid (pastry_node.rt_tbl, node.rt_tbl);

  node.leafset.owner = pastry_node.leafset.owner;
  pastry_node.leafset = node.leafset;
  
  // Reorganise leafset to maintain consistency 
  add_to_leafset (&(pastry_node.leafset), &res);

  /* Caller must broadcast to all nodes in the leafset and ask them to add
   * us (the newcomer) to their leafsets and ask our neighbours 
   * to redistribute their keys */

  if (!strcmp (success_code, "SUCCESS"))
    return CLIERR_SUCCESS;
  else
    return CLIERR_FAILURE;
}

int 
Client::write_to_socket (int fd, const char * buff, int len)
{
  int cw = 0, w = 0;
  while (cw < len) {
    if ((w = write (fd, buff + cw, len - cw)) < 0)
      handle_error(CLIERR_WTOSOCK);
    cw += w;
  }
  return cw;
}

int
Client::read_from_socket (int fd, char * buff)
{
  int cr = 0, r = 0;
  while ((r = read (fd, buff + cr, MAXBUFF)) > 0)
    cr += r;
  if (r < 0)
    handle_error (CLIERR_RFROMSOCK);
  return cr;
}

