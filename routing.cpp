#include "util.h"
#include "routing.h"

int
get_rt_hop ( hashkey_t key, 
             node_t &node, 
             rt_tblent_t &next_hop, 
             int &status )
{
    D( printf("ROUTE: [%s %s:%d] is attempting to route [%s]\n", 
       node.hexid, node.addr.ip, node.addr.port, hash2hex (key)); )
  
  /* Check if key == node's id (EDIT: This is covered by the code below) */
  if (node.leafset.cw_set_sz < LEAF_SET_HALFSZ || 
      node.leafset.ccw_set_sz < LEAF_SET_HALFSZ) {
      
      D( printf ("\tleafset not full - All nodes are here\n"); )
      hashkey_t min_diff, cur_diff, res;
      HASH_COPY (res, node.id);
      hash_moddiff2 (key, res, min_diff);
      next_hop = node.leafset.owner;

      for (int i = 0; i < node.leafset.cw_set_sz; i++) {
        hash_moddiff2 (key, node.leafset.cw_set[i].id, cur_diff);
        if (HASH_LT (cur_diff, min_diff)) {
          HASH_COPY (min_diff, cur_diff);
          HASH_COPY (res, node.leafset.cw_set[i].id);
          next_hop = node.leafset.cw_set[i];
        }
      }
      
      for (int i = 0; i < node.leafset.ccw_set_sz; i++) {
        hash_moddiff2 (key, node.leafset.ccw_set[i].id, cur_diff);
        if (HASH_LT (cur_diff, min_diff)) {
          HASH_COPY (min_diff, cur_diff);
          HASH_COPY (res, node.leafset.ccw_set[i].id);
          next_hop = node.leafset.ccw_set[i];
        }
      }

      if (HASH_EQ (res, node.id)) {
        status = SELF_HIT;
      } else {
        status = FORWARD;
      }

      return 0;

  } else {
    D( printf ("\tleafset if full\n"); )
    if (in_cw_arc (node.id, node.leafset.cw_max, key)) {
      // Find closest in leafset CW
      D( printf ("\tFound in CW leafset\n"); )
      hashkey_t min_diff, cur_diff, res;
      HASH_COPY (res, node.id);
      hash_moddiff2 (key, res, min_diff);
      next_hop = node.leafset.owner;

      for (int i = 0; i < node.leafset.cw_set_sz; i++) {
        hash_moddiff2 (key, node.leafset.cw_set[i].id, cur_diff);
        if (HASH_LT (cur_diff, min_diff)) {
          HASH_COPY (min_diff, cur_diff);
          HASH_COPY (res, node.leafset.cw_set[i].id);
          next_hop = node.leafset.cw_set[i];
        }
      }
      
      if (HASH_EQ (res, node.id)) {
        status = SELF_HIT;
      } else {
        status = FORWARD;
      }

      return 0;
    } else if (in_ccw_arc (node.id, node.leafset.ccw_max, key)) {
      // Find closest in leafset CCW
        D( printf ("\tFound in CCW leafset\n"); )
        hashkey_t min_diff, cur_diff, res;
        HASH_COPY (res, node.id);
        hash_moddiff2 (key, res, min_diff);
        next_hop = node.leafset.owner;

        for (int i = 0; i < node.leafset.ccw_set_sz; i++) {
          hash_moddiff2 (key, node.leafset.ccw_set[i].id, cur_diff);
          if (HASH_LT (cur_diff, min_diff)) {
            HASH_COPY (min_diff, cur_diff);
            HASH_COPY (res, node.leafset.ccw_set[i].id);
            next_hop = node.leafset.ccw_set[i];
          }
        }
        
        if (HASH_EQ (res, node.id)) {
          status = SELF_HIT;
        } else {
          status = FORWARD;
        }

        return 0;
    } else {
      // Use Routing Table
      D( printf ("\tLooking in Routing Table\n"); )
      int r = max_pref_match_hash (node.id, key);
      assert (r < KEYS_STR_SZ - 1);
      int c = hexdig2dec ((hash2hex (key))[r]);

      if (node.rt_tbl[r][c].status == VALID) {
      //Forward to that cell
        status = FORWARD;
        next_hop = node.rt_tbl[r][c];
        
        D( printf ("\tForwarding to [%s %s:%d]\n", hash2hex (next_hop.id), 
           next_hop.addr.ip, next_hop.addr.port); )
        return 0;
      } else {
        // Make an array of hashes of all nodes you know
        // Find the closest one to the key and forward it to that node
        D( printf ("\tRare Case encountered\n"); )
        hashkey_t min_diff, cur_diff, res;
        HASH_COPY (res, node.id);
        hash_moddiff2 (key, res, min_diff);
        next_hop = node.leafset.owner;
        
        /* Unused. Why ?
        int node_pref_match = max_pref_match (A, D), cur_pref_match = 0;
        */

        for (int i = 0; i < node.leafset.cw_set_sz; i++) {
          hash_moddiff2 (key, node.leafset.cw_set[i].id, cur_diff);
          if (HASH_LT (cur_diff, min_diff)) {
            HASH_COPY (min_diff, cur_diff);
            HASH_COPY (res, node.leafset.cw_set[i].id);
            next_hop = node.leafset.cw_set[i];
          }
        }
        
        for (int i = 0; i < node.leafset.ccw_set_sz; i++) {
          hash_moddiff2 (key, node.leafset.ccw_set[i].id, cur_diff);
          if (HASH_LT (cur_diff, min_diff)) {
            HASH_COPY (min_diff, cur_diff);
            HASH_COPY (res, node.leafset.ccw_set[i].id);
            next_hop = node.leafset.ccw_set[i];
          }
        }
        
        for (int r = 0; r < KEYS_STR_SZ; r++) {
          for (int c = 0; c < BASE; c++) {
            if (node.rt_tbl[r][c].status == VALID) {
              hash_moddiff2 (key, node.rt_tbl[r][c].id, cur_diff);
              if (HASH_LT (cur_diff, min_diff)) {
                HASH_COPY (min_diff, cur_diff);
                HASH_COPY (res, node.rt_tbl[r][c].id);
                next_hop = node.rt_tbl[r][c];
              }
            }
          }
        }

        if (HASH_EQ (res, node.id)) {
          status = SELF_HIT;
        } else {
          status = FORWARD;
          D( printf ("\tForwarding to [%s %s:%d]\n", 
             hash2hex (next_hop.id), next_hop.addr.ip, 
             next_hop.addr.port); )
        }
        return 0;
      }
    }
  }
}

