#include "leafset.h"


/* Traverses the array arr of rt_tblent_t of size pointed to by sz
 * which is allowed to grow to a maximum of max_sz entries.
 * origin is the entry that can be (for the sake of visualization)
 * assumed to the left of arr (call it index -1). 
 * Every element at index i forms a directed arc with its neighbour 
 * at index i - 1 where 0 <= i < sz.
 * arc_func returns true when a hash (third arg) lies between the 
 * directed arc from the first arg to the second arg
 * We insert n at ist proper place in the array, if it lies in
 * between one of these directed args, by shifting all the downstream
 * elements one position to the left (causing the last element to possibly
 * fall out of the array if the array is full). We return the index
 * at which n was inserted, -1 if we could not insert n anywhere.
 * If our insertion causes the number of entries in the array to increase
 * we increment sz, but we never let sz cross max_sz and may discard the 
 * edge element to ensure this */
static int
ins_in_leafset_arr (  rt_tblent_t *arr, 
                      rt_tblent_t *origin, 
                      int *sz, int max_sz, 
                      rt_tblent_t *n,
                      int (*arc_func)(hashkey_t, hashkey_t, hashkey_t) )
{
  int i;
  hashkey_t base, k, st, end;
  
  HASH_COPY (base, origin->id);
  HASH_COPY (st, base);
  HASH_COPY (k, n->id);

  for (i = 0; i < *sz; i++) {
    HASH_COPY (end, arr[i].id);
    if (arc_func (st, end, k)) {
      shift_leafset (arr + i, (*sz >= max_sz) ? (*sz - i - 1) : (*sz - i));
      arr[i] = *n;
      if (*sz < max_sz)
        (*sz)++;
      return i;
    }

    HASH_COPY (st, end);
  }

  return -1;
}

int
add_to_leafset (leafset_t *lf, rt_tblent_t *n)
{
  int lf_size = lf->cw_set_sz + lf->ccw_set_sz;
  int i;
  hashkey_t base, k;

  HASH_COPY (base, lf->owner.id);
  HASH_COPY (k, n->id);
  
  D( printf ("Adding [%s %s:%d] to leafset\n", hash2hex (k), 
     n->addr.ip, n->addr.port); )
  
  if (lf_size == 0) {
    D( printf ("\tleafset is empty\n"); )
    
    if (in_cw_halfcirc (base, k)) {
      lf->cw_set[lf->cw_set_sz++] = *n;
      HASH_COPY (lf->cw_max, k);
      
      D( printf ("\tadded to CW set\n"); )
    } else {
      lf->ccw_set[lf->ccw_set_sz++] = *n;
      HASH_COPY (lf->ccw_max, k);
      
      D( printf ("\tadded to CW set\n"); )
    }
  } else if (lf_size < LEAF_SET_SZ) {
    if (lf->cw_set_sz < lf->ccw_set_sz) {
      D( printf ("\tCW set has fewer elements (%d vs CCW's %d)\n", 
         lf->cw_set_sz, lf->ccw_set_sz); )
      i = ins_in_leafset_arr (lf->cw_set, &(lf->owner), &(lf->cw_set_sz),
                              LEAF_SET_HALFSZ, n, in_cw_arc);
  
      if (i < 0) {
        lf->cw_set[lf->cw_set_sz++] = *n;
        HASH_COPY (lf->cw_max, k);
        D( printf ("\tAdded at the end (index: %d)\n", lf->cw_set_sz); )
      }
      D( else printf ("\tAdded at index: %d\n", i); )
    } else if (lf->ccw_set_sz < lf->cw_set_sz) {
      D( printf ("\tCCW set has fewer elements (%d vs CCW's %d)\n", 
         lf->cw_set_sz, lf->ccw_set_sz); )
      
      i = ins_in_leafset_arr (lf->ccw_set, &(lf->owner), &(lf->ccw_set_sz),
                              LEAF_SET_HALFSZ, n, in_ccw_arc);
      
      if (i < 0) {
        lf->ccw_set[lf->ccw_set_sz++] = *n;
        HASH_COPY (lf->ccw_max, k);
        D( printf ("\tAdded at the end (index: %d)\n", lf->ccw_set_sz); )
      }
      D( else printf ("\tAdded at index: %d\n", i); )
    } else {
      D( printf ("\tleafset not full, but balanced\n"); )
      i = ins_in_leafset_arr (lf->cw_set, &(lf->owner), &(lf->cw_set_sz),
                              LEAF_SET_HALFSZ, n, in_cw_arc);

      if (i < 0) {
        i = ins_in_leafset_arr (lf->ccw_set, &(lf->owner), 
                                &(lf->ccw_set_sz), LEAF_SET_HALFSZ, n, 
                                in_ccw_arc);

        if (i < 0) {
          lf->ccw_set[lf->ccw_set_sz++] = *n;
          HASH_COPY (lf->ccw_max, k);

          D( printf ("\tAdded at the end of CCW set(index: %d)\n", 
             lf->ccw_set_sz); )
        }
        D( else printf ("\tAdded at index: %d in CCW set\n", i); )
      }
      D( else printf ("\tAdded at index: %d in CW set\n", 
         lf->ccw_set_sz); )
    }
  } else {
    D( printf ("\tleafset is full\n"); )
    i = ins_in_leafset_arr (lf->cw_set, &(lf->owner), &(lf->cw_set_sz),
                            LEAF_SET_HALFSZ, n, in_cw_arc);

    if (i < 0) {
        i = ins_in_leafset_arr (lf->ccw_set, &(lf->owner), 
                                &(lf->ccw_set_sz), LEAF_SET_HALFSZ, n, 
                                in_ccw_arc);
      assert (i >= 0);
      D( printf ("\tAdded at index: %d in CCW set\n", i); )
    }
    D( else printf ("\tAdded at index: %d in CW set\n", i); )

    HASH_COPY (lf->cw_max, lf->cw_set[lf->cw_set_sz - 1].id);
    HASH_COPY (lf->ccw_max, lf->ccw_set[lf->ccw_set_sz - 1].id);
  }

  return 0;
}
      
