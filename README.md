# hashmapR
A fast, vectorized hashmap implementation for R built as a wrapper wrapper around C++ std::unordered_map

The hashmap allows for the insertion of any key, value as long as it is serializable.

## Installation 

```R
devtools::install_github("svensglinzChashmap")
```

or via CRAN directly:
```R
install.package("hashmapR")
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
