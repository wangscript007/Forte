#ifndef __DbAutoConnection_h
#define __DbAutoConnection_h

#ifndef FORTE_NO_DB

#include "DbConnection.h"
#include "DbConnectionPool.h"
#include "LogManager.h"

namespace Forte
{
// Auto helper to get and release a database connection.
    class CDbAutoConnection {
    public:
        CDbAutoConnection(bool autocommit = true)
            : mPool(CDbConnectionPool::GetInstance()), mDbConnection(mPool.GetDbConnection())
            {
                // set autocommit appropriately
                autoCommit(autocommit);
            };

        CDbAutoConnection(CDbConnectionPool& pool, bool autocommit = true)
            : mPool(pool), mDbConnection(pool.GetDbConnection())
            {
                // set autocommit appropriately
                autoCommit(autocommit);
            };

    private:
        // disallow the copy constructor, we don't refcount so the
        // destructor would release the DB connection from a copy before
        // the original may be finished.  Plus, this would potentially
        // allow sharing of a db connection.
        CDbAutoConnection(const CDbAutoConnection &other) : 
            mPool(other.mPool), mDbConnection(other.mDbConnection) { 
            // should never execute
            throw CException();
        };

    public:
        virtual ~CDbAutoConnection() {
            // turn off temporary logging
            logQueries(false);
            try {
                if (mDbConnection.hasPendingQueries())
                {
                    hlog(HLOG_WARN, "rolling back pending queries on connection");
                    rollback();
                }
                mPool.ReleaseDbConnection(mDbConnection);
            } catch (...) {
            }
        };

        inline void autoCommit(bool autoCommit) {
            mDbConnection.autoCommit(autoCommit);
        };
        inline void begin(void) {
            mDbConnection.begin();
        };
        inline void commit(void) {
            mDbConnection.commit();
        };
        inline void rollback(void) {
            mDbConnection.rollback();
        };
        inline void logQueries(bool log) {
            mDbConnection.logQueries(log);
        }
    
        // Cast operator
        inline operator CDbConnection* () const {
            return &mDbConnection;
        };
        inline operator CDbConnection& () const {
            return mDbConnection;
        };
    
        // Dereference operator
        inline CDbConnection& operator*() const {
            return mDbConnection;
        };

        // Dereference operator
        inline CDbConnection* operator->() const {
            return &mDbConnection;
        };
    
    private:
        CDbConnectionPool& mPool;
        CDbConnection & mDbConnection;
    };
};
#endif
#endif
