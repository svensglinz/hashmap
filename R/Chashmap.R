#' @useDynLib hashmap, .registration=TRUE

#' @export
`[<-.hashmap` <- function(map, key, vectorize=FALSE, value) {
  map$set(key, value, vectorize)
}

#' @export
`[.hashmap` <- function(map, val, vectorize=FALSE) {
  map$get(val, vectorize)
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
#' \item{\code{map$delete(key, vectorize = FALSE)}} {
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
#' }
#' 
#' @examples
#' # create hashmap object
#' library(hashmap)
#' m <- hashmap()
#' 
#' # insert key, value pair into hashmap
#' # any serializable R value can be uesd as a key and value
#' m$set("key", "value")
#' m$set(1, 2)
#' m$set(mtcars, Sys.Date())
#' 
#' # alternative to $set()
#' m["key"] <- "value"
#' 
#' # insert key, value pairs (vectorized)
#' # if vectorized is not set to TRUE, the list itself is inseted as a single key / value
#' m$set(list(1, 2, 3), list("one", "two", "three"), vectorize = TRUE)
#'
#' # alternatively
#' m[list(1, 2, 3), vectorize = TRUE] <- list("one", "two", "three")
#' 
#' # retreive values
#' m$get(1)
#' m$get(list(1, 2, 3), vectorize = TRUE)
#' # alternatively 
#' m[1]
#' m[list(1, 2, 3), vectorize = TRUE]
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
#' # return 
#' @export
hashmap <- function() {
  ptr = .Call("C_hashmap_init")
  return (hashmap_init(ptr))
}

hashmap_init <- function(ptr) {
  self <- new.env(parent = emptyenv()) # add a class for printing
  class(self) <- "hashmap"
  self$.ptr <- ptr

  # primary methods
  self$get <- function(key, vectorize=FALSE) {
    if (vectorize) {
        stopifnot(is.list(key))
        vals <- .Call("C_hashmap_get_range", self$.ptr, key)
      } else {
        vals <- .Call("C_hashmap_get", self$.ptr, key)
      }
      return(vals)
  }

  self$set <- function(key, value, vectorize=FALSE) {
      if (vectorize) {
        if (is.list(key) && is.list(value)) {
          if (length(key) != length(value)) {
            stop("key and value must have the same length when vectorize=TRUE")
          } else {
            .Call("C_hashmap_insert_range", self$.ptr, key, value)
          }
        } else {
            stop("key and value must be lists when vectorize = TRUE")
        }
      } else {
        .Call("C_hashmap_insert", self$.ptr, key, value)
      }
      invisible(self)
      }

  self$contains <- function(key, vectorize=FALSE) {
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
  }

  self$size <- function() {
    .Call("C_hashmap_size", self$.ptr)
  }

  self$for_each <- function(fn) {
    .Call("C_hashmap_for_each", self$.ptr)
  }

  self$invert <- function() {
    # returns new extptr object
    new_ptr <- .Call("C_hashmap_invert", self$.ptr)
    new_map <- hashmap_init(new_ptr)
    return (new_map)
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
    .Call("C_hashmap_from_list", self$.ptr, list)
  }

  return(self)
}

#' @export
print.hashmap <- function(x, ...) {
  glue::glue("Hashmap: Size {x$size()}")
}
