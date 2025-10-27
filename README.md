# hashmap
A fast, vectorized hashmap implementation for R built as a wrapper wrapper around C++ std::unordered_map

The hashmap allows for the insertion of any key, value as long as it is serializable.
Equlity is tested with ...

Inserting and accessing elements is similar to how you would access and assign elements to a vector in R.
``
```


## Installation 

```R
devtools::install_github("svensglinzChashmap")
```

or via CRAN directly:
install.package("hashmap")


# Usage
```r
# initialize hashmap
map <- hashmap::hashmap()
```

## Insertion, lookup and removal

```r
# add (key, value) pair
map[KEY] = VALUE
map$set(KEY, VALUE) # if KEY already exists, VALUE is not inserted
map$set(KEY, VALUE, override=TRUE) # explicitly override old value if KEY already exists
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

map$values()

l <- map$to_list()
s <- serialize(l)

l <- unserialize(s)
map <- map$from_list(l)

map$size()
map$clear()
```

## Alternatives


Comparison between this package and the package r2r, which is purely implemented in R and makes use 
of R's environments which are implemented as hashmaps.

The only other c / c++ implementation I have found seems to be 
[this one](https://github.com/nathan-russell/hashmap), which however 
does 
