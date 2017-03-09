#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <bits/stdc++.h>
#include <openssl/md5.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;

/* Debug Output Macros */
#ifdef DEBUG
  #define D(x) x
#else
  #define D(x)
#endif

/* Stringification Macros */
#define XSTR(s) STR(s) 
#define STR(s) #s

/* Size in bytes of raw hashes */
#define BASE MD5_DIGEST_LENGTH

/* Size in bytes of hashes stored as a hexadecimal string */
#define KEYS_STR_SZ (2 * BASE)

/* For storing hashes as hexadceimal strings (NULL terminated) */
typedef char hexkey_t[KEYS_STR_SZ + 1];
typedef char hashkey_t[BASE];
typedef hashkey_t nodeid_t;

typedef char ipv4_t[16];
typedef ipv4_t ip_t;
typedef uint16_t port_t;
typedef struct
{
  ip_t ip;
  port_t port;
} addr_t;

enum rt_tblent_status { EMPTY = 0, VALID };
typedef struct
{
  rt_tblent_status status;
  nodeid_t id;
  addr_t addr;
} rt_tblent_t;  /* Routing Table Entry */

#define RT_TBL_NCOLS BASE
#define RT_TBL_NROWS KEYS_STR_SZ

/* Routing Table */
typedef rt_tblent_t rt_tbl_t[RT_TBL_NROWS][RT_TBL_NCOLS];

#define LEAF_SET_HALFSZ (BASE)
#define LEAF_SET_SZ (2 * LEAF_SET_HALFSZ)

/* Leaf Set */
typedef struct
{
  int cw_set_sz, ccw_set_sz;    /* No. of elements in CW and CCW sets */
  rt_tblent_t cw_set[LEAF_SET_HALFSZ];  /* CW set of nodes*/
  rt_tblent_t ccw_set[LEAF_SET_HALFSZ]; /* CCW set of nodes */
  rt_tblent_t owner;  /* Owner of this leaf set */

  /* Maximum extent of CW leaf set (i.e. cw dist 'd' from owner such that
   * d is the max of all such cw-distances for all nodes in the cw_leaf set
   * and d is still less than half_cicle */
  hashkey_t cw_max;

  /* Similary for ccw distances */
  hashkey_t ccw_max;
} leafset_t;

/* Node state */
typedef struct
{
  nodeid_t id;        /* Node ID as raw string */
  hexkey_t hexid;     /* Node ID as printable hex string */
  addr_t addr;        /* Addess (IP, Port) of node */
  rt_tbl_t rt_tbl;    /* Node's Routing Table */
  leafset_t leafset;  /* Node's Leaf Set */
} node_t;

char *hash2hex (char *s);

char *md5_hash (const char *s);
char *md5_hex (const char *s);

int mk_node (ip_t ip, port_t port, node_t *node);

void pr_node (node_t *node);

char *hash_diff (hashkey_t A, hashkey_t B);
char *hash_diff2 (hashkey_t A, hashkey_t B, hashkey_t res);

int hash_cmp (hashkey_t A, hashkey_t B);

char *hash_moddiff (hashkey_t A, hashkey_t B);
char *hash_moddiff2 (hashkey_t A, hashkey_t B, hashkey_t res);

char *hash_ccwdist2 (hashkey_t A, hashkey_t B, hashkey_t res);
char *hash_cwdist2 (hashkey_t A, hashkey_t B, hashkey_t res);

#define HASH_COPY(dest, src) memcpy ((dest), (src), BASE)

#define GE_HALFCIRC(x) (((unsigned char)((x)[0])) >= 0x80U)
#define LT_HALFCIRC(x) (((unsigned char)((x)[0])) < 0x80U)

#define HASH_LT(x, y) (hash_cmp ((x), (y)) == -1)
#define HASH_LE(x, y) (hash_cmp ((x), (y)) <=  0)
#define HASH_GT(x, y) (hash_cmp ((x), (y)) ==  1)
#define HASH_GE(x, y) (hash_cmp ((x), (y)) >=  0)
#define HASH_EQ(x, y) (hash_cmp ((x), (y)) ==  0)

int in_ccw_halfcirc (hashkey_t base, hashkey_t x);
int in_cw_halfcirc (hashkey_t base, hashkey_t x);

int in_ccw_arc (hashkey_t base, hashkey_t extent, hashkey_t x);
int in_cw_arc (hashkey_t base, hashkey_t extent, hashkey_t x);

int max_pref_match (char *s1, char *s2);
int max_pref_match_hash (hashkey_t k1, hashkey_t k2);

int cp_rt_tbl_row (rt_tblent_t *dest, rt_tblent_t *src);
int cp_rt_tbl_row_noclobvalid (rt_tblent_t *dest, rt_tblent_t *src);

int cp_rt_tbl (rt_tblent_t (*dest)[BASE], 
               rt_tblent_t (*src)[BASE]);
int cp_rt_tbl_noclobvalid ( rt_tblent_t (*dest)[BASE], 
                            rt_tblent_t (*src)[BASE]);

int shift_leafset (rt_tblent_t *base, int sz);

int hexdig2dec (char c);

int serialize_tbl_entry (char * s, rt_tblent_t * t);
int deserialize_tbl_entry (char * s, rt_tblent_t * t);
int serialize (char *s, node_t *n);
int deserialize (char * s, node_t * n);

void pr_leafset_entry (rt_tblent_t *ent);

#endif
