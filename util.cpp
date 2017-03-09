#include "util.h"

using namespace std;

/* Return the hex string corresponding to the hash string in s
 * s need not be NULL terminated but must be at least BASE bytes.
 * Only first BASE bytes of s are considered.
 * The returned array is NULL terminated.
 * Caller must copy the returned static array to their own
 * storage for safe persistence (strcpy family can be used) */
char *
hash2hex (char *s)
{
  static char str[KEYS_STR_SZ + 1];
  char *c = str;
  for (int i = 0; i < BASE; i++) {
    sprintf (c, "%02X", (unsigned int)((unsigned char)s[i]));
    c += 2;
  }
  return str;
}


/* Returns the md5 hash of the string s as a 128 bit raw string
 * s must be NULL terminated.
 * The returned static char string is NULL terminated for safety, but
 * caller must use the memcpy family function for copying/moving */
char *
md5_hash (const char *s)
{
  static char str[MD5_DIGEST_LENGTH + 1];
  MD5 ((unsigned char *) s,
            strlen (s), (unsigned char *) str);
  str[BASE] = '\0';
  return str;
}


/* Returns the md5 hash of the string s as a hex string
 * s must be NULL terminated.
 * The returned hex string is NULL terminated.
 * Caller must copy the returned static array to their own
 * storage for safe persistence (strcpy family can be used) */
char *
md5_hex (const char *s)
{
  char str[MD5_DIGEST_LENGTH];
  MD5 ((unsigned char *)s,
            strlen (s), (unsigned char *)str);
  return hash2hex (str);
}


/* Returns the decimal value of a hex digit 
 * i.e. '3' returns 3, 'B' and 'b' both return 11 and so on */ 
int
hexdig2dec (char c)
{
  c = ::toupper (c);
  if (c <= '9')
    return c - '0';

  return c - 'A' + 10;
}


/* Creates a new empty pastry node in the storage pointed to by node
 * (i.e. this function allocates no memory: node must be already
 * allocated). ip must be an IPv4 address in dotted decimal format.
 * Port must be a valid INET port. 0 is returned upon successful
 * node creation. */
int 
mk_node (ip_t ip, port_t port, node_t *node)
{
  memset (node, 0, sizeof (node_t));

  strcpy (node->addr.ip, ip);
  node->addr.port = port;

  string s = string (ip) + ";" + to_string (port);
  memcpy (node->id, md5_hash (s.c_str ()), BASE);
  strcpy (node->hexid, hash2hex (node->id));

  for (int i = 0; i < KEYS_STR_SZ; i++){
    int j = hexdig2dec (node->hexid[i]);
    node->rt_tbl[i][j].status = VALID;
    HASH_COPY (node->rt_tbl[i][j].id, node->id);
    node->rt_tbl[i][j].addr = node->addr; 
  }
  
  node->leafset.owner.status = VALID;
  HASH_COPY (node->leafset.owner.id, node->id);
  node->leafset.owner.addr = node->addr;

  return 0;
}

/* Print the node's state data for debugging/logging */
void 
pr_node (node_t *node)
{
  cout << "Node ID: " << node->hexid << endl;
  cout << "IP: " << node->addr.ip << endl;
  cout << "Port: " << node->addr.port << endl;
}

/* Returns the 2's complement difference between A and B
 * Caller must copy the returned static array to their own
 * storage for safe persistence (memcpy family should be used) */
char * 
hash_diff (hashkey_t A, hashkey_t B)
{
  static hashkey_t C;
  uint8_t *a, *b, *c;
  uint8_t borrow = 0;
  uint16_t acc;

  a = (uint8_t *)A;
  b = (uint8_t *)B;
  c = (uint8_t *)C;
  for (int i = 15; i >= 0; i--) {
    if ((a[i] < b[i]) || (a[i] == b[i] && borrow)) {
      acc = (((((uint16_t)1)<<8) + a[i]) - borrow) - b[i];
      c[i] = acc;
      borrow = 1;
    } else {
      acc = (a[i] - borrow) - b[i];
      c[i] = acc;
      borrow = 0;
    }
  }

  return C;
}


/* Same as hash_diff () except that the storage used for the result
 * is provided by the caller in res */
char * 
hash_diff2 (hashkey_t A, hashkey_t B, hashkey_t res)
{
  uint8_t *a, *b, *c;
  uint8_t borrow = 0;
  uint16_t acc;

  a = (uint8_t *)A;
  b = (uint8_t *)B;
  c = (uint8_t *)res;
  for (int i = 15; i >= 0; i--) {
    if ((a[i] < b[i]) || (a[i] == b[i] && borrow)) {
      acc = (((((uint16_t)1)<<8) + a[i]) - borrow) - b[i];
      c[i] = acc;
      borrow = 1;
    } else {
      acc = (a[i] - borrow) - b[i];
      c[i] = acc;
      borrow = 0;
    }
  }

  return res;
}


/* Compares two hash values A and B
 * Returns -1, 0, 1, if A < B, A == B, and A > B, respectively */
int 
hash_cmp (hashkey_t A, hashkey_t B)
{
  for (int i = 0; i < BASE; i++) {
    if ((unsigned char)A[i] < (unsigned char)B[i])
      return -1;
    else if ((unsigned char)A[i] > (unsigned char)B[i])
      return 1;
  }

  return 0;
}

/* Performs distance calculation in a circular keyspace of 2^128
 * The distance returned is always <= 2^127 */
char *
hash_moddiff (hashkey_t A, hashkey_t B)
{
  static hashkey_t res;

  hash_diff2 (A, B, res);
  if ((unsigned char)res[0] >= 0x80U) {
    hash_diff2 (B, A, res);
  }

  return res;
}


/* Same as hash_moddiff () except that the storage used for the result
 * is provided by the caller in res */
char *
hash_moddiff2 (hashkey_t A, hashkey_t B, hashkey_t res)
{
  hash_diff2 (A, B, res);
  if ((unsigned char)res[0] >= 0x80U)
    hash_diff2 (B, A, res);

  return res;
}

/* Distance from A to B measured in Counter-Clock Wise direction 
 * A --> B (CCW) */
char *
hash_ccwdist2 (hashkey_t A, hashkey_t B, hashkey_t res)
{
  hash_diff2 (A, B, res);
  return res;
}

/* Distance from A to B measured in Clock Wise direction 
 * A --> B (CW) */
char *
hash_cwdist2 (hashkey_t A, hashkey_t B, hashkey_t res)
{
  hash_diff2 (B, A, res);
  return res;
}


/* Return true if x lies in the CCW half circle of base */
int 
in_ccw_halfcirc (hashkey_t base, hashkey_t x)
{
  hashkey_t res;
  hash_ccwdist2 (base, x, res);

  return LT_HALFCIRC (res);
}


/* Return true if x lies in the CW half circle of base */
int 
in_cw_halfcirc (hashkey_t base, hashkey_t x)
{
  hashkey_t res;
  hash_ccwdist2 (base, x, res);

  return GE_HALFCIRC (res);
}

/* Returns true if x lies in the CCW arc defined by base and extent */
int
in_ccw_arc (hashkey_t base, hashkey_t extent, hashkey_t x)
{
  hashkey_t res, chk;

  hash_ccwdist2 (base, extent, chk);
  hash_ccwdist2 (base, x, res);

  return HASH_LT (res, chk);
}

/* Returns true if x lies in the CW arc defined by base and extent */
int
in_cw_arc (hashkey_t base, hashkey_t extent, hashkey_t x)
{
  hashkey_t res, chk;
  hash_cwdist2 (base, extent, chk);
  hash_cwdist2 (base, x, res);

  return HASH_LE (res, chk);
}

/* Returns the length of the maximum common prefix between strings s1 & s2
 * Return value is 0 to MIN (strlen (s1), strlen (s2)) */
int
max_pref_match (char *s1, char *s2)
{
  int i = 0;
  while ((*(s1++) == *(s2++)) != '\0')
    i++;

  return i;
}

/* Does the same thing as max_pref_match (), but accepts hash keys as
 * arguments, converts them to hexadecimal strings and then calls
 * max_pref_match () to compare their prefixes */
int
max_pref_match_hash (hashkey_t k1, hashkey_t k2)
{
  char s1[KEYS_STR_SZ + 1], s2[KEYS_STR_SZ];
  strcpy (s1, hash2hex (k1));
  strcpy (s2, hash2hex (k2));

  return max_pref_match (s1, s2);
}

/* Copies a row of the routing table from src to dest
 * src and dest give the base address of the first element of
 * the corresponding source and destination rows */
int
cp_rt_tbl_row (rt_tblent_t *dest, rt_tblent_t *src)
{
  int c;
  for (c = 0; c < BASE; c++)
    dest[c] = src[c];

  return 0;
}

/* Same as cp_rt_tbl_row () but does not overwrite already valid entries
 * in the destination */
int
cp_rt_tbl_row_noclobvalid (rt_tblent_t *dest, rt_tblent_t *src)
{
  int c;
  for (c = 0; c < BASE; c++)
    if (dest[c].status != VALID)
      dest[c] = src[c];

  return 0;
}

/* Copies the routing table based at src to dest */
int
cp_rt_tbl (rt_tblent_t (*dest)[BASE], rt_tblent_t (*src)[BASE])
{
  int r, c;
  for (r = 0; r < KEYS_STR_SZ; r++)
    for (c = 0; c < BASE; c++)
      dest[r][c] = src[r][c];

  return 0;
}

/* Same as cp_rt_tbl () but does not overwrite already valid entries 
 * in the destination */
int
cp_rt_tbl_noclobvalid (rt_tblent_t (*dest)[BASE], rt_tblent_t (*src)[BASE])
{
  int r, c;
  for (r = 0; r < KEYS_STR_SZ; r++)
    for (c = 0; c < BASE; c++)
      if (dest[r][c].status != VALID)
        dest[r][c] = src[r][c];

  return 0;
}

int
shift_leafset (rt_tblent_t *base, int sz)
{
  for (int i = sz; i > 0; i--)
    base[i] = base[i - 1];
  return 0;
}

/* Converts table entry t to a serialized form sitable for network transfer
 * and stores it in c */
int serialize_tbl_entry (char * s, rt_tblent_t * t)
{
  uint16_t temp = htons ((uint16_t)(t->status));
  memcpy (s, (void *) &temp, 2);
  s += 2;
  memcpy (s, t->id, BASE);
  s += BASE;
  memcpy (s, t->addr.ip, 16);
  s += 16;
  
  temp = htons ((uint16_t)(t->addr.port));
  memcpy (s, (void *) &temp, 2);
  s += 2;

  return 0;
}


/* Uses serialized table entry stored in s to populate the table entry t
 * s must have been generated by a previous call to 
 * serialize_tbl_entry () */
int deserialize_tbl_entry (char * s, rt_tblent_t * t)
{
  uint16_t *temp = (uint16_t *) s;
  t->status = (rt_tblent_status) ntohs (*temp);
  s += 2;
  memcpy (t->id, s, BASE);
  s += BASE;
  memcpy (t->addr.ip, s, 16);
  s += 16;
  temp = (uint16_t *) s;
  t->addr.port = ntohs (*temp);

  return 0;
}

/* Uses serialized routing table stored in s to populate the 
 * routing table t
 * s must have been generated by a previous call to serialize () */
int
deserialize (char * s, node_t * n)
{
  memcpy (n->id, s, BASE);
  s += BASE;
  
  memcpy (n->addr.ip, s, 16);
  s += 16;
  
  port_t *port_temp = (port_t *) s;
  n->addr.port = ntohs (*port_temp);
  s += 2;
  
  for (int i = 0; i < KEYS_STR_SZ; i++)
    for(int j = 0; j < BASE; j++) {
      deserialize_tbl_entry (s, &(n->rt_tbl[i][j]));
      s += 20 + BASE;
    }

  n->leafset.cw_set_sz  = (unsigned char)(*s++);
  n->leafset.ccw_set_sz = (unsigned char)(*s++);

  for (int i = 0; i < LEAF_SET_HALFSZ; i++) {
    deserialize_tbl_entry (s, &(n->leafset.cw_set[i]));
    s += 20 + BASE;
  }

  for (int i = 0; i < LEAF_SET_HALFSZ; i++) {
    deserialize_tbl_entry (s, &(n->leafset.ccw_set[i]));
    s += 20 + BASE;
  }

  deserialize_tbl_entry (s, &(n->leafset.owner));
  s += 20 + BASE;
  
  memcpy (n->leafset.cw_max, s, BASE);
  s += BASE;
  
  memcpy (n->leafset.ccw_max, s, BASE);
  s += BASE;

  strcpy (n->hexid, hash2hex (n->id));

  return 0;
}


/* Converts routing table t to a serialized form suitable for 
 * network transfer and stores it in s.
 * n->hexid is ignored */
int
serialize (char *s, node_t *n)
{
  char *sav = s;

  memcpy (s, n->id, BASE);
  s += BASE;

  memcpy (s, n->addr.ip, 16);
  s += 16;

  uint16_t temp = htons ((uint16_t)(n->addr.port));
  memcpy (s, (void *) &temp, 2);
  s += 2;
  
  for(int i = 0; i < KEYS_STR_SZ; i++)
    for(int j = 0; j < BASE; j++) {
      serialize_tbl_entry (s, &(n->rt_tbl[i][j]));
      s += 20 + BASE;
    }

  *(s++) = (unsigned char) n->leafset.cw_set_sz;
  *(s++) = (unsigned char) n->leafset.ccw_set_sz;

  for (int i = 0; i < LEAF_SET_HALFSZ; i++) {
    serialize_tbl_entry (s, &(n->leafset.cw_set[i]));
    s += 20 + BASE;
  } 

  for (int i = 0; i < LEAF_SET_HALFSZ; i++) {
    serialize_tbl_entry (s, &(n->leafset.ccw_set[i]));
    s += 20 + BASE;
  }

  serialize_tbl_entry (s, &(n->leafset.owner));
  s += 20 + BASE;
  
  memcpy (s, n->leafset.cw_max, BASE);
  s += BASE;

  memcpy (s, n->leafset.ccw_max, BASE);
  s += BASE;

  return (s - sav);
}

/* prints a routing table entry */
void
pr_leafset_entry (rt_tblent_t *ent)
{
  const char fmt_str[] = "[ ID: %*s  IP-ADDR: %s   PORT: %d ]";
  printf (fmt_str, KEYS_STR_SZ, hash2hex(ent->id), 
          ent->addr.ip, ent->addr.port);
}

#ifdef TESTING

int
eq_rt_tbl_ent (rt_tblent_t *A, rt_tblent_t *B)
{
  int res = 1;

  res *= A->status == B->status;
  res *= !memcmp (A->id, B->id, BASE);
  res *= !memcmp (A->addr.ip, B->addr.ip, 16);
  res *= A->addr.port == B->addr.port;

  return res;
}
  
int 
eq_nodes (node_t *A, node_t *B)
{
  int res = 1;
  res *= !memcmp (A->id, B->id, BASE);
  res *= !memcmp (A->addr.ip, B->addr.ip, 16);
  res *= A->addr.port == B->addr.port;
  
  for (int r = 0; r < KEYS_STR_SZ; r++)
    for (int c = 0; c < BASE; c++)
      res *= eq_rt_tbl_ent (&(A->rt_tbl[r][c]), &(B->rt_tbl[r][c]));

  res *= A->leafset.cw_set_sz == B->leafset.cw_set_sz;
  res *= A->leafset.ccw_set_sz == B->leafset.ccw_set_sz;

  for (int i = 0; i < LEAF_SET_HALFSZ; i++) {
    res *= eq_rt_tbl_ent (A->leafset.cw_set + i, B->leafset.cw_set + i);
    res *= eq_rt_tbl_ent (A->leafset.ccw_set + i, B->leafset.ccw_set + i);
  }

  res *= eq_rt_tbl_ent (&(A->leafset.owner), &(B->leafset.owner));

  res *= !memcmp (A->leafset.cw_max, B->leafset.cw_max, BASE);
  res *= !memcmp (A->leafset.ccw_max, B->leafset.ccw_max, BASE);

  return res;
}



void test_serial_deserial ()
{
  /* Test Serialization and Deserialization*/
  printf ("\n(START TEST) SERIALIZATION AND DESERIALIZATION:\n");

  hashkey_t arr[20];
  int p = 3000;

  for (int i = 0; i < 20; i++, p++) {
    char str[20];
    sprintf (str, "%d", p);
    memcpy (arr + i, md5_hash (str), BASE);
    printf ("arr[%2d] = %s\n", i, hash2hex (arr[i]));
  }

  qsort (arr, 20, BASE, hash_cmp);

  printf ("\nAfter Sorting:\n");
  for (int i = 0; i < 20; i++) 
    printf ("arr[%2d] = %s\n", i, hash2hex (arr[i]));

  printf ("(END TEST)\n");
}


void test_hex_diff ()
{
  /* Test Subtraction Routine */
  printf ("\n(START TEST) HEX_DIFF ():\n");

  hashkey_t a, b, a_b, b_a;
  memset (a, 0, BASE);
  a[BASE - 1] = 12;
  printf ("a = 0x%s\n", hash2hex (a));

  memset (b, 0, BASE);
  b[BASE - 1] = 200;
  printf ("b = 0x%s\n", hash2hex (b));

  memcpy (a_b, hash_diff (a, b), BASE);
  printf ("a_b = 0x%s\n", hash2hex (a_b));

  memcpy (b_a, hash_diff (b, a), BASE);
  printf ("b_a = 0x%s\n", hash2hex (b_a));

  printf ("(END TEST)\n");
}

void
test_nodes ()
{
  node_t node_1, node_2;

  printf ("\n(START TEST) NODE CREATION:\n");
  mk_node ("127.0.0.1", 3000, &node_1);
  printf ("Node 1:\n");
  pr_node (&node_1);

  mk_node ("127.0.0.1", 3002, &node_2);
  printf ("\nNode 2:\n");
  pr_node (&node_2);

  printf ("\nhash (Node_1) - hash (Node_2) = %s\n", 
          hash2hex (hash_moddiff (node_1.id, node_2.id)));
  printf ("hash (Node_2) - hash (Node_1) = %s\n", 
          hash2hex (hash_moddiff (node_2.id, node_1.id)));

  printf ("(END TEST)\n");
}


void test_arc_rel ()
{
  /* Test Subtraction Routine */
  printf ("\n(START TEST) ARC RELATIONSHIPS:\n");

  hashkey_t a, b, x, res;
  memset (a, 0, BASE);
  a[BASE - 1] = 12;
  printf ("a = 0x%s\n", hash2hex (a));

  memset (b, 0, BASE);
  b[BASE - 1] = 200;
  printf ("b = 0x%s\n", hash2hex (b));

  memset (x, 0, BASE);
  x[BASE - 1] = 12;
  printf ("x = 0x%s\n", hash2hex (x));

  hash_cwdist2 (a, b, res);
  printf ("CW distance from a to b = %s\n", hash2hex (res));

  hash_ccwdist2 (a, b, res);
  printf ("CCW distance from a to b = %s\n", hash2hex (res));

  if (in_ccw_arc (a, b, x))
    printf ("x is in CCW arc of a & b\n");

  if (in_cw_arc (a, b, x))
    printf ("x is in CW arc of a & b\n");

  printf ("(END TEST)\n");
}


void test_cmp ()
{
  /* Test Subtraction Routine */
  printf ("\n(START TEST) ARC RELATIONSHIPS:\n");

  hashkey_t arr[20];
  int p = 3000;

  for (int i = 0; i < 20; i++, p++) {
    char str[20];
    sprintf (str, "%d", p);
    memcpy (arr + i, md5_hash (str), BASE);
    printf ("arr[%2d] = %s\n", i, hash2hex (arr[i]));
  }

  qsort (arr, 20, BASE, hash_cmp);

  printf ("\nAfter Sorting:\n");
  for (int i = 0; i < 20; i++) 
    printf ("arr[%2d] = %s\n", i, hash2hex (arr[i]));

  printf ("(END TEST)\n");
}


int
main ()
{
  test_cmp ();
  //test_arc_rel ();
  //test_hex_diff ();
  //test_nodes ();

  return 0;
}

#endif

