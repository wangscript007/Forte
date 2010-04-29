#ifndef FORTE_NO_DB
#ifndef FORTE_NO_SQLITE

#include "Forte.h"
#include "DbLiteConnection.h"
#include "DbLiteResult.h"
#include "DbException.h"

DbLiteConnection::DbLiteConnection()
:
    DbConnection()
{
    mDBType = "sqlite";
    mDB = NULL;
    mFlags = SQLITE_OPEN_READWRITE;
}


DbLiteConnection::~DbLiteConnection()
{
    Close();
}


void DbLiteConnection::set_error()
{
    if (mDB != NULL)
    {
        mErrno = sqlite3_errcode(mDB);
        mError = sqlite3_errmsg(mDB);
    }
    else
    {
        mErrno = SQLITE_ERROR;
        mError = "Connection is gone.";
    }
}


bool DbLiteConnection::Init(struct sqlite3 *db)
{
    mDB = db;
    mDidInit = true;
    mReconnect = false;
    return true;
}


bool DbLiteConnection::Connect()
{
    int err;

    if (mDB != NULL && !Close()) return false;

    err = sqlite3_open_v2(mDBName, &mDB, mFlags, NULL);

    if (err == SQLITE_OK)
    {
        sqlite3_busy_timeout(mDB, 250);
        return true;
    }

    if (mDB != NULL)
    {
        set_error();
    }
    else
    {
        mErrno = SQLITE_NOMEM;
        mError = "Out of memory";
    }

    return false;
}


bool DbLiteConnection::Close()
{
    if (sqlite3_close(mDB) == SQLITE_OK)
    {
        mDB = NULL;
        return true;
    }

    set_error();
    return false;
}


DbResult DbLiteConnection::query(const FString& sql)
{
    unsigned int tries_remaining = mRetries + 1;
    struct timeval tv_start, tv_end;
    DbLiteResult res;
    sqlite3_stmt *stmt = NULL;
    FString remain = sql;
    const char *tail = NULL;

    // init
    mTries = 0;
    mErrno = SQLITE_OK;
    mError.clear();

    if (mDB == NULL)
    {
        set_error();
        return res;
    }

    if (mAutoCommit == false) mQueriesPending = true;

    // while queries remain in the given sql, and we have some tries left...
    while (remain.length() && tries_remaining > 0)
    {
        // prepare statement
        mErrno = sqlite3_prepare_v2(mDB, remain, remain.length(), &stmt, &tail);
        if (stmt == NULL) break;

        do
        {
            // run query
            mTries++;
            gettimeofday(&tv_start, NULL);
            mErrno = res.Load(stmt);
            gettimeofday(&tv_end, NULL);
            if (sDebugSql) logSql(remain, tv_end - tv_start);

            // check for errors
            if (mErrno != SQLITE_OK)
            {
                res.Clear();

                switch (mErrno)
                {
                    //// soft failures, keep retrying:
                case SQLITE_BUSY:
                case SQLITE_LOCKED:
                    sqlite3_reset(stmt);
                    --tries_remaining;
                    break;

                    //// hard failures, just fail immediately:
                default:
                    tries_remaining = 0;
                    break;
                }
            }
        }
        while (!res && tries_remaining > 0);

        // cleanup statement
        sqlite3_finalize(stmt);
        stmt = NULL;

        // set remaining sql
        if (tail == NULL) remain.clear();
        else remain = tail;
    }

    if (!res) set_error();
    return res;
}


bool DbLiteConnection::execute(const FString& sql)
{
    return query(sql);
}


DbResult DbLiteConnection::store(const FString& sql)
{
    return query(sql);
}


DbResult DbLiteConnection::use(const FString& sql)
{
    return query(sql);
}


bool DbLiteConnection::isTemporaryError() const
{
    return (mErrno == SQLITE_BUSY ||
            mErrno == SQLITE_LOCKED);
}


FString DbLiteConnection::escape(const char *str)
{
    FString ret;
    char *tmp;

    if ((tmp = sqlite3_mprintf("%q", str)) != NULL)
    {
        ret = tmp;
        sqlite3_free(tmp);
    }

    return ret;
}

#endif
#endif
