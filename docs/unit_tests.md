# Unit Tests

[TOC]

## Running Unit Tests

The unit tests run on the host machine using the [`host` board].

## Writing Unit Tests

The unit test framework is similar to [Google Test]. Unit tests live in the
[`test`] subdirectory of the CrOS EC codebase.

Test-related macros (e.g., `TEST_ASSERT`) and functions are defined in
 [`test_util.h`].

`test/my_test.c`:
```c
/* Write a function with the following signature: */
test_static int test_my_function(void)
{
	/* Run some code */
	bool condition = some_function();

	/* Check that the expected condition is correct. */
	TEST_ASSERT(condition);

	return EC_SUCCESS;
}

```

`test/my_test.c`:
```c
/* The test framework will call the function named "run_test" */
void run_test(void)
{
	/* Each unit test can be run using the RUN_TEST macro: */
	RUN_TEST(test_my_function);

	/* Report the results of all the tests at the end. */
	test_print_result();
}

```

*** note
*TIP*: Unit tests should be independent from each other as much as possible.
This keeps the test (and any system state) simple to reason about and also
allows running unit tests in parallel.
***

## Mocks

[Mocks][`mock`] enable you to simulate behavior for parts of the system that
you're not directly testing. They can also be useful for testing specific edge
cases that are hard to exercise during normal use (e.g., error conditions).

### Mock Time

When writing unit tests that rely on a clock, it's best not to rely on a real
hardware clock. It's very difficult to enforce exact timing with a real clock,
which leads to test flakiness (and ignored tests). Instead, use the [Mock
Timer] to adjust the time during the test.


[`mock`]: https://chromium.googlesource.com/chromiumos/platform/ec/+/master/include/mock
[Mock Timer]: https://chromium.googlesource.com/chromiumos/platform/ec/+/master/include/mock/timer_mock.h
[`test`]: https://chromium.googlesource.com/chromiumos/platform/ec/+/master/test
[Google Test]: https://github.com/google/googletest
[`host` board]: https://chromium.googlesource.com/chromiumos/platform/ec/+/master/board/host/
[`test_util.h`]: https://chromium.googlesource.com/chromiumos/platform/ec/+/master/include/test_util.h
