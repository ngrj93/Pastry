#ifndef ROUTING_H_INCLUDED
#define ROUTING_H_INCLUDED

#include "util.h"

enum rtng_status { SELF_HIT, FORWARD };

/* Determines the next hop to take in routing a get/put/add request
 * SELF_HIT means that the current node is the desired destination itself
 * FORWARD means that we must forward the request to 
 * the pastry-node stored in next_hop.
 * This status is return via the status variable (call by reference).
 * The calling node must provide a reference to its node state 
 * by the variable node
 */
int
get_rt_hop ( hashkey_t key, 
             node_t &node, 
             rt_tblent_t &next_hop, 
             int &status );

#endif
