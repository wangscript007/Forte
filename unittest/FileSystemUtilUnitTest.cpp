#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/regex.hpp>
#include "FileSystemUtil.h"
#include "MockProcRunner.h"

using namespace Forte;
using namespace std;

LogManager logManager;

class FileSystemUtilTest : public ::testing::Test
{
protected:
    static void SetUpTestCase() {
        mProcRunner = new MockProcRunner();
    }

    static void TearDownTestCase() {
        delete mProcRunner;
    }

    static MockProcRunner *mProcRunner;

    void SetUp() {
        logManager.BeginLogging("FileSystemUtilUnitTest.log");
    }
};
MockProcRunner * FileSystemUtilTest::mProcRunner = NULL;


TEST_F(FileSystemUtilTest, FormatProcRunnerCommandIsCorrect)
{
    FTRACE;
    mProcRunner->SetCommandResponse("/sbin/mkfs.fakefs  /dev/test", "", 0);
    FileSystemUtil fsutil(*mProcRunner);
    ASSERT_NO_THROW(fsutil.Format("/dev/test", "fakefs", false));
}

TEST_F(FileSystemUtilTest, FormatProcRunnerCommandIsCorrectWhenForceIsTrue)
{
    FTRACE;
    mProcRunner->SetCommandResponse("/sbin/mkfs.fakefs -F /dev/test", "", 0);
    FileSystemUtil fsutil(*mProcRunner);
    ASSERT_NO_THROW(fsutil.Format("/dev/test", "fakefs", true));
}

TEST_F(FileSystemUtilTest, FormatThrowsEDeviceFormat)
{
    FTRACE;
    mProcRunner->SetCommandResponse("/sbin/mkfs.fakefs -F /dev/test", "", 1);
    FileSystemUtil fsutil(*mProcRunner);
    ASSERT_THROW(fsutil.Format("/dev/test", "fakefs", true), EDeviceFormat);
}

TEST_F(FileSystemUtilTest, MountProcRunnerCommandIsCorrect)
{
    FTRACE;

    mProcRunner->SetCommandResponse(
        "/bin/mount -o '' -t fakefs /dev/test /home/test", "", 0);
    FileSystemUtil fsutil(*mProcRunner);
    ASSERT_NO_THROW(fsutil.Mount("fakefs", "/dev/test", "/home/test", ""));
}

TEST_F(FileSystemUtilTest, MountThrowsEDeviceMount)
{
    FTRACE;

    mProcRunner->SetCommandResponse(
        "/bin/mount -o '' -t fakefs /dev/test /home/test", "", 1);
    FileSystemUtil fsutil(*mProcRunner);
    ASSERT_THROW(fsutil.Mount("fakefs", "/dev/test", "/home/test", ""),
                 EDeviceMount);
}

TEST_F(FileSystemUtilTest, UnmountProcRunnerCommandIsCorrect)
{
    FTRACE;

    mProcRunner->SetCommandResponse("/bin/umount /home/test",
                                    "", 0);
    FileSystemUtil fsutil(*mProcRunner);
    ASSERT_NO_THROW(fsutil.Unmount("/home/test"));

}

TEST_F(FileSystemUtilTest, UnmountThrowsEDeviceUnmount)
{
    FTRACE;

    mProcRunner->SetCommandResponse("/bin/umount /home/test",
                                    "", 1);
    FileSystemUtil fsutil(*mProcRunner);
    ASSERT_THROW(fsutil.Unmount("/home/test"), EDeviceUnmount);
}

