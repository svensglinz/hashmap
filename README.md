# Chashmap
A fully vectorized hashmap implementation for R built as a simple wrapper around c++ unordered map

Chashmap alows for the insertion of integer, reals (SEXPINT, SEXPREAL) and strings (SEXPSTR). The insertion of any other type of data will throw an error.

Inserting and accessing elements is similar to how you would access and assign elements to a vector in R.
```
**Integers and Reals of the same value (eg 1, 1L) are treated as different values.**
For example, `1:100` is a vector of integers, but c(1, 2, ... 100) a vector of reals. 
```


## Installation 

```R
devtools::install_github("svensglinz/Chashmap")
```

# Usage
```r
# initialize hashmap
map <- Chashmap::hashmap()
```

## Insertion, lookup and removal

all operations are fully vectorized (ie. KEY and VALUE can be vectors of the types described above and the map will insert them as key value pairs of scalars) 
```r
# add (key, value) pair
map[KEY] = VALUE
insert(map, KEY, VALUE)

# vectorized example
map[c(1, 2, 100)] <- c("one", "two", "onehundred")
insert(map, c(1, 2, 100), c("one", "two", "onehundred"))

```

```r
# retrieve keys and returns them as a list
# returns NULL if no element is found
map[KEY]
get(map, KEY)

map[c(1, 10)]
## [[1]]
## [1] "one"
## [[2]]
## [2] NULL
```

```r
# remove keys from a map 
remove(map, KEYS)

```
## Utility Functions
```r
# return all keys in the map as a list
#simplify = TRUE returns a vector (beware that this may lead to datatype coercion if not all keys are of the same type)

keys(map, simplify = FALSE)
## [[1]]
## [1] 1
## [[2]]
## [2] 2
## [[3]]
## [3] 100

# return all values in the map as a list
#simplify = TRUE returns a vector (beware that this may lead to datatype coercion if not all values are of the same type)

values(map, simplify = TRUE)
## [1] "one" "two" "onehundred"

# returns the size of the hashmap (# of unique entries)

size(map)
## [1] 3

# clear all entries in the map
clear(map)
```

## Benchmarks 

Comparison between this package and the package r2r, which is purely implemented in R and makes use 
of R's environments which are implemented as hashmaps.

The only other c / c++ implementation I have found seems to be 
[this one](https://github.com/nathan-russell/hashmap), which however 
does not seem to compile anymore for many and on my machine makes my R session crash. 

### 1. Insertion of keys 

![](utils/b1.png)

### 2. Lookup of keys 
![](utils/b2.png)

