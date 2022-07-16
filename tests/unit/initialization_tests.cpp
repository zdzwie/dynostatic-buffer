#include <gtest/gtest.h>

#include "dynostatic-buffer.h"
#include "ds-defs.h"

void test_logger(const char *message, size_t len)
{
    if (len == 0) {
        return;
    }

    printf("%s", message);
}

TEST(InitializationTests, TwiceInitialize)
{
    ds_err_code_t ret = EDS_OK;

    ret = ds_initialize_allocation(test_logger);
    EXPECT_EQ(ret, EDS_OK);

    ret = ds_initialize_allocation(test_logger);
    EXPECT_EQ(ret, EDS_ALREADY_INIT);

    ds_deinit_allocation();
}

TEST(InitializationTests, Deinit)
{
    ds_err_code_t ret = EDS_OK;

    ret = ds_initialize_allocation(test_logger);
    EXPECT_EQ(ret, EDS_OK);

    ds_deinit_allocation();

    ret = ds_initialize_allocation(test_logger);
    EXPECT_EQ(ret, EDS_OK);

    ds_deinit_allocation();
}

TEST(InitializationTests, BadLogger)
{
    ds_err_code_t ret = EDS_OK;

    ret = ds_initialize_allocation(NULL);
    EXPECT_EQ(ret, EDS_INVALID_PARAMS);

    ds_deinit_allocation();
}
