# hashmapR

<!-- badges: start -->
[![CRAN status](https://www.r-pkg.org/badges/version/hashmapR)](https://CRAN.R-project.org/package=hashmapR) [![R-CMD-check](https://github.com/svensglinz/hashmapR/actions/workflows/R-CMD-check.yaml/badge.svg)](https://github.com/svensglinz/hashmapR/actions/workflows/R-CMD-check.yaml)
<!-- badges: end -->


A fast, vectorized hashmap implementation for R built as a wrapper wrapper around C++ std::unordered_map

The hashmap allows for the insertion of any key, value as long as it is serializable.

### Memory

As R uses copy-on-write, the hashmap does not make a copy of keys and values that are inserted but only stores a reference to them. Thus, operations such as cloning a hashmap or inserting existing objects into a hashmap come at very little memory overhead.

If any data structure is inserted that does not follow copy-on-write semantics (unlikely), it is up to you to copy the object before insertion should you wish for the hashmap to take ownership of the data.

The same applies for all data returned from the hashmap (eg. through $keys(), $values(), $get()). If any returned element does not follow copy-on-write semantics, changes to the data will result in changes to the data in the hashmap.


### Serialization

External pointers cannot be directly serialized in R. To save or
transfer a hashmap, you can convert it to a list using
`map$to_list()`, which returns a standard R list representation.
You can restore a hashmap from such a list using `map$from_list(...)`

### Key Equality

Keys are considered equal if `identical(k1, k2) == TRUE`.
This strict equality means that numeric objects like `1L` (integer)
and `1` (double) are not considered equal.

## Installation 

```R
devtools::install_github("svensglinz/hashmapR")
```

or via CRAN directly:

```R
install.packages("hashmapR")
```

# Usage
```r
# initialize hashmap
map <- hashmapR::hashmap()
```

## Insertion, lookup and removal

```r
# add (key, value) pair
map[KEY] = VALUE
map$set(KEY, VALUE) # if KEY already exists, VALUE is not inserted
map$set(KEY, VALUE, replace=TRUE) # explicitly override old value if KEY already exists
map$set(list(K1,K2,K3), list(V1,V2,V3), vectorize=TRUE) # insert multiple values
```

```r
# remove elements
map$remove(KEY)
map$remove(list(K1,K2,K3), vectorize=TRUE) # remove multiple values
```

```r
# lookup elements
map[KEY]
map$get(KEY)
map$get(list(K1,K2,K3), vectorize=TRUE) #retreive multiple values
```

## Utility Functions
```r
# return all keys in the map as a list
map$keys()

# return all values in the map as a list
map$values()

# return the map as a list / create map from list
# this can be helpful for serialization/ unserialization
map_list <- map$to_list()
map_list_serialized <- serialize(map_list)

map_list <- unserialize(map_list_serialized)
map <- map$from_list(map_list)

# get number of (key, value) pairs in map
map$size()

# clear the map
map$clear()

# duplicate the map 
map$clone()

# invert the key, value pair mappings
# If multiple keys exist for a value on the original map, "first" picks the first 
# one to be the new value, "stack" packs them into a list
map$invert(duplicates="first")
```
