#' @useDynLib hashmapR, .registration=TRUE

#' @export
`[<-.hashmap` <- function(map, key, value) {
  map$set(key, value, replace = FALSE, vectorize = FALSE)
}

#' @export
`[.hashmap` <- function(map, val) {
  map$get(val, vectorize = FALSE)
}

#' Create a Hashmap Object
#'
#' A \code{hashmap} provides a key-value store where keys and values
#' can be any R object that is serializable. Internally, it uses a
#' C++ \code{std::unordered_map} for efficient storage and lookup.
#'
#' ## Serialization
#' External pointers cannot be directly serialized in R. To save or
#' transfer a hashmap, you can convert it to a list using
#' \code{map$to_list()}, which returns a standard R list representation.
#' You can restore a hashmap from such a list using \code{map$from_list(...)}.
#'
#' ## Key Equality
#' Keys are considered equal if \code{identical(k1, k2) == TRUE}.
#' This strict equality means that numeric objects like \code{1L} (integer)
#' and \code{1} (double) are not considered equal.
#'
#' ### Memory
#' As R uses copy-on-write, the hashmap does not make a copy of keys and values that are inserted but only stores a reference to them.
#' Thus, operations such as cloning a hashmap or inserting existing objects into a hashmap come at very little memory overhead.
#' If any data structure is inserted that does not follow copy-on-write semantics (unlikely),
#' it is up to you to copy the object before insertion should you wish for the hashmap to take ownership of the data.
#' 
#' The same applies for all data returned from the hashmap (eg. through $keys(), $values(), $get()).
#' If any returned element does not follow copy-on-write semantics, changes to the data will result in changes to the data in the hashmap.
#'
#' ## Methods
#' A hashmap object exposes the following methods:
#' \describe{
#' \item{\code{map$set(key, value, replace = FALSE, vectorize = FALSE)}}{
#' Set the value associated with a key in the hashmap.
#'
#' If \code{replace=TRUE}, the value will replace the old value in case the map already
#' contains an element with the same key.
#'
#' If \code{vectorize = TRUE}, both \code{key} and \code{value} must be lists of the same length.
#' Each element of the \code{key} list is paired with the corresponding element of the \code{value} list,
#' and all pairs are inserted into the hashmap.
#' }
#'
#' \item{\code{map$get(key, vectorize = FALSE)}}{
#' Retrieve the value associated with a key. Returns NULL if the elment does not exist
#'
#' If \code{vectorize=TRUE}, \code{key} must be a list and each element of the \code{key} list will be looked up separately.
#' The result is returned as a list.
#' }
#'
#' \item{\code{map$delete(key, vectorize = FALSE)}}{
#' Remove a key-value pair from the map.
#'
#' If \code{vectorize=TRUE}, \code{key} must be a list. Each element in the list will be deleted separately.
#' }
#'
#' \item{\code{map$contains(key, vectorize=FALSE)}}{
#'
#' Check if a key exists in the map. Returns TRUE/FALSE.
#'
#' If \code{vectorize=TRUE}, \code{key} must be a list. Existense will be checked
#' for each item in the list.
#' }
#'
#' \item{\code{map$keys()}}{Returns a list of all keys.}
#' \item{\code{map$values()}}{Returns a list of all values.}
#' \item{\code{map$to_list()}}{
#'  Serialize the hashmap to a standard R list.
#'  Returns a list with two sublits (keys, values)
#' }
#' \item{\code{map$from_list(lst)}}{
#' Restore hashmap contents from a list.
#' Returns the restored hashmap
#' }
#'
#' \item{\code{map$clone()}}{
#' Returns a duplicate of the map
#' }
#'
#' \item{\code{map$invert(duplicates=c("stack", "first"))}}{
#' Invert the hashmap by swapping keys and values. Returns a new hashmap where
#' the original values become keys and the original keys become values.
#'
#' The \code{duplicates} parameter controls how to handle cases where multiple keys
#' map to the same value:
#' \itemize{
#'   \item \code{"first"}: Keep only the first key encountered for each value
#'   \item \code{"stack"}: Store all keys that map to the same value as a list
#' }
#' }
#' }
#'
#' @examples
#' # create hashmap object
#' library(hashmapR)
#' m <- hashmap()
#'
#' # insert key, value pair into hashmap
#' # any serializable R value can be uesd as a key and value
#' m$set("key", "value")
#' m$set(1, 2)
#' m$set(mtcars, Sys.Date())
#'
#' #' # alternative to $set()
#' m["key"] <- "value"
#'
#' # insert key, value pairs (vectorized)
#' # if vectorized is not set to TRUE, the list itself is inseted as a single key / value
#' m$set(list(1, 2, 3), list("one", "two", "three"), vectorize = TRUE)
#'
#' # retreive values
#' m$get(1)
#' m$get(list(1, 2, 3), vectorize = TRUE)
#'
#' #' # alternatively (for single lookups)
#' m[1]
#'
#' # remove values
#' m$remove(1)
#' m$remove(mtcars)
#'
#' # remove values (vectorized)
#' m$remove(list(1, 2, 3), vectorize = TRUE)
#'
#' # return number of items in map
#' m$size()
#'
#' # return keys as a list
#' m$keys()
#'
#' # return values as a list
#' m$values()
#'
#' # clear map
#' m$clear()
#'
#' # duplicate map
#' m$clone()
#'
#' # invert map (stack duplicates)
#' m$invert(duplicates = "stack")
#'
#' @return A list of class \code{hashmap} which contains an external pointer object (the hashmap) and methods to access and store elements in the map.
#' @export
hashmap <- function() {
  ptr <- .Call("C_hashmap_init")
  return(hashmap_init(ptr))
}

hashmap_init <- function(ptr) {

  self <- list() # add a class for printing
  class(self) <- "hashmap"
  self$.ptr <- ptr

  # primary methods
  self$get <- function(key, vectorize = FALSE) {
    if (vectorize) {
      stopifnot(is.list(key))
      vals <- .Call("C_hashmap_get_range", self$.ptr, key)
    } else {
      vals <- .Call("C_hashmap_get", self$.ptr, key)
    }
    return(vals)
  }

  #idea: pub: dupplicates: = c("ignore", "replace", "append") // append is effectively multimap
  self$set <- function(key, value, replace = FALSE, vectorize = FALSE) {
    if (vectorize) {
      if (is.list(key) && is.list(value)) {
        if (length(key) != length(value)) {
          stop("key and value must have the same length when vectorize=TRUE")
        } else {
          .Call("C_hashmap_set_range", self$.ptr, key, value, replace)
        }
      } else {
        stop("key and value must be lists when vectorize = TRUE")
      }
    } else {
      .Call("C_hashmap_set", self$.ptr, key, value, replace)
    }
    invisible(self)
  }

  self$contains <- function(key, vectorize = FALSE) {
    if (vectorize) {
      if (is.list(key)) {
        .Call("C_hashmap_contains_range", self$.ptr, key)
      } else {
        stop("key must be a list when vectorize = TRUE")
      }
    } else {
      .Call("C_hashmap_contains", self$.ptr, key)
    }
  }

  self$keys <- function() {
    .Call("C_hashmap_getkeys", self$.ptr)
  }

  self$values <- function() {
    .Call("C_hashmap_getvals", self$.ptr)
  }

  self$to_list <- function() {
    .Call("C_hashmap_tolist", self$.ptr)
  }

  self$clear <- function() {
    .Call("C_hashmap_clear", self$.ptr)
    invisible(self)
  }

  self$size <- function() {
    .Call("C_hashmap_size", self$.ptr)
  }

  self$invert <- function(duplicates = "first") {
    # returns new extptr object
    if (!duplicates %in% c("first", "stack")) {
      stop("duplicates must be first or stack")
    }

    new_ptr <- .Call("C_hashmap_invert", self$.ptr, duplicates)
    new_map <- hashmap_init(new_ptr)
    return(new_map)
  }

  self$clone <- function() {
    ptr <- .Call("C_hashmap_clone", self$.ptr)
    return(hashmap_init(ptr))
  }

  self$remove <- function(key, vectorize = FALSE) {
    if (vectorize) {
      if (is.list(key)) {
        .Call("C_hashmap_remove_range", self$.ptr, key)
      } else {
        stop("key must be a list if vectorize = TRUE")
      }
    } else {
      .Call("C_hashmap_remove", self$.ptr, key)
    }
    invisible(self)
  }

  # document as external function -> takes self as param (documented if not exported ? )
  self$from_list <- function(list) {
    # test structure
    stopifnot(length(list) == 2)
    stopifnot(names(list[1]) == "keys")
    stopifnot(names(list[2]) == "values")
    stopifnot(typeof(list[[1]]) == "list")
    stopifnot(typeof(list[[2]]) == "list")
    stopifnot(length(list[1]) == length(list[2]))
    .Call("C_hashmap_fromlist", self$.ptr, list)
    invisible(self)
  }
  return(self)
}

#' @export
print.hashmap <- function(x, ...) {
  cat(sprintf("Hashmap: Size %d\n", x$size()))
}

