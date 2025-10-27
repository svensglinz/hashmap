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

    # vectorized inserts
    m[as.list(1:1e5), vectorize = TRUE] = as.list(rep("i", 1e5))
    expect_identical(m$size(), 1e5 + 5)

    expect_identical(m[12345L], "i")
    expect_error(m[list(1, 2, 3), vectorize = TRUE] <- list(1, 2))
    expect_error(m[c(1, 2, 3), vectorize = TRUE] <- list(1, 2, 3))
    m$clear()
    expect_identical(m$size(), 0)
})

test_that("deletion", {
    m <- hashmap()
    m[as.list(1:100), vectorize = TRUE] <- as.list(1:100)
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
    m[list(1, 2, 3), vectorize = TRUE] <- list(1, 2, 3)
    expect_identical(m[list(1, 2, 3), vectorize = TRUE], list(1, 2, 3))
    expect_error(m[100, vectorize = TRUE] <- list(1))
    expect_identical(m$size(), 4)
})

