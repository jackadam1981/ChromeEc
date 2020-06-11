#include <stdbool.h>
#include "test_util.h"

static bool some_function(void)
{
    return true;
}

/* Write a function with the following signature: */
test_static int test_my_function(void)
{
    /* Run some code */
    bool condition = some_function();

    /* Check that the expected condition is correct. */
    TEST_EQ(condition, true, "%d");

    return EC_SUCCESS;
}

/* The test framework will call the function named "run_test" */
void run_test(int argc, char **argv)
{
    /* Each unit test can be run using the RUN_TEST macro: */
    RUN_TEST(test_my_function);

    /* Report the results of all the tests at the end. */
    test_print_result();
}
