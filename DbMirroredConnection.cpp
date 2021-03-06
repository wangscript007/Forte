#include "DbMirroredConnection.h"
#include "DbConnectionFactory.h"
#include "DbSqlStatement.h"
#include "LogManager.h"
#include "FileSystemImpl.h"
#include <sys/inotify.h>
#include <sqlite3.h> //todo: remove after add exceptions to sqliteconnection
#include <boost/shared_ptr.hpp>
#include <stdio.h>

using namespace Forte;
using namespace boost;

DbMirroredConnection::DbMirroredConnection(boost::shared_ptr<DbConnectionFactory> primaryDbConnectionFactory,
                                           boost::shared_ptr<DbConnectionFactory> secondaryDbConnectionFactory,
                                           const FString& altDbName)
    : mPrimaryDbConnectionFactory(primaryDbConnectionFactory),
      mSecondaryDbConnectionFactory(secondaryDbConnectionFactory),
      mDbConnection(primaryDbConnectionFactory->create()),
      mAltDbName(altDbName)
{
}

bool DbMirroredConnection::isActiveSecondary() const
{
    return (mDbConnection == mDbConnectionSecondary);
}

bool DbMirroredConnection::isActivePrimary() const
{
    return ! isActiveSecondary();
}

bool DbMirroredConnection::setActiveSecondary()
{
    try
    {
        AutoUnlockMutex guard(mMutex);

        if(mDbConnection->HasPendingQueries())
        {
            throw EDbConnectionPendingFailed();
        }

        if(! mDbConnectionSecondary)
        {
            hlogstream(HLOG_DEBUG, "create a secondary DB connection");
            mDbConnectionSecondary.reset(mSecondaryDbConnectionFactory->create());
        }

        hlogstream(HLOG_DEBUG, "try to init secondary DB connection to " << mAltDbName);
        const bool ok(mDbConnectionSecondary->Init(mAltDbName, mDbConnection->mUser, mDbConnection->mPassword,
                      mDbConnection->mHost, mDbConnection->mSocket, 3));

        if(ok)
        {
            mDbConnection = mDbConnectionSecondary;
        }
        else
        {
            hlogstream(HLOG_ERR, "failed to init secondary DB connection");
        }

        return ok;
    }
    catch(EDbConnectionIoError& e)
    {
        hlogstream(HLOG_WARN, e.what());
        return false;
    }
    catch(EDbConnectionConnectFailed& e)
    {
        hlogstream(HLOG_WARN, e.what());
        return false;
    }

    return false;
}

bool DbMirroredConnection::Init(const FString& db, const FString& user, const FString& pass, const FString& host, const FString& socket, unsigned int retries)
{
    AutoUnlockMutex guard(mMutex);

    if(isActiveSecondary())
    {
        hlogstream(HLOG_DEBUG, "try to init secondary connection");

        try
        {
            const bool ret(mDbConnection->Init(mAltDbName, user, pass, host, socket, retries));
            return ret;
        }
        catch(EDbConnectionConnectFailed& e)
        {
            hlogstream(HLOG_WARN, e.what());
        }
        catch(EDbConnectionIoError& e)
        {
            hlogstream(HLOG_WARN, e.what());
        }
    }
    else
    {
        hlogstream(HLOG_DEBUG, "attempt to init primary DB connection");

        try
        {
            const bool ret(mDbConnection->Init(db, user, pass, host, socket, retries));
            if(ret)
            {
                FileSystemImpl fs;
                if(! mAltDbName.empty() && ! fs.FileExists(mAltDbName))
                {
                    const string dirName(fs.Dirname(mAltDbName));
                    if(! fs.FileExists(dirName))
                    {
                        fs.MakeFullPath(dirName);
                    }

                    mDbConnection->BackupDatabase(mAltDbName);
                }
            }
            return ret;
        }
        catch(EDbConnectionConnectFailed& e)
        {
            if (hlog_ratelimit(60))
                hlogstream(HLOG_WARN, e.what());
        }
        catch(EDbConnectionIoError& e)
        {
            if (hlog_ratelimit(60))
                hlogstream(HLOG_WARN, e.what());
        }
        if (hlog_ratelimit(60))
            hlog(HLOG_WARN, "failed to init primary DB Connection: '%s'",
                 db.c_str());

        return setActiveSecondary();
    }

    return false;
}

bool DbMirroredConnection::Connect()
{
    AutoUnlockMutex guard(mMutex);

    try
    {
        return (mDbConnection->Connect());
    }
    catch(EDbConnectionConnectFailed& e)
    {
        hlogstream(HLOG_WARN, e.what());
    }
    catch(EDbConnectionIoError& e)
    {
        hlogstream(HLOG_WARN, e.what());
    }

    if(isActivePrimary())
    {
        if(setActiveSecondary())
        {
            return Connect();
        }
    }

    return false;
}

bool DbMirroredConnection::Close()
{
    AutoUnlockMutex guard(mMutex);

    try
    {
        return mDbConnection->Close();
    }
    catch(EDbConnectionIoError& e)
    {
        hlogstream(HLOG_WARN, e.what());
    }

    if(isActivePrimary())
    {
        if(setActiveSecondary())
        {
            return Close();
        }
    }

    return false;
}

bool DbMirroredConnection::Execute(const FString& sql)
{
    AutoUnlockMutex guard(mMutex);

    try
    {
        return mDbConnection->Execute(sql);
    }
    catch(EDbConnectionIoError& e)
    {
        hlogstream(HLOG_WARN, e.what());
    }

    if(isActivePrimary())
    {
        if(setActiveSecondary())
        {
            return Execute(sql);
        }
    }

    return false;
}

DbResult DbMirroredConnection::Store(const FString& sql)
{
    AutoUnlockMutex guard(mMutex);

    try
    {
        return mDbConnection->Store(sql);
    }
    catch(EDbConnectionIoError& e)
    {
        hlogstream(HLOG_WARN, e.what());
    }

    if(isActivePrimary())
    {
        if(setActiveSecondary())
        {
            return Store(sql);
        }
    }

    return DbResult();
}

DbResult DbMirroredConnection::Use(const FString& sql)
{
    AutoUnlockMutex guard(mMutex);

    try
    {
        return mDbConnection->Use(sql);
    }
    catch(EDbConnectionIoError& e)
    {
        hlogstream(HLOG_WARN, e.what());
    }

    if(isActivePrimary())
    {
        if(setActiveSecondary())
        {
            return Use(sql);
        }
    }

    return DbResult();
}

bool DbMirroredConnection::Execute2(const FString& sql)
{
    AutoUnlockMutex guard(mMutex);

    try
    {
      return mDbConnection->Execute2(sql);
    }
    catch(EDbConnectionIoError& e)
    {
      hlogstream(HLOG_WARN, e.what());
    }

    if(isActivePrimary())
    {
        if(setActiveSecondary())
        {
          return Execute2(sql);
        }
    }

    return false;
}

DbResult DbMirroredConnection::Store2(const FString& sql)
{
    AutoUnlockMutex guard(mMutex);

    try
    {
        return mDbConnection->Store2(sql);
    }
    catch(EDbConnectionIoError& e)
    {
        hlogstream(HLOG_WARN, e.what());
    }

    if(isActivePrimary())
    {
        if(setActiveSecondary())
        {
            return Store2(sql);
        }
    }

    return DbResult();
}

DbResult DbMirroredConnection::Use2(const FString& sql)
{
    AutoUnlockMutex guard(mMutex);

    try
    {
        return mDbConnection->Use2(sql);
    }
    catch(EDbConnectionIoError& e)
    {
        hlogstream(HLOG_WARN, e.what());
    }

    if(isActivePrimary())
    {
        if(setActiveSecondary())
        {
            return Use2(sql);
        }
    }

    return DbResult();
}

void DbMirroredConnection::AutoCommit(bool enabled)
{
    AutoUnlockMutex guard(mMutex);

    try
    {
        mDbConnection->AutoCommit(enabled);
        return;
    }
    catch(EDbConnectionIoError& e)
    {
        hlogstream(HLOG_WARN, e.what());
    }

    if(isActivePrimary())
    {
        if(setActiveSecondary())
        {
            AutoCommit(enabled);
        }
    }
}

void DbMirroredConnection::Begin()
{
    AutoUnlockMutex guard(mMutex);

    try
    {
        mDbConnection->Begin();
        return;
    }
    catch(EDbConnectionIoError& e)
    {
        hlogstream(HLOG_WARN, e.what());
    }

    if(isActivePrimary())
    {
        if(setActiveSecondary())
        {
            Begin();
        }
    }
}

void DbMirroredConnection::Commit()
{
    AutoUnlockMutex guard(mMutex);

    if(isActiveSecondary())
    {
        // @TODO we don't always need to throw in this case, as it is
        // feasible the database user may have done several select
        // calls (reading only) in a transaction, and is calling
        // commit to end the transaction.  Let's think some more about
        // this before changing the behavior.
        throw EDbConnectionReadOnly();
    }
    else
    {
        try
        {
            mDbConnection->Commit();
            return;
        }
        catch(EDbConnectionIoError& e)
        {
            hlogstream(HLOG_WARN, e.what());
        }

        if(setActiveSecondary())
        {
            return Commit();
        }
    }
}

void DbMirroredConnection::Rollback()
{
    AutoUnlockMutex guard(mMutex);

    if(isActiveSecondary())
    {
        throw EDbConnectionReadOnly();
    }
    else
    {
        try
        {
            mDbConnection->Rollback();
            return;
        }
        catch(EDbConnectionIoError& e)
        {
            hlogstream(HLOG_WARN, e.what());
        }

        if(setActiveSecondary())
        {
            Rollback();
        }
    }
}

uint64_t DbMirroredConnection::InsertID()
{
    AutoUnlockMutex guard(mMutex);

    try
    {
        return mDbConnection->InsertID();
    }
    catch(EDbConnectionIoError& e)
    {
        hlogstream(HLOG_ERROR, e.what());

        if(! isActiveSecondary())
        {
            (void)setActiveSecondary();
        }

        throw;
    }
}

uint64_t DbMirroredConnection::AffectedRows()
{
    try
    {
        return mDbConnection->AffectedRows();
    }
    catch(EDbConnectionIoError& e)
    {
        hlogstream(HLOG_ERROR, e.what());

        if(! isActiveSecondary())
        {
            (void)setActiveSecondary();
        }

        throw;
    }
}

FString DbMirroredConnection::Escape(const char *str)
{
    AutoUnlockMutex guard(mMutex);
    return mDbConnection->Escape(str);
}

FString DbMirroredConnection::GetError() const
{
    return mDbConnection->GetError();
}
unsigned int DbMirroredConnection::GetErrno() const
{
    return mDbConnection->GetErrno();
}

unsigned int DbMirroredConnection::GetTries() const
{
    return mDbConnection->GetTries();
}

bool DbMirroredConnection::IsTemporaryError() const
{
    return mDbConnection->IsTemporaryError();
}

unsigned int DbMirroredConnection::GetRetries() const
{
    return mDbConnection->GetRetries();
}

unsigned int DbMirroredConnection::GetQueryRetryDelay() const
{
    return mDbConnection->GetQueryRetryDelay();
}

void DbMirroredConnection::BackupDatabase(const FString &targetPath)
{
    AutoUnlockMutex guard(mMutex);

    if(isActivePrimary())
    {
        mDbConnection->BackupDatabase(targetPath);
    }
}

void DbMirroredConnection::BackupDatabase(DbConnection &targetDatabase)
{
    AutoUnlockMutex guard(mMutex);

    if(isActivePrimary())
    {
        mDbConnection->BackupDatabase(targetDatabase);
    }
}
bool DbMirroredConnection::Execute(const DbSqlStatement& statement)
{
    AutoUnlockMutex guard(mMutex);

    if(isActiveSecondary())
    {
        if(statement.IsMutator())
        {
            throw EDbConnectionReadOnly();
        }
        else
        {
            return mDbConnection->Execute(statement);
        }
    }
    else
    {
        try
        {
            return (mDbConnection->Execute(statement));
        }
        catch(EDbConnectionIoError& e)
        {
            hlogstream(HLOG_WARN, e.what());
        }

        if(setActiveSecondary())
        {
            return Execute(statement);
        }
    }

    return false;
}

DbResult DbMirroredConnection::Use(const DbSqlStatement& statement)
{
    AutoUnlockMutex guard(mMutex);

    if(isActiveSecondary())
    {
        if(statement.IsMutator())
        {
            throw EDbConnectionReadOnly();
        }
        else
        {
            return mDbConnection->Use(statement);
        }
    }
    else
    {
        try
        {
            return (mDbConnection->Use(statement));
        }
        catch(EDbConnectionIoError& e)
        {
            hlogstream(HLOG_WARN, e.what());
        }

        if(setActiveSecondary())
        {
            return Use(statement);
        }
    }

    return DbResult();
}

DbResult DbMirroredConnection::Store(const DbSqlStatement& statement)
{
    AutoUnlockMutex guard(mMutex);

    if(isActiveSecondary())
    {
        if(statement.IsMutator())
        {
            throw EDbConnectionReadOnly();
        }
        else
        {
            return mDbConnection->Store(statement);
        }
    }
    else
    {
        try
        {
            return (mDbConnection->Store(statement));
        }
        catch(EDbConnectionIoError& e)
        {
            hlog(HLOG_WARN, "%s", e.what());
        }

        if(setActiveSecondary())
        {
            return Store(statement);
        }
    }

    return DbResult();
}


DbMirroredConnection::~DbMirroredConnection()
{
}

const std::string& DbMirroredConnection::GetDbName() const
{
    AutoUnlockMutex guard(mMutex);

    if(mDbConnection)
    {
        return mDbConnection->GetDbName();
    }

    return mDBName;
}

bool DbMirroredConnection::HasPendingQueries() const
{
    AutoUnlockMutex guard(mMutex);

    if(mDbConnection)
    {
        return mDbConnection->HasPendingQueries();
    }

    return false;
}

const FString& DbMirroredConnection::GetCurrentQuery() const
{
    AutoUnlockMutex guard(mMutex);

    if (mDbConnection)
    {
        return mDbConnection->GetCurrentQuery();
    }

    return mCurrentQuery;
}
