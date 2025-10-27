#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

extern SEXP C_hashmap_insert(SEXP hashmap, SEXP key, SEXP value);
extern SEXP C_hashmap_init();
extern SEXP C_hashmap_get(SEXP hashmap, SEXP key);
extern SEXP C_hashmap_remove(SEXP hashmap, SEXP key);
extern SEXP C_hashmap_getkeys(SEXP hashmap);
extern SEXP C_hashmap_getvals(SEXP hashmap);
extern SEXP C_hashmap_clear(SEXP hashmap);
extern SEXP C_hashmap_size(SEXP hashmap);
extern SEXP C_hashmap_get_range(SEXP hashmap, SEXP keys);
extern SEXP C_hashmap_contains(SEXP hashmap, SEXP keys);
extern SEXP C_hashmap_contains_range(SEXP hashmap, SEXP keys);
extern SEXP C_hashmap_remove_range(SEXP hashmap, SEXP keys);
extern SEXP C_hashmap_tolist(SEXP hashmap);
extern SEXP C_hashmap_invert(SEXP hashmap);
extern SEXP C_hashmap_set_range(SEXP hashmap, SEXP keys, SEXP values, SEXP replace);
extern SEXP C_hashmap_set(SEXP hashmap, SEXP value, SEXP replace);

static const R_CallMethodDef CallEntries[] = {
    {"C_hashmap_insert", (DL_FUNC) &C_hashmap_set, 4}, 
    {"C_hashmap_init", (DL_FUNC) &C_hashmap_init, 0}, 
    {"C_hashmap_get", (DL_FUNC) &C_hashmap_get, 2}, 
    {"C_hashmap_remove", (DL_FUNC) &C_hashmap_remove, 2}, 
    {"C_hashmap_getkeys", (DL_FUNC) &C_hashmap_getkeys, 1}, 
    {"C_hashmap_getvals", (DL_FUNC) &C_hashmap_getvals, 1}, 
    {"C_hashmap_clear", (DL_FUNC) &C_hashmap_clear, 1}, 
    {"C_hashmap_size", (DL_FUNC) &C_hashmap_size, 1}, 
    {"C_hashmap_contains", (DL_FUNC) &C_hashmap_contains, 2},
    {"C_hashmap_contains_range", (DL_FUNC) &C_hashmap_contains_range, 2},
    {"C_hashmap_get_range", (DL_FUNC) &C_hashmap_get_range, 2},
    {"C_hashmap_insert_range", (DL_FUNC) &C_hashmap_set_range, 4},
    {"C_hashmap_remove_range", (DL_FUNC) &C_hashmap_remove_range, 2},
    {"C_hashmap_tolist", (DL_FUNC) &C_hashmap_tolist, 1},
    {"C_hashmap_invert", (DL_FUNC) &C_hashmap_invert, 1},
    {NULL, NULL, 0}
};


void R_init_Chashmap(DllInfo *dll) {
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL); 
    R_useDynamicSymbols(dll, FALSE);

}