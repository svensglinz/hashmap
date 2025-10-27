test_that("initialization", {
    m <- hashmap()
    expect_identical(m$size(), 0)
    m$clear()
    expect_identical(m$size(), 0)
})

test_that("insertion", {
    m <- hashmap()
    m[1] = 1
    m[2] = 2
    m[c(1, 2, 3)] = Sys.getenv # should be able to insert any R object
    m[mtcars] = as.Date("2022-01-01")
    m["key"] = "value"
    expect_identical(m[mtcars], as.Date("2022-01-01"))
    expect_identical(m[1], 1)
    expect_identical(m[2], 2)
    expect_identical(m[c(1, 2, 3)], Sys.getenv)
    expect_identical(m["key"], "value")
    expect_identical(m$size(), 5)

    # duplicate inserts
    m[1] = 100
    expect_identical(m[1], 1)
    m$set(1, 100, replace=TRUE)
    expect_identical(m[1], 100)

    # vectorized inserts
    m$set(as.list(1:1e5), as.list(rep("i", 1e5)), vectorize = TRUE)
    expect_identical(m$size(), 1e5 + 5)

    expect_identical(m[12345L], "i")
    expect_error(m$set(list(1, 2, 3), list(1, 2), vectorize=TRUE))
    expect_error(m$set(c(1, 2, 3), list(1, 2, 3), vectorize=TRUE))
    m$clear()
    expect_identical(m$size(), 0)
})

test_that("deletion", {
    m <- hashmap()
    m$set(as.list(1:100), as.list(1:100), vectorize=TRUE)
    m$remove(1) # should not remove as 1:100 are integers
    m$remove(1L)
    m$remove(2L)
    m$remove("test")
    m$remove("invalid")
    expect_identical(m$size(), 98)
    expect_null(m[1L])
    expect_null(m[2L])
    m$remove(as.list(1:10), vectorize = TRUE)
    expect_identical(m$size(), 90)
    expect_null(m[3L])
    expect_error(m$remove(c(1, 2, 3), vectorize = TRUE))
})
test_that("lookup", {
    m <- hashmap()
    expect_null(m[200])
    expect_null(m["test"])
    m[100L] <- 200
    expect_identical(m[100L], 200)
    expect_null(m[100])
    m$set(list(1, 2, 3), list(1, 2, 3))
    expect_identical(m$get(list(1, 2, 3)), list(1, 2, 3))
    expect_identical(m$size(), 2)
})

test_that("clone", {
    m <- hashmap()
    m[1] <- 1
    m[2] <- 2
    a <- m$clone()
    expect_identical(a$to_list(), m$to_list())
})

test_that("invert", {
    m <- hashmap()
    m$set(list(1, 2, 3), list(10, 10, 10), vectorize=TRUE)
    a <- m$invert(duplicates="first")
    expect_identical(a$size(), 1)
    expect_identical(length(a$values()[[1]]), 1L)
    
    b <- m$invert(duplicates="stack")
    expect_identical(b$size(), 1)
    expect_identical(length(b$values()[[1]]), 3L)
})

test_that("from_list", {
    m <- hashmap()
    m$set(100, 200)
    list <- m$to_list()
    expect_error(m$from_list(list(a = 100, b = 200))) # names should be keys, values
    expect_error(m$from_list(list("keys" = 100, "values" = 200, "other" = 300))) # other is redundant
    expect_no_error(m$from_list(list("keys" = list(100, 200), "values" = list(1, 2))))
    m$from_list(list("keys" = list(100, 200), "values" = list(1, 2)))
    expect_equal(m$size(), 2)
    expect_equal(m[100], 1)
    expect_equal(m[200], 2)
})