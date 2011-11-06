#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/filesystem/convenience.hpp>
#include "LogManager.h"
#include "DbLiteConnection.h"
#include "DbConnectionPool.h"
#include "DbSqlStatement.h"

using namespace Forte;

#define CREATE_TEST_TABLE                       \
    "CREATE TABLE `test` ("                     \
    "  `a` int(10) not null default '0',"       \
    "  `b` int(10) not null default '0',"       \
    "  PRIMARY KEY (`a`)"                       \
    ");"

#define SELECT_TEST_TABLE                       \
    "SELECT * FROM test"


class DatabasePoolTest : public ::testing::Test
{
protected:
    virtual void CreateDatabases()
    {
    }

    virtual void CreateTables()
    {
    }

    virtual size_t PopulateData()
    {
        return 0;
    }

    virtual void RemoveDatabases()
    {
    }

    virtual const char* getDatabaseName(const char* str="")=0;

    virtual void SetUp()
    {
        mLogManager.BeginLogging("//stderr");

        {
            hlog(HLOG_TRACE, "{");

            RemoveDatabases();
            CreateDatabases();
            CreateTables();

            hlog(HLOG_TRACE, "} ");
        }
    }

    virtual void TearDown()
    {
        hlog(HLOG_TRACE, "{");

        RemoveDatabases();

        hlog(HLOG_TRACE, "}");
    }

private:
    LogManager mLogManager;
};


class BasicDatabasePoolTest : public DatabasePoolTest
{
protected:
    typedef DatabasePoolTest base_type;

    virtual void CreateDatabases()
    {
        DbLiteConnection db (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
        EXPECT_TRUE(db.Init(getDatabaseName()));
    }

    virtual void CreateTables()
    {
        DbLiteConnection db (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
        EXPECT_TRUE(db.Init(getDatabaseName()));
        EXPECT_TRUE(db.Execute(CREATE_TEST_TABLE));
    }

    virtual size_t PopulateData(DbConnection& db)
    {
        const size_t rows(100);
        for (int i = 0; i < rows; i++)
        {
            EXPECT_TRUE(db.Execute(FString(FStringFC(),"INSERT INTO test VALUES (%d, %d)", i, rand())));
        }

        return rows;
    }

    virtual size_t PopulateData()
    {
        DbLiteConnection db (SQLITE_OPEN_READWRITE| SQLITE_OPEN_CREATE);
        EXPECT_TRUE(db.Init(getDatabaseName()));

        return PopulateData(db);
    }

    virtual void RemoveDatabases()
    {
        unlink(getDatabaseName());
    }

    virtual void SetUp()
    {
        base_type::SetUp();
    }

    const char* getDatabaseName(const char* str="")
    {
        string databasePath("/tmp/");
        databasePath += boost::filesystem::basename(__FILE__);
        databasePath += ".unittest.db";
        return databasePath.c_str();
    }
};

/*
 * SQLITE Database Pool Tests
 */

TEST_F(BasicDatabasePoolTest, SqliteConstructDatabaseConnectionPool)
{
    ASSERT_NO_THROW(DbConnectionPool pool("sqlite", getDatabaseName()));
}

TEST_F(BasicDatabasePoolTest, SqliteConstructInvalidDatabaseConnectionPool)
{
    ASSERT_THROW(DbConnectionPool pool("not-a-real-type-of-connection", getDatabaseName()), Forte::EDbConnectionPool);
}

TEST_F(BasicDatabasePoolTest, SqliteGetDatabaseConnectionTest)
{
    DbConnectionPool pool("sqlite", getDatabaseName());
    DbConnection& dbConnection(pool.GetDbConnection());
    dbConnection.Connect();
}

TEST_F(BasicDatabasePoolTest, SqliteUseDatabaseConnectionTest)
{
    size_t rows(0);

    DbConnectionPool pool("sqlite", getDatabaseName());
    DbConnection& dbConnection(pool.GetDbConnection());

    ASSERT_NO_THROW(rows=PopulateData(dbConnection));

    DbResult res = dbConnection.Store(SELECT_TEST_TABLE);
    EXPECT_EQ(res.GetNumRows(), rows);
}

TEST_F(BasicDatabasePoolTest, SqliteReleaseDatabaseConnectionTest)
{
    DbConnectionPool pool("sqlite", getDatabaseName());
    DbConnection& dbConnection(pool.GetDbConnection());
    ASSERT_NO_THROW(pool.ReleaseDbConnection(dbConnection));
}

TEST_F(BasicDatabasePoolTest, SqliteDoubleReleaseDatabaseConnectionTest)
{
    DbConnectionPool pool("sqlite", getDatabaseName(), "", "", "", "", "", 1);
    DbConnection& dbConnection(pool.GetDbConnection());
    ASSERT_NO_THROW(pool.ReleaseDbConnection(dbConnection));
    ASSERT_THROW(pool.ReleaseDbConnection(dbConnection), Forte::EDbConnectionPool);
}

TEST_F(BasicDatabasePoolTest, SqliteDeleteUnacquiredDatabaseConnectionsTest)
{
    DbConnectionPool pool("sqlite", getDatabaseName());
    DbConnection& dbConnection(pool.GetDbConnection());
    ASSERT_NO_THROW(pool.ReleaseDbConnection(dbConnection));
    ASSERT_NO_THROW(pool.DeleteConnections());
}

TEST_F(BasicDatabasePoolTest, SqliteDeleteAcquiredDatabaseConnectionsTest)
{
    DbConnectionPool pool("sqlite", getDatabaseName());
    DbConnection& dbConnection(pool.GetDbConnection());
    dbConnection.Connect();
    ASSERT_THROW(pool.DeleteConnections(), Forte::EDbConnectionPoolOpenConnections);
}

/*
 * SQLITE Mirrored Database Pool Tests against existing Database Pool Tests
 */

TEST_F(BasicDatabasePoolTest, SqliteMirroredConstructDatabaseConnectionPool)
{
    ASSERT_NO_THROW(DbConnectionPool pool("sqlite_mirrored", getDatabaseName()));
}

TEST_F(BasicDatabasePoolTest, SqliteMirroredGetDatabaseConnectionTest)
{
    DbConnectionPool pool("sqlite_mirrored", getDatabaseName());
    DbConnection& dbConnection(pool.GetDbConnection());
    dbConnection.Connect();
}

TEST_F(BasicDatabasePoolTest, SqliteMirroredUseDatabaseConnectionTest)
{
    DbConnectionPool pool("sqlite_mirrored", getDatabaseName());
    DbConnection& dbConnection(pool.GetDbConnection());

    size_t rows(0);
    ASSERT_NO_THROW(rows=PopulateData(dbConnection));

    DbResult res = dbConnection.Store(SELECT_TEST_TABLE);
    EXPECT_EQ(res.GetNumRows(), rows);
}

TEST_F(BasicDatabasePoolTest, SqliteMirroredOverloadStoreDatabaseConnectionTest)
{
    DbConnectionPool pool("sqlite_mirrored", getDatabaseName());
    DbConnection& dbConnection(pool.GetDbConnection());

    size_t rows(0);
    ASSERT_NO_THROW(rows=PopulateData(dbConnection));

    DbResult res = dbConnection.Store(SelectDbSqlStatement(SELECT_TEST_TABLE));
    EXPECT_EQ(res.GetNumRows(), rows);
}

TEST_F(BasicDatabasePoolTest, SqliteMirroredReleaseDatabaseConnectionTest)
{
    DbConnectionPool pool("sqlite_mirrored", getDatabaseName());
    DbConnection& dbConnection(pool.GetDbConnection());
    ASSERT_NO_THROW(pool.ReleaseDbConnection(dbConnection));
}

TEST_F(BasicDatabasePoolTest, SqliteMirroredDoubleReleaseDatabaseConnectionTest)
{
    DbConnectionPool pool("sqlite_mirrored", getDatabaseName(),"","","","","",1);
    DbConnection& dbConnection(pool.GetDbConnection());
    ASSERT_NO_THROW(pool.ReleaseDbConnection(dbConnection));
    ASSERT_THROW(pool.ReleaseDbConnection(dbConnection), Forte::EDbConnectionPool);
}

TEST_F(BasicDatabasePoolTest, SqliteMirroredDeleteUnacquiredDatabaseConnectionsTest)
{
    DbConnectionPool pool("sqlite_mirrored", getDatabaseName());
    DbConnection& dbConnection(pool.GetDbConnection());
    ASSERT_NO_THROW(pool.ReleaseDbConnection(dbConnection));
    ASSERT_NO_THROW(pool.DeleteConnections());
}

TEST_F(BasicDatabasePoolTest, SqliteMirroredDeleteAcquiredDatabaseConnectionsTest)
{
    DbConnectionPool pool("sqlite_mirrored", getDatabaseName());
    DbConnection& dbConnection(pool.GetDbConnection());
    dbConnection.Connect();
    ASSERT_THROW(pool.DeleteConnections(), Forte::EDbConnectionPoolOpenConnections);
}