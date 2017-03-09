#include "util.h"
#include "pastry.h"
#include "server.h"
#include "routing.h"
#include "leafset.h"

using namespace std;
map_t pastry_store;

/* Server side error handling */
void 
Server::handle_error (int errsv) 
{
  cout << "Error: ";
  if (errsv == SVRERR_CONN)
    cout << "Server connection could not be established!" << endl;
  else if (errsv == SVRERR_SELECT)
    cout << "Select function failure!" << endl;
  else if (errsv == SVRERR_FAILURE)
    cout << "Request could not be served!" << endl;
}

/* Listening server initialized and waits on requests 
 * made by the attached client as well as other nodes
 */
void
Server::init ()
{
  /* socket descriptor set corresponding to internal client 
   * and external connections */
  
  D (printf ("Server Started\n");)
  fd_set current_fd_set, read_fd_set;

  //establish server socket connection
  int server_sockfd;
  struct sockaddr_in server_addr = {0};

  if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    handle_error(SVRERR_CONN);

  //FD_ZERO (&current_fd_set);
  //FD_SET (server_sockfd, &current_fd_set);
  //FD_SET (svr_cli_conn[0], &current_fd_set);

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr (pastry_ip);
  server_addr.sin_port = htons (pastry_port);

  if (bind (server_sockfd, (struct sockaddr *) &server_addr, 
      sizeof (server_addr)) < 0)
    handle_error (SVRERR_CONN);

  listen (server_sockfd, LISTEN_QUEUE);

  /* notify client that the server has started and allow the client 
   * to continue independently of the server 
   */
  pastry_status = SERVER_STARTED;
  pastry_sema.post ();


  while (true) {
    //read_fd_set = current_fd_set;

    /* block until either a client or external request is received */  
    //if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
    //  handle_error (SVRERR_SELECT);

		/*
    if (FD_ISSET (svr_cli_conn[0], &read_fd_set)) {
        // Read from pipe and call appropriate function
        thread req_handler (&Server::handle_pipe_req, this);
        req_handler.join (); // Server waits untils client is served
        /* No socket requests get entertained if client processing 
         * is ongoing (TODO: Change this. Divide this work between
         * two threads */
   // }

    //if (FD_ISSET (server_sockfd, &read_fd_set)) {
        //Read from socket and send reply
        int connfd = accept (server_sockfd, NULL, NULL);
        thread req_handler (&Server::handle_sock_req, this, connfd);
        req_handler.detach();
    //}
  }
}


int 
Server::write_to_socket (int fd, const char * buff, int len)
{
  int cw = 0, w = 0;
  while (cw < len) {
    if ((w = write (fd, buff + cw, len - cw)) < 0)
      handle_error(SVRERR_WTOSOCK);
    cw += w;
  }
  return cw;
}

int
Server::read_from_socket (int fd, char * buff)
{
  int cr = 0, r = 0;
  while ((r = read (fd, buff + cr, MAXBUFF)) > 0)
    cr += r;
  if (r < 0)
    handle_error (SVRERR_RFROMSOCK);
  return cr;
}


void
Server::handle_sock_req (int clientfd) 
{
  char rbuf[MAXBUFF];
  read_from_socket (clientfd, rbuf);

  int r;
  char cmd_str[MAXLINE], msglen[MAXLINE];
  sscanf (rbuf, "%*[^\n]\n%[^\n]\n%[^\n]\n%n", cmd_str, msglen, &r);

  if (!strcmp (cmd_str, "GET")) {
    D (printf ("Recieved GET Request\n");)
    int key_len = 0;
    sscanf (msglen, "%d", &key_len);
    char buff[key_len + 1];
    memcpy(buff, rbuf + r, key_len);
    buff[key_len] = '\0';

    string s, val;
    s.append (buff, key_len);

    int retval = get_sock_req (s, val);

    if (retval == SVRERR_SUCCESS) {
      string msg = "REP\nGET\nSUCCESS\n" + 
                    to_string (val.size ()) + "\n" + val;

      write_to_socket (clientfd, msg.c_str(), msg.size());
      close(clientfd);  
    } else {
      string msg = "REP\nGET\nFAILURE\n" + 
                    to_string (val.size ()) + "\n" + val;

      write_to_socket (clientfd, msg.c_str(), msg.size());
      close(clientfd);  

      handle_error (SVRERR_FAILURE); 
    }
  } else if (!strcmp (cmd_str, "PUT")) {
    D (printf ("Recieved PUT Request\n");)
    int key_len = 0, val_len = 0;
    
    sscanf (msglen, "%d%d", &key_len, &val_len);

    string s, val;
    s.append (rbuf + r, key_len);
    val.append (rbuf + r + key_len, val_len);
    
    rt_tblent_t res;
    int retval = put_sock_req (s, val, res);

    if (retval == SVRERR_SUCCESS) {
      string port_str = to_string (res.addr.port);
      string msg = "REP\nPUT\nSUCCESS\n" +
                    to_string (BASE) + " 16 " + 
                    to_string (port_str.size()) + "\n";
      msg.append (res.id, BASE);
      msg.append (res.addr.ip, 16);
      msg += port_str;

      write_to_socket (clientfd, msg.c_str(), msg.size());
      close(clientfd);

    } else {
      handle_error (SVRERR_FAILURE); 
    }

  } else if (!strcmp (cmd_str, "ADD")) {
    int key_len = 0, ip_len = 0, port_len = 0;

    sscanf (msglen, "%d%d%d", &key_len, &ip_len, &port_len);

    hashkey_t key;
    ip_t ip;
    port_t port;
    memcpy(key, rbuf + r, key_len);
    memcpy(ip, rbuf + r + key_len, ip_len);
    char port_str[MAXLINE];
    memcpy (port_str, rbuf + r + key_len + ip_len, port_len);
    port_str[port_len] = '\0';
    port = atoi (port_str);
    
    char res[MAXBUFF];
    int res_len;
    D ( printf ("Received ADD req from [%s %s:%d\n]", hash2hex(key), ip, 
        port); )
    D ( printf ("\tkey_len = %d  ip_len = %d  port_len = %d\n", key_len, 
        ip_len, port_len);)
  
    int retval = add_sock_req (key, ip, port, res, &res_len);

    if (retval == SVRERR_SUCCESS) {
      write_to_socket (clientfd, res, res_len);
      close(clientfd);
    } else {
      handle_error(SVRERR_FAILURE);
    }

  } else if (!strcmp (cmd_str, "LEAF")) {
    int key_len = 0, ip_len = 0, port_len = 0;

    sscanf (msglen, "%d%d%d", &key_len, &ip_len, &port_len);

    hashkey_t key;
    ip_t ip;
    port_t port;
    memcpy(key, rbuf + r, key_len);
    memcpy(ip, rbuf + r + key_len, ip_len);
    char port_str[MAXLINE];
    memcpy (port_str, rbuf + r + key_len + ip_len, port_len);
    port_str[port_len] = '\0';
    port = atoi (port_str);

    D ( printf ("Recieved LEAF request from [%s %s:%d]\n", hash2hex (key), 
        ip, port);)

    rt_tblent_t newcomer;
    HASH_COPY (newcomer.id, key);
    strcpy (newcomer.addr.ip, ip);
    newcomer.addr.port = port;

    add_to_leafset (&(pastry_node.leafset), &newcomer);
    close(clientfd);
    
    // TODO: Add code for hash redistribution request

  } else if (!strcmp (cmd_str, "HASH")) {
    int key_len = 0, ip_len = 0, port_len = 0;

    sscanf (msglen, "%d%d%d", &key_len, &ip_len, &port_len);

    hashkey_t key;
    ip_t ip;
    port_t port;
    memcpy(key, rbuf + r, key_len);
    memcpy(ip, rbuf + r + key_len, ip_len);
    char port_str[MAXLINE];
    memcpy (port_str, rbuf + r + key_len + ip_len, port_len);
    port_str[port_len] = '\0';
    port = atoi (port_str);

    D ( printf ("Recieved HASH request from [%s %s:%d]\n", hash2hex (key), 
        ip, port);)

		map_t::iterator it;

		for (it = pastry_store.begin (); it != pastry_store.end (); it++) {
			string h_str = it->first;
			hashkey_t h_hash;
			HASH_COPY (h_hash, md5_hash (h_str.c_str ()));

			hashkey_t d_cur, d_new;
			hash_moddiff2 (pastry_node.id, h_hash, d_cur);
			hash_moddiff2 (key, h_hash, d_new);

			if (HASH_LT (d_new, d_cur)) { 
				rt_tblent_t res;
				put_sock_req (it->first, it->second, res);
			}

		}

    close(clientfd);
    
    // TODO: Add code for hash redistribution request

  } else {
    assert (0); /* Invalid Request: Not one of PUT/GET/ADD/LEAF */
  }
}

int
Server::put_sock_req (string s, string val, rt_tblent_t &res)
{
  int status;
  rt_tblent_t next_hop;
	
	hashkey_t key;
	HASH_COPY (key, md5_hash (s.c_str ()));
  get_rt_hop (key, pastry_node, next_hop, status);

  /* Create the base Mega-Packet and update and forward it.
   * The newcomer's leafset need to be adjusted (The newcomer does this)
   * All those who are in the newcomer's leafset need to update
   * their leafset's to include the newcomer as well.
   * Write command for get_leafset () and add_to_leafset () as well
   * Also redistribution of hashed keys? */
  
  if (status == SELF_HIT) {
    D (printf ("SELF_HIT: Storing into local hash %s:%s\n", s.c_str (),
		   val.c_str ());)
    
   	//TODO: Put value of s into local hash table
		// and return success/failure
		pastry_store[s] = val;
		
		HASH_COPY (res.id, pastry_node.id);
		res.addr = pastry_node.addr;

		return SVRERR_SUCCESS;

  } else {
		/* Forward request to the next_hop
		 * Wait for reply.
		 * Identify the routing table row that we must give to the newcomer
		 * Modify it and send msg back. */

		D( printf ("FORWARD: Forwarding to [%s %s:%d]\n", 
					hash2hex (next_hop.id), next_hop.addr.ip, next_hop.addr.port); ) 

			int client_sockfd;
		struct sockaddr_in server_addr = {0};

		if ((client_sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
			handle_error(SVRERR_CONN);

		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = inet_addr (next_hop.addr.ip);
		server_addr.sin_port = htons (next_hop.addr.port);
		if (connect (client_sockfd, (struct sockaddr *) &server_addr, 
					sizeof (server_addr)) < 0)
			handle_error(SVRERR_CONN);

		//string port_temp = to_string (port);
		string req = string ("REQ\nPUT\n") + to_string (s.size ()) + " " +
			to_string (val.size ()) + "\n";
		req += s;
		req += val;

		int len = req.size();
		write_to_socket (client_sockfd, req.c_str(), len);

		shutdown (client_sockfd, SHUT_WR);

		char rbuf[MAXBUFF];
		len = read_from_socket (client_sockfd, rbuf);
		close (client_sockfd);

		char success_code[MAXLINE], val_len_str[MAXLINE];
		int l;
		sscanf (rbuf, "%*[^\n]\n%*[^\n]\n%[^\n]\n%[^\n]\n%n", 
				success_code, val_len_str, &l);
		
		if (!strcmp (success_code, "SUCCESS")) {
			int key_len, ip_len, port_len;
			sscanf (val_len_str, "%d%d%d", &key_len, &ip_len, &port_len);
			HASH_COPY (res.id, rbuf + l);
			memcpy (res.addr.ip, rbuf + l + key_len, ip_len);
			string port_str = string (rbuf + l + key_len + ip_len, port_len);
			res.addr.port = atoi (port_str.c_str ());

			return SVRERR_SUCCESS;
		}

		return SVRERR_FAILURE;
		
  }
}

int
Server::get_sock_req (string s, string &val)
{
  int status;
  rt_tblent_t next_hop;

	hashkey_t key;
	HASH_COPY (key, md5_hash (s.c_str ()));
  get_rt_hop (key, pastry_node, next_hop, status);

  /* Create the base Mega-Packet and update and forward it.
   * The newcomer's leafset need to be adjusted (The newcomer does this)
   * All those who are in the newcomer's leafset need to update
   * their leafset's to include the newcomer as well.
   * Write command for get_leafset () and add_to_leafset () as well
   * Also redistribution of hashed keys? */
  
  if (status == SELF_HIT) {
    D (printf ("SELF_HIT: Looking up %s\n", s.c_str ());)
    
   	//TODO: Get value of s from local hash table. Put it in val
		// and return success/failure
		map_t::const_iterator it = pastry_store.find (s);

		if (it == pastry_store.end ()) {
    	D (printf ("Could not find %s\n", s.c_str ());)
			return SVRERR_FAILURE;
		} else {
			val = it->second;
			return SVRERR_SUCCESS;
		}

  } else {
    /* Forward request to the next_hop
     * Wait for reply.
     * Identify the routing table row that we must give to the newcomer
     * Modify it and send msg back. */

    D( printf ("FORWARD: Forwarding to [%s %s:%d]\n", 
       hash2hex (next_hop.id), next_hop.addr.ip, next_hop.addr.port); ) 

    int client_sockfd;
    struct sockaddr_in server_addr = {0};

    if ((client_sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
      handle_error(SVRERR_CONN);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr (next_hop.addr.ip);
    server_addr.sin_port = htons (next_hop.addr.port);
    if (connect (client_sockfd, (struct sockaddr *) &server_addr, 
        sizeof (server_addr)) < 0)
      handle_error(SVRERR_CONN);

    //string port_temp = to_string (port);
    string req = string ("REQ\nGET\n") + 
                  to_string (s.size ()) + " 0\n"; 
    req += s;

    int len = req.size();
    write_to_socket (client_sockfd, req.c_str(), len);
  
    shutdown (client_sockfd, SHUT_WR);

    char rbuf[MAXBUFF];
    len = read_from_socket (client_sockfd, rbuf);
    close (client_sockfd);

    char success_code[MAXLINE], val_len_str[MAXLINE];
    int l;
    sscanf (rbuf, "%*[^\n]\n%*[^\n]\n%[^\n]\n%[^\n]\n%n", 
            success_code, val_len_str, &l);
	
		int val_len = atoi (val_len_str);
    
		if (!strcmp (success_code, "SUCCESS")) {
			val = string (rbuf + l, val_len);
			return SVRERR_SUCCESS;
		}

		return SVRERR_FAILURE;
		
  }
}

int
Server::add_sock_req ( hashkey_t key, 
                       ip_t ip, 
                       port_t port, 
                       char *buff, 
                       int *len )
{
  int status;
  rt_tblent_t next_hop;

  get_rt_hop (key, pastry_node, next_hop, status);

  /* Create the base Mega-Packet and update and forward it.
   * The newcomer's leafset need to be adjusted (The newcomer does this)
   * All those who are in the newcomer's leafset need to update
   * their leafset's to include the newcomer as well.
   * Write command for get_leafset () and add_to_leafset () as well
   * Also redistribution of hashed keys? */
  
  if (status == SELF_HIT) {
    D (printf ("SELF_HIT: Adding %s %s:%d\n", hash2hex (key), ip, port);)
    node_t node = {0};
    node.leafset = pastry_node.leafset;
    HASH_COPY (node.id, pastry_node.id);
    node.addr = pastry_node.addr;

    int r = max_pref_match_hash (pastry_node.id, key);
    int c = hexdig2dec ((hash2hex (key))[r]);
    D (printf ("\tAdding newcomer to Row: %d and Col: %d\n", r, c);)
    
    /* Add the newcomer's entry to our routing table 
     * only if we do not have an entry already at the 
     * position */
    if (pastry_node.rt_tbl[r][c].status != VALID) {
      pastry_node.rt_tbl[r][c].status = VALID;
      HASH_COPY (pastry_node.rt_tbl[r][c].id, key);
      strcpy (pastry_node.rt_tbl[r][c].addr.ip, ip);
      pastry_node.rt_tbl[r][c].addr.port = port;
    }

    cp_rt_tbl_row_noclobvalid (*(node.rt_tbl + r), 
                               *(pastry_node.rt_tbl + r));
    
    *len = serialize (buff, &node);
    string msg = "REP\nADD\nSUCCESS\n" + to_string (*len) + "\n";
    msg.append (buff, *len);
    memcpy (buff, msg.c_str(), msg.size ());
    *len = msg.size ();

  } else {
    /* Forward request to the next_hop
     * Wait for reply.
     * Identify the routing table row that we must give to the newcomer
     * Modify it and send msg back. */

    D( printf ("FORWARD: Forwarding to [%s %s:%d]\n", 
       hash2hex (next_hop.id), next_hop.addr.ip, next_hop.addr.port); ) 

    int client_sockfd;
    struct sockaddr_in server_addr = {0};

    if ((client_sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
      handle_error(SVRERR_CONN);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr (next_hop.addr.ip);
    server_addr.sin_port = htons (next_hop.addr.port);
    if (connect (client_sockfd, (struct sockaddr *) &server_addr, 
        sizeof (server_addr)) < 0)
      handle_error(SVRERR_CONN);

    string port_temp = to_string (port);
    string req = string ("REQ\nADD\n") + 
                  to_string (BASE) + " 16 " + 
                  to_string (port_temp.size ()) + "\n";
    req.append (key, BASE);
    req.append (ip, 16);
    req += port_temp;

    *len = req.size();
    write_to_socket (client_sockfd, req.c_str(), *len);
  
    shutdown (client_sockfd, SHUT_WR);

    char *rbuf = buff;
    *len = read_from_socket (client_sockfd, rbuf);
    close (client_sockfd);

    int r = max_pref_match_hash (pastry_node.id, key);
    int c = hexdig2dec ((hash2hex (key))[r]);
    if (pastry_node.rt_tbl[r][c].status != VALID) {
      pastry_node.rt_tbl[r][c].status = VALID;
      HASH_COPY (pastry_node.rt_tbl[r][c].id, key);
      strcpy (pastry_node.rt_tbl[r][c].addr.ip, ip);
      pastry_node.rt_tbl[r][c].addr.port = port;
    }
    
    char success_code[MAXLINE];
    int l;
    sscanf (rbuf, "%*[^\n]\n%*[^\n]\n%[^\n]\n%*[^\n]\n%n", 
            success_code, &l);

    node_t tmp;
		deserialize (rbuf + l, &tmp);
  	cp_rt_tbl_row_noclobvalid (*(tmp.rt_tbl + r), *(pastry_node.rt_tbl + r));
		*len = serialize (rbuf + l, &tmp);
        
  }
  return SVRERR_SUCCESS;
}

