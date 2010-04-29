#ifndef DbUtil_h
#define DbUtil_h

#ifndef FORTE_NO_DB

#include "DbRow.h"
#include "DbResult.h"
#include "DbConnection.h"
#include "DbException.h"
#include "LogManager.h"
#include <stdarg.h>

namespace Forte
{
    class DbUtil {
    public:
        static inline DbResult DbStore(const char *func, DbConnection &db, const char *sql)
            {
#ifdef DB_DEBUG
                hlog(HLOG_DEBUG, "Executing sql [%s]", sql);
#else
                if (db.mLogQueries) hlog(HLOG_DEBUG, "Executing sql [%s]", sql);
#endif
                DbResult result;
                if (!(result = db.store(sql)))
                {
                    // database error
                    FString err;
                    err.Format("%s: query failed; errno=%u tries=%u retries=%u error=%s full_query=%s",
                               func, db.mErrno, db.mTries, db.mRetries, db.mError.c_str(), sql);
                    if (db.isTemporaryError())
                        throw DbTempErrorException(err, db.mErrno);
                    else
                        throw DbException(err, db.mErrno);
                }
                if (db.mTries > 1)
                    hlog(HLOG_WARN, "%s: query executed after %u tries; errno was %u",
                         func, db.mTries, db.mErrno);
                return result;
            }
        static inline DbResult DbUse(const char *func, DbConnection &db, const char *sql)
            {
                if (db.mLogQueries) hlog(HLOG_DEBUG, "Executing sql [%s]", sql);
                DbResult result;
                if (!(result = db.use(sql)))
                {
                    // database error
                    FString err;
                    err.Format("%s: query failed; errno=%u tries=%u retries=%u error=%s full_query=%s",
                               func, db.mErrno, db.mTries, db.mRetries, db.mError.c_str(), sql);
                    if (db.isTemporaryError())
                        throw DbTempErrorException(err, db.mErrno); // indicate temporary error
                    else
                        throw DbException(err, db.mErrno);
                }
                if (db.mTries > 1)
                    hlog(HLOG_WARN, "%s: query executed after %u tries; errno was %u",
                         func, db.mTries, db.mErrno);
                return result;
            }
        static inline void DbExecute(const char *func, DbConnection &db, const char *sql)
            {
#ifdef DB_DEBUG
                hlog(HLOG_DEBUG, "Executing sql [%s]", sql);
#else
                if (db.mLogQueries) hlog(HLOG_DEBUG, "Executing sql [%s]", sql);
#endif
                if (!db.execute(sql))
                {
                    // database error
                    FString err;
                    err.Format("%s: query failed; errno=%u tries=%u retries=%u error=%s full_query=%s",
                               func, db.mErrno, db.mTries, db.mRetries, db.mError.c_str(), sql);
                    if (db.isTemporaryError())
                        throw DbTempErrorException(err, db.mErrno);
                    else
                        throw DbException(err, db.mErrno);
                }
                if (db.mTries > 1)
                    hlog(HLOG_WARN, "%s: query executed after %u tries; errno was %u",
                         func, db.mTries, db.mErrno);
            }
        static inline FString DbEscape(DbConnection &db, const char *sql_in)
            {
                return db.escape(sql_in);
            }
    };
};
#define DbStore(db, sql) DbUtil::DbStore(__FUNCTION__, db, sql)
#define DbUse(db, sql) DbUtil::DbUse(__FUNCTION__, db, sql)
#define DbExecute(db, sql) DbUtil::DbExecute(__FUNCTION__, db, sql)
#define DbEscape(db, sql) DbUtil::DbEscape(db, sql)
#endif
#endif
