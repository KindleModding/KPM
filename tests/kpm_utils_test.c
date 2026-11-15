#include "internal_utils.h"
#include "kpm_test_common.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
    char *str = asprintf_hd("%s%i", "Hello, World!", 42);
    ASSERT_HD_STR(str, "Hello, World!42");
    free(str);

    mkdir_r("./test_folder/idk/idk2/idk3", 0777);
    ASSERT_HD(access("./test_folder/idk/idk2/idk3", R_OK), 0);
    int fd = open("./test_folder/idk/idk2/idk3/file", O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    ASSERT_HD(fd != -1, 1);
    ftruncate(fd, 1);
    close(fd);
    ASSERT_HD(access("./test_folder/idk/idk2/idk3/file", R_OK), 0);
    rmdir_r("./test_folder");
    ASSERT_HD(access("./test_folder/idk/idk2/idk3/file", R_OK), -1);
    ASSERT_HD(access("./test_folder/idk/idk2/idk3", R_OK), -1);
    ASSERT_HD(access("./test_folder/idk/idk2/", R_OK), -1);
    ASSERT_HD(access("./test_folder/idk/", R_OK), -1);
    ASSERT_HD(access("./test_folder/", R_OK), -1);

    mkdir_r("./test_folder/idk/idk2/idk3/", 0777);
    ASSERT_HD(access("./test_folder/idk/idk2/idk3", R_OK), 0);
    rmdir_r("./test_folder");
    ASSERT_HD(access("./test_folder/idk/idk2/idk3", R_OK), -1);
    ASSERT_HD(access("./test_folder/idk/idk2/", R_OK), -1);
    ASSERT_HD(access("./test_folder/idk/", R_OK), -1);
    ASSERT_HD(access("./test_folder/", R_OK), -1);
}