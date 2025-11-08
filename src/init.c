#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

extern SEXP C_hashmap_init();
extern SEXP C_hashmap_get(SEXP map, SEXP key);
extern SEXP C_hashmap_remove(SEXP map, SEXP key);
extern SEXP C_hashmap_getkeys(SEXP map);
extern SEXP C_hashmap_getvals(SEXP map);
extern SEXP C_hashmap_clear(SEXP map);
extern SEXP C_hashmap_size(SEXP map);
extern SEXP C_hashmap_get_range(SEXP map, SEXP keys);
extern SEXP C_hashmap_contains(SEXP map, SEXP keys);
extern SEXP C_hashmap_contains_range(SEXP map, SEXP keys);
extern SEXP C_hashmap_remove_range(SEXP map, SEXP keys);
extern SEXP C_hashmap_tolist(SEXP map);
extern SEXP C_hashmap_invert(SEXP map, SEXP duplicates);
extern SEXP C_hashmap_set_range(SEXP map, SEXP keys, SEXP values, SEXP replace);
extern SEXP C_hashmap_set(SEXP map, SEXP key, SEXP values, SEXP replace);
extern SEXP C_hashmap_clone(SEXP map);
extern SEXP C_hashmap_fromlist(SEXP map, SEXP list);

static const R_CallMethodDef CallEntries[] = {
    {"C_hashmap_set", (DL_FUNC) &C_hashmap_set, 4}, 
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
    {"C_hashmap_set_range", (DL_FUNC) &C_hashmap_set_range, 4},
    {"C_hashmap_remove_range", (DL_FUNC) &C_hashmap_remove_range, 2},
    {"C_hashmap_tolist", (DL_FUNC) &C_hashmap_tolist, 1},
    {"C_hashmap_invert", (DL_FUNC) &C_hashmap_invert, 2},
    {"C_hashmap_clone", (DL_FUNC) &C_hashmap_clone, 1},
    {"C_hashmap_fromlist", (DL_FUNC) &C_hashmap_fromlist, 2},
    {NULL, NULL, 0}
};


void R_init_Chashmap(DllInfo *dll) {
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL); 
    R_useDynamicSymbols(dll, FALSE);

}