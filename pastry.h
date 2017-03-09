#ifndef PASTRY_H_INCLUDED
#define PASTRY_H_INCLUDED

#include "util.h"
#include "semaphore.h" 

enum pastry_status_t { INITIAL, SERVER_STARTED, JOINED };
/* Current state of a pastry node :
 * INITIAL: no node created (create command not encountered yet)
 * SERVER_STARTED: create command given, pastry network has just the
 *                 local node
 * JOINED: Joined another network */
extern pastry_status_t pastry_status;

/* IP of current pastry node */
extern ip_t pastry_ip;
#define DEFAULT_IP "127.0.0.1"

/* Port of current pastry node */ 
extern port_t pastry_port;
#define DEFAULT_PORT 3000

#define PASTRY_PROMPT "pastry $ "

/* A node listens on its pastry_ip:pastry_port for incoming requests from
 * other nodes. Communication with the client part of the same node is
 * via the pipe svr_cli_conn */

/* State data (node ID, Leaf Set, Routing Table) of current node */
extern node_t pastry_node;

/* Pipe for comm. between Server and Client parts of the same node */
extern int svr_cli_conn[2];

/* Semaphore to coordinate actions of Client and Server */
extern Sema pastry_sema;

#endif
