/*
 *#SCQAD TAG: scaled.condition.capacity.low
 */

#include "FileSystemImpl.h"
#include "LogManager.h"
#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>
#include <cstring>
#include <glob.h>
#include <sys/time.h>
#include "SystemCallUtil.h"
#include "FTrace.h"

using namespace std;
using namespace Forte;
namespace boostfs = boost::filesystem;

// constants
const unsigned MAX_RESOLVE = 1000;

// ctor/dtor
FileSystemImpl::FileSystemImpl()
{
}


FileSystemImpl::~FileSystemImpl()
{
}

// helpers
FString FileSystemImpl::StrError(int err) const
{
    return SystemCallUtil::GetErrorDescription(err);
}


// interface
FString FileSystemImpl::Basename(const FString& filename,
                                 const FString& suffix)
{
    size_t pos = filename.find_last_of("/");
    FString result;
    if (pos == NOPOS)
        result = filename;
    else
        result = filename.Right(filename.length() - (pos + 1));

    if (!suffix.empty())
    {
        FString end = result.Right(suffix.length());
        if (end.Compare(suffix) == 0)
            result = result.Left(result.length() - suffix.length());
    }

    return result;
}

FString FileSystemImpl::Dirname(const FString& filename)
{
    size_t pos = filename.find_last_of("/");
    FString result;
    if (pos == NOPOS)
        result = ".";
    else
        result = filename.Left(pos);

    return result;
}

FString FileSystemImpl::GetCWD()
{
    FString ret;
    char buf[1024];  // MAXPATHLEN + 1

    ::getcwd(buf, sizeof(buf));
    buf[sizeof(buf) - 1] = 0;
    ret = buf;
    return ret;
}

FString FileSystemImpl::GetPathToCurrentProcess()
{
    FString procPath = ResolveSymLink("/proc/self/exe");
    if (procPath.ComparePrefix("/", 1))
    {
        procPath = GetCWD() + "/" + procPath;
    }
    return procPath;
}


unsigned int FileSystemImpl::Glob(std::vector<FString> &resultVec,
                                  const FString &pattern,
                                  const int globFlags) const
{
    int defaultFlags = GLOB_MARK | GLOB_BRACE | GLOB_TILDE |GLOB_TILDE_CHECK;
    int flags = globFlags;
    glob_t globbuf;
    char errbuf[1024];

    resultVec.clear();

    if (!flags)
        flags = defaultFlags;

    int res = glob(pattern.c_str(), flags, NULL, &globbuf);
    switch (res)
    {
    case 0:
    case GLOB_NOMATCH:
        for (size_t i=0; i < globbuf.gl_pathc; i++)
        {
            resultVec.push_back(FString(globbuf.gl_pathv[i]));
        }
        break;
    default:
        memset(errbuf, 0, 1024);
        strerror_r(errno, errbuf, 1024);
        hlog(HLOG_DEBUG, "glob(%s) failed: %s\n",
             pattern.c_str(), errbuf);
        break;
    }
    globfree(&globbuf);
    return resultVec.size();
}

void FileSystemImpl::Touch(const FString& file)
{
    AutoFD fd;
    struct timeval tv[2];
    struct stat st;

    /*
     * If the file doesn't exist, then create it using open  which
     * should automatically set ctime, mtime & atime to be the same.
     * Otherwise, just update the mtime & atime via utimes()
     */
    if (stat(file, &st) < 0) {
        if (errno != ENOENT) {
            SystemCallUtil::ThrowErrNoException(errno);
        }
        if ((fd = ::open(file, O_WRONLY | O_CREAT, 0666)) == AutoFD::NONE) {
            SystemCallUtil::ThrowErrNoException(errno);
        }
    } else {
        time_t timeInSecondsSinceEpoch = time(NULL);

        tv[0].tv_sec = timeInSecondsSinceEpoch;
        tv[0].tv_usec = 0;

        tv[1].tv_sec = timeInSecondsSinceEpoch;
        tv[1].tv_usec = 0;

        if (::utimes(file, tv) == -1) {
            SystemCallUtil::ThrowErrNoException(errno);
        }
    }
}

void FileSystemImpl::ChangeOwner(const FString& path, uid_t uid, gid_t gid)
{
    if (::chown(path.c_str(), uid, gid))
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }
}


bool FileSystemImpl::FileExists(const FString& filename) const
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s)", __FUNCTION__, filename.c_str());
    struct stat st;
    return (stat(filename, &st) == 0);
}

bool FileSystemImpl::IsDir(const FString& path) const
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s)", __FUNCTION__, path.c_str());
    struct stat st;

    if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode))
    {
        return true;
    }
    else
    {
        return false;
    }
}

void FileSystemImpl::StatFS(const FString& path, struct statfs *st)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s)", __FUNCTION__, path.c_str());
    //TODO: check return codes, return appropriate values

    if (::statfs(path.c_str(), st) == 0)
    {
        return;
    }

    SystemCallUtil::ThrowErrNoException(errno);
}

int FileSystemImpl::Stat(const FString& path, struct stat *st)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s)", __FUNCTION__, path.c_str());
    return ::stat(path.c_str(), st);
}


int FileSystemImpl::LStat(const FString& path, struct stat *st)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s)", __FUNCTION__, path.c_str());
    return ::lstat(path.c_str(), st);
}


int FileSystemImpl::StatAt(int dir_fd, const FString& path, struct stat *st)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%d, %s)", __FUNCTION__, dir_fd, path.c_str());
    return ::fstatat(dir_fd, path.c_str(), st, 0);
}


int FileSystemImpl::LStatAt(int dir_fd, const FString& path, struct stat *st)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%d, %s)", __FUNCTION__, dir_fd, path.c_str());
    return ::fstatat(dir_fd, path.c_str(), st, AT_SYMLINK_NOFOLLOW);
}


int FileSystemImpl::FStatAt(int dir_fd, const FString& path, struct stat *st, int flags)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%d, %s)", __FUNCTION__, dir_fd, path.c_str());
    return ::fstatat(dir_fd, path.c_str(), st, flags);
}

void FileSystemImpl::GetChildren(const FString& path,
                                 std::vector<Forte::FString> &children,
                                 bool recurse,
                                 bool includePathInChildNames,
                                 bool includeDirNames) const
{
    FTRACE2("%s, %s, %s", path.c_str(), (recurse ? "true" : "false"),
            (includePathInChildNames ? "true" : "false"));

    DIR *d = ::opendir(path);
    if (!d)
        SystemCallUtil::ThrowErrNoException(errno);
    AutoFD dir(d);

    FString stmp;
    struct dirent *result;
    struct dirent entry;

    while (readdir_r(dir, &entry, &result) == 0 && result != NULL)
    {
        stmp = entry.d_name;

        if (stmp != "." && stmp != "..")
        {
            if (IsDir(path + "/" + stmp))
            {
                if (recurse)
                {
                    GetChildren(path + "/" + stmp, children, recurse, includePathInChildNames, includeDirNames);
                }
                else
                {
                    hlog(HLOG_DEBUG2, "Skipping %s (not recursing)",
                         stmp.c_str());
                }

                if(includeDirNames)
                {
                    if(includePathInChildNames)
                    {
                        children.push_back(path + "/" + stmp);
                    }
                    else
                    {
                        children.push_back(stmp);
                    }
                }
            }
            else
            {
                if (includePathInChildNames)
                {
                    children.push_back(path + "/" + stmp);
                }
                else
                {
                    children.push_back(stmp);
                }
            }
        }
    }
}

uint64_t FileSystemImpl::CountChildren(const FString& path,
                                       bool recurse) const
{
    FTRACE2("%s, %s", path.c_str(), (recurse ? "true" : "false"));

    DIR *d = ::opendir(path);
    if (!d)
        SystemCallUtil::ThrowErrNoException(errno);
    AutoFD dir(d);

    FString stmp;
    struct dirent *result;
    struct dirent entry;

    uint64_t count = 0;
    while (readdir_r(dir, &entry, &result) == 0 && result != NULL)
    {
        stmp = entry.d_name;

        if (stmp != "." && stmp != "..")
        {
            if (IsDir(path + "/" + stmp))
            {
                if (recurse)
                {
                    count += CountChildren(path + "/" + stmp, recurse);
                }
                else
                {
                    hlog(HLOG_DEBUG, "Skipping %s (not recursing)",
                         stmp.c_str());
                }
            }
            else
            {
                count++;
            }
        }
    }
    return count;
}

void FileSystemImpl::Unlink(const FString& path, bool unlink_children,
                            const ProgressCallback &progressCallback)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s, %s)", __FUNCTION__,
         path.c_str(), (unlink_children ? "true" : "false"));
    FString stmp;

    // recursive delete?
    if (unlink_children)
    {
        int err_number;
        struct dirent entry;
        struct dirent *result;

        struct stat st;
        AutoFD dir;

        // inefficient but effective recursive delete algorithm
        // NOTE: use lstat() so we don't follow symlinks
        if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode))
        {
            errno = 0;
            dir = opendir(path);

            if (dir.dir() == NULL || errno != 0)
            {
                if (errno == ENOENT)
                {
                    // the thing we're trying to unlink is gone, great!
                    return;
                }

                //else, we could not work with this dir to delete it
                SystemCallUtil::ThrowErrNoException(errno);
            }

            err_number = 0;
            while ((err_number = readdir_r(dir, &entry, &result)) == 0
                   && result != NULL)
            {
                stmp = entry.d_name;

                if (stmp != "." && stmp != "..")
                {
                    Unlink(path + "/" + entry.d_name,
                           true, progressCallback);
                }
            }

            // when finished we should get NULL for readdir_r and no error.
            if (err_number != 0)
            {
                if (errno == ENOENT)
                {
                    // the thing we're trying to unlink is gone, great!
                    return;
                }

                SystemCallUtil::ThrowErrNoException(errno);
            }
        }
    }

    // delete self
    unlinkHelper(path);

    // callback?
    if (progressCallback) progressCallback(0);
}


void FileSystemImpl::UnlinkAt(int dir_fd, const FString& path)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%d, %s)", __FUNCTION__, dir_fd, path.c_str());
    FString stmp;
    int err;

    // delete self
    if ((err = ::unlinkat(dir_fd, path, 0)) != 0)
    {
        if (errno == ENOENT) return;
        if (errno == EISDIR) err = ::unlinkat(dir_fd, path, AT_REMOVEDIR);
    }

    if (err != 0)
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }
}


void FileSystemImpl::unlinkHelper(const FString& path)
{
    // hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s)", __FUNCTION__, path.c_str());
    int err;

    if ((err = ::unlink(path)) != 0)
    {
        if (errno == ENOENT) return;
        if (errno == EISDIR) err = ::rmdir(path);
    }

    if (err != 0)
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }
}


void FileSystemImpl::Rename(const FString& from, const FString& to)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s, %s)", __FUNCTION__, from.c_str(), to.c_str());

    if (::rename(from.c_str(), to.c_str()) != 0)
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }
}


void FileSystemImpl::RenameAt(int dir_from_fd, const FString& from,
                              int dir_to_fd, const FString& to)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%d, %s, %d, %s)", __FUNCTION__,
         dir_from_fd, from.c_str(), dir_to_fd, to.c_str());

    if (::renameat(dir_from_fd, from.c_str(), dir_to_fd, to.c_str()) != 0)
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }
}


void FileSystemImpl::MakeDir(const FString& path, mode_t mode, bool make_parents)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s, %04o, %s)", __FUNCTION__,
         path.c_str(), mode, (make_parents ? "true" : "false"));
    FString stmp;
    struct stat st;
    int err;

    // check for /
    if (path.empty() || path == "/") return;

    // path exists?
    if (stat(path, &st) == 0)
    {
        // early exit?
        if (S_ISDIR(st.st_mode))
        {
            return;
        }
        else
        {
            stmp.Format("FORTE_MAKEDIR_FAIL_IN_THE_WAY|||%s",
                        path.c_str());
            throw EFileSystemMakeDir(stmp);
        }
    }
    else
    {
        err = errno;
        if (err != ENOENT && err != ENOTDIR)
        {
            hlog(HLOG_WARN, "error: %s", SystemCallUtil::GetErrorDescription(err).c_str());
        }
        // create parent?
        if (make_parents)
        {
            size_t pos = path.find_last_of('/');
            if (pos != std::string::npos)
            {
                MakeDir(path.Left(pos), mode, true);
            }
        }

        // make path
        ::mkdir(path, mode);
        err = errno;

        // validate path
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
        {
            if (err == 0)
            {
                throw EFileSystemMakeDir(FStringFC(),
                                         "FORTE_MAKEDIR_SUBSEQUENTLY_DELETED|||%s",
                                         path.c_str());
            }

            SystemCallUtil::ThrowErrNoException(err);
        }
    }
}

int FileSystemImpl::ScanDir(const FString& path, std::vector<FString> &namelist)
{
    if (boostfs::exists(path.c_str()))
    {
        namelist.clear();
        boostfs::directory_iterator end_itr; // default construction yields past-the-end
        for (boostfs::directory_iterator itr(path.c_str());
             itr != end_itr; ++itr )
        {
            namelist.push_back(itr->path().filename().string());
        }

    }
    return static_cast<int>(namelist.size());
}

void FileSystemImpl::MakeDirAt(int dir_fd, const FString& path, mode_t mode)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%d, %s, %04o)", __FUNCTION__, dir_fd, path.c_str(), mode);
    struct stat st;
    int err;

    // make path
    ::mkdirat(dir_fd, path, mode);
    err = errno;

    // validate path
    if (StatAt(dir_fd, path, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        if (err == 0)
        {
            throw EFileSystemMakeDir(FStringFC(),
                                     "FORTE_MAKEDIR_SUBSEQUENTLY_DELETED|||%s",
                                     path.c_str());
        }

        SystemCallUtil::ThrowErrNoException(err);
    }
}


void FileSystemImpl::Link(const FString& from, const FString& to)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s, %s)", __FUNCTION__, from.c_str(), to.c_str());

    if (::link(from.c_str(), to.c_str()) != 0)
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }
}


void FileSystemImpl::LinkAt(int dir_from_fd, const FString& from, int dir_to_fd, const FString& to)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%d, %s, %d, %s)", __FUNCTION__,
         dir_from_fd, from.c_str(), dir_to_fd, to.c_str());

    if (::linkat(dir_from_fd, from.c_str(), dir_to_fd, to.c_str(), 0) != 0)
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }
}


void FileSystemImpl::SymLink(const FString& from, const FString& to)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s, %s)", __FUNCTION__, from.c_str(), to.c_str());

    if (::symlink(from.c_str(), to.c_str()) != 0)
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }
}


void FileSystemImpl::SymLinkAt(const FString& from, int dir_to_fd, const FString& to)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s, %d, %s)", __FUNCTION__,
         from.c_str(), dir_to_fd, to.c_str());

    if (::symlinkat(from.c_str(), dir_to_fd, to.c_str()) != 0)
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }
}


FString FileSystemImpl::ReadLink(const FString& path)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s)", __FUNCTION__, path.c_str());
    FString ret;
    char buf[1024];  // MAXPATHLEN + 1
    int rc;

    if ((rc = ::readlink(path, buf, sizeof(buf))) == -1)
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }

    buf[std::min<size_t>(rc, sizeof(buf) - 1)] = 0;
    ret = buf;
    return ret;
}


FString FileSystemImpl::ResolveSymLink(const FString& path)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s)", __FUNCTION__, path.c_str());
    struct stat st;
    char buf[1024];  // MAXPATHLEN + 1
    FString stmp, base(GetCWD()), ret(path);
    std::map<FString, bool> visited;
    unsigned n = 0;
    int rc;

    // resolve loop
    while (n++ < MAX_RESOLVE)
    {
        // check visited map
        if (visited.find(ret) != visited.end())
        {
            stmp.Format("FORTE_RESOLVE_SYMLINK_LOOP|||%s|||%s",
                        path.c_str(), ret.c_str());
            throw EFileSystemResolveSymLink(stmp);
        }

        visited[ret] = true;

        // stat path
        if (LStat(ret, &st) != 0)
        {
            rc = errno;
            hlog(HLOG_DEBUG4, "FileSystemImpl::%s(): unable to stat %s", __FUNCTION__, ret.c_str());
            stmp.Format("FORTE_RESOLVE_SYMLINK_BROKEN|||%s|||%s", path.c_str(), ret.c_str());
            throw EFileSystemResolveSymLink(stmp);
        }

        if (!S_ISLNK(st.st_mode)) return ret;

        // get base dir
        base = ResolveRelativePath(base, ret);
        base = base.Left(base.find_last_of('/'));

        // read link
        if ((rc = ::readlink(ret, buf, sizeof(buf))) == -1)
        {
            SystemCallUtil::ThrowErrNoException(errno);
        }

        buf[std::min<size_t>(rc, sizeof(buf) - 1)] = 0;
        ret = buf;

        // resolve link relative to dir containing the symlink
        ret = ResolveRelativePath(base, ret);
    }

    // too many iterations
    stmp.Format("FORTE_RESOLVE_SYMLINK_TOO_MANY|||%s|||%u", path.c_str(),
                MAX_RESOLVE);
    throw EFileSystemResolveSymLink(stmp);
}


FString FileSystemImpl::FullyResolveSymLink(const FString& path)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::%s(%s)", __FUNCTION__, path.c_str());
    std::vector<FString>::iterator pi;
    std::vector<FString> parts;
    std::map<FString, bool> visited, good;
    FString stmp, base, ret, partial, result;
    unsigned i = 0;
    bool done = false;
    struct stat st;
    int rc;

    // start with a full path
    ret = ResolveRelativePath(GetCWD(), path);

    // loop until path resolution results in no changes
    while (!done)
    {
        // check visited map
        if (visited.find(ret) != visited.end())
        {
            stmp.Format("FORTE_RESOLVE_SYMLINK_LOOP|||%s|||%s",
                        path.c_str(), ret.c_str());
            throw EFileSystemResolveSymLink(stmp);
        }

        visited[ret] = true;

        // check for too many recursions
        if (i++ > MAX_RESOLVE)
        {
            stmp.Format("FORTE_RESOLVE_SYMLINK_TOO_MANY|||%s|||%u",
                        path.c_str(), MAX_RESOLVE);
            throw EFileSystemResolveSymLink(stmp);
        }

        // init
        done = true;
        partial.clear();
        parts = ret.split("/");

        // loop over parts
        for (pi = parts.begin(); pi != parts.end(); ++pi)
        {
            // get partial path and base
            if (pi->empty()) continue;
            base = partial;
            if (base.empty()) base = "/";
            partial += "/" + (*pi);
            if (good.find(partial) != good.end()) continue;

            // hlog(HLOG_DEBUG4, "%s(%s) [step %u]: resolve %s",
            //__FUNCTION__, path.c_str(), i, partial.c_str());

            // stat partial path
            if (lstat(partial, &st) != 0)
            {
                rc = errno;
                stmp.Format("FORTE_RESOLVE_SYMLINK_BROKEN|||%s|||%s",
                            path.c_str(), partial.c_str());
                throw EFileSystemResolveSymLink(stmp);
            }

            // is partial path a link?
            if (S_ISLNK(st.st_mode))
            {
                result = ResolveRelativePath(base, ReadLink(partial));
            }
            else
            {
                result = partial;
                good[result] = true;
            }

            // did it change?
            if (partial != result)
            {
                done = false;
                ret = result;

                while ((++pi) != parts.end())
                {
                    ret += "/" + (*pi);
                }

                break;
            }
        }
    }

    // done
    hlog(HLOG_DEBUG4, "%s(%s) = %s", __FUNCTION__, path.c_str(), ret.c_str());
    return ret;
}


FString FileSystemImpl::MakeRelativePath(const FString& base, const FString& path)
{
    std::vector<FString>::iterator bi, pi;
    std::vector<FString> b, p;
    FString ret;

    // get paths
    b = base.split("/");
    p = path.split("/");
    bi = b.begin();
    pi = p.begin();

    // find common paths
    while (bi != b.end() && pi != p.end() && (*bi == *pi))
    {
        ++bi;
        ++pi;
    }

    // generate path
    while (bi != b.end())
    {
        if (!ret.empty()) ret += "/..";
        else ret = "..";
        ++bi;
    }

    while (pi != p.end())
    {
        if (!ret.empty()) ret += "/";
        ret += *pi;
        ++pi;
    }

    // done
    hlog(HLOG_DEBUG4, "make_rel_path(%s, %s) = %s", base.c_str(), path.c_str(), ret.c_str());
    return ret;
}


FString FileSystemImpl::ResolveRelativePath(const FString& base,
                                            const FString& path)
{
    std::vector<FString>::reverse_iterator br;
    std::vector<FString>::iterator bi, pi;
    std::vector<FString> b, p;
    FString ret;

    // early exit?
    if (path.empty()) return base;
    if (path[0] == '/') return path;

    // get paths
    b = base.split("/");
    p = path.split("/");
    br = b.rbegin();
    pi = p.begin();

    // resolve path
    while (pi != p.end() && *pi == "..")
    {
        if (br != b.rend()) br++;  // backwards iteration
        ++pi;
    }

    // build path
    for (bi = b.begin(); br != b.rend(); ++bi, ++br)
    {
        if (bi->empty()) continue;
        ret += "/" + *bi;
    }

    while (pi != p.end())
    {
        ret += "/" + *pi;
        ++pi;
    }

    // done
    hlog(HLOG_DEBUG4, "ResolveRelativePath(%s, %s) = %s", base.c_str(), path.c_str(), ret.c_str());
    return ret;
}


void FileSystemImpl::FileCopy(const FString& from, const FString& to, mode_t mode)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::file_copy(%s, %s, %4o)",
         from.c_str(), to.c_str(), mode);
    FString command, to_dir, stmp;

    // make directory
    to_dir = to.Left(to.rfind('/'));
    MakeDir(to_dir, mode, true);

    try
    {
        boostfs::copy_file(static_cast<const string&>(from),
                           static_cast<const string&>(to),
                           boostfs::copy_option::overwrite_if_exists);
    }
    catch (boostfs::filesystem_error &e)
    {
        stmp.Format("FORTE_COPY_FAIL|||%s|||%s|||%s", from.c_str(), to.c_str(),
                    e.what());
        throw EFileSystemCopy(stmp);
    }
}


FString FileSystemImpl::FileGetContents(const FString& filename) const
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::file_get_contents(%s)", filename.c_str());
    ifstream in(filename, ios::in | ios::binary);
    FString ret, stmp;
    char buf[16384];

    while (in.good())
    {
        in.read(buf, sizeof(buf));
        stmp.assign(buf, in.gcount());
        ret += stmp;
    }

    return ret;
}

void FileSystemImpl::FileOpen(AutoFD &autoFd, const FString& path, int flags, int mode)
{
    FTRACE2("%s, %i, %i", path.c_str(), flags, mode);

    autoFd = ::open(path, flags, mode);
    if (autoFd == AutoFD::NONE)
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }
}

void FileSystemImpl::FilePutContentsWithPerms(
    const FString& path, const FString& data, int flags, int mode)
{
    FTRACE2("%s, %i, %i", path.c_str(), flags, mode);

    int fd = ::open(path, flags, mode);
    if (fd < 0)
        SystemCallUtil::ThrowErrNoException(errno);

    FILE *file = fdopen(fd, "w");
    if (file == NULL)
    {
        close(fd);
        SystemCallUtil::ThrowErrNoException(errno);
    }

    fprintf(file, "%s", data.c_str());

    fclose(file); // we must use fclose if open and fdopen succceeded
}

void FileSystemImpl::FilePutContents(
    const FString& filename, const FString& data, bool append, bool throwOnError)
{
    FTRACE2("%s, [data], %s, %s", filename.c_str(),
            (append ? "APPEND" : "DON'T APPEND"),
            (throwOnError ? "THROW ON ERROR" : "DON'T THROW ON ERROR"));

    ios_base::openmode mode=ios::out | ios::binary;

    if (append)
    {
        mode=mode | ios::app;
    }

    ofstream out(filename, mode);
    if (out.good())
    {
        hlog(HLOG_DEBUG3, "Writing data to %s", filename.c_str());
        if (throwOnError)
        {
            out.exceptions( ofstream::failbit | ofstream::badbit );
        }
        out.write(data.c_str(), data.size());
    }
    else if (throwOnError)
    {
        hlog_and_throw(HLOG_ERR,
                       EFileSystem(FStringFC(), "%s not available for writing",
                                   filename.c_str()));
    }
    else
    {
        hlog(HLOG_WARN, "%s: NOT available for writing", filename.c_str());
    }
}

void FileSystemImpl::FileAppend(const FString& from, const FString& to)
{
    hlog(HLOG_DEBUG4, "FileSystemImpl::file_append(%s, %s)",
         from.c_str(), to.c_str());
    FString to_dir, stmp;

    ifstream in(from, ios::in | ios::binary);
    ofstream out(to,  ios::out | ios::binary | ios::app);
    if (out.good() && in.good())
        out << in.rdbuf();
    else
    {
        stmp.Format("FORTE_FILE_APPEND_FAIL|||%s|||%s", from.c_str(), to.c_str());
        throw EFileSystemAppend(stmp);
    }
}

void FileSystemImpl::DeepCopy(const FString& source, const FString& dest,
                              const ProgressCallback &progressCallback)
{
    InodeMap inode_map;
    uint64_t size_copied = 0;
    deepCopyHelper(source, dest, source, inode_map, size_copied,
                   progressCallback);
}


void FileSystemImpl::deepCopyHelper(const FString& base_from,
                                    const FString& base_to,
                                    const FString& dir,
                                    InodeMap &inode_map,
                                    uint64_t &size_copied,
                                    const ProgressCallback &progressCallback)
{
    hlog(HLOG_DEBUG4, "Filesystem::%s(%s, %s, %s)", __FUNCTION__,
         base_from.c_str(), base_to.c_str(), dir.c_str());
    struct dirent **entries;
    struct stat st;
    FString name, path, to_path, rel;
    int i, n;

    // first call?
    if (dir == base_from)
    {
        inode_map.clear();
        size_copied = 0;
    }

    // scan dir
    if ((n = scandir(dir, &entries, NULL, alphasort)) == -1) return;

    // copy this directory
    rel = dir.Mid(base_from.length());
    to_path = base_to + rel;

    if (stat(dir, &st) != 0)
    {
        hlog(HLOG_ERR, "Unable to perform deep copy on %s: directory is gone",
             path.c_str());
        throw EFileSystemCopy(FStringFC(), "FORTE_DEEP_COPY_FAIL|||%s", rel.c_str());
    }

    // create this directory
    try
    {
        MakeDir(path.Left(path.find_last_of('/')), 0777, true);  // make parent dirs first
        copyHelper(dir, to_path, st, inode_map, size_copied);
    }
    catch (Exception &e)
    {
        hlog(HLOG_ERR, "%s", e.GetDescription().c_str());
        throw EFileSystemCopy(FStringFC(), "FORTE_DEEP_COPY_FAIL|||%s", rel.c_str());
    }

    // copy the directory's entries
    for (i=0; i<n; free(entries[i++]))
    {
        name = entries[i]->d_name;
        if (name == "." || name == "..") continue;
        path = dir + "/" + name;
        rel = path.Mid(base_from.length());
        to_path = base_to + rel;

        if (lstat(path, &st) != 0)
        {
            hlog(HLOG_ERR, "Unable to perform deep copy on %s: directory is gone",
                 path.c_str());
            throw EFileSystemCopy(FStringFC(), "FORTE_DEEP_COPY_FAIL|||%s", rel.c_str());
        }
        else if (S_ISDIR(st.st_mode))
        {
            deepCopyHelper(base_from, base_to, path, inode_map, size_copied,
                           progressCallback);
        }
        else
        {
            try
            {
                copyHelper(path, to_path, st, inode_map, size_copied,
                           progressCallback);
            }
            catch (Exception &e)
            {
                hlog(HLOG_ERR, "%s", e.GetDescription().c_str());
                throw EFileSystemCopy(FStringFC(), "FORTE_DEEP_COPY_FAIL|||%s", rel.c_str());
            }
        }
    }

    // done
    free(entries);
}


void FileSystemImpl::Copy(const FString &from_path,
                          const FString &to_path,
                          const ProgressCallback &progressCallback)
{
    InodeMap inode_map;
    uint64_t size_copied = 0;
    struct stat st;

    if (stat(from_path, &st) != 0)
    {
        throw EFileSystemCopy(FStringFC(), "FORTE_COPY_FAIL|||%s|||%s",
                              from_path.c_str(), to_path.c_str());
    }

    copyHelper(from_path, to_path, st, inode_map, size_copied, progressCallback);
}


void FileSystemImpl::copyHelper(const FString& from_path,
                                const FString& to_path,
                                const struct stat& st,
                                InodeMap &inode_map,
                                uint64_t &size_copied,
                                const ProgressCallback &progressCallback)
{
    struct timeval times[2];

    // what are we dealing with?
    if (S_ISDIR(st.st_mode))
    {
        // create directory
        MakeDir(to_path, st.st_mode & 0777);
        chown(to_path, st.st_uid, st.st_gid);
        times[0].tv_sec = st.st_atim.tv_sec;
        times[0].tv_usec = st.st_atim.tv_nsec / 1000;
        times[1].tv_sec = st.st_mtim.tv_sec;
        times[1].tv_usec = st.st_mtim.tv_nsec / 1000;
        utimes(to_path, times);
    }
    else if (S_ISLNK(st.st_mode))
    {
        // create symlink
        symlink(ReadLink(from_path), to_path);
    }
    else if (S_ISREG(st.st_mode))
    {
        bool copy = true;

        // has hard links?
        if (st.st_nlink > 1)
        {
            // check map
            InodeMap::iterator mi;

            if ((mi = inode_map.find(st.st_ino)) != inode_map.end())
            {
                // make hard link
                Link(mi->second, to_path);
                copy = false;
            }
            else
            {
                // save path so we can link it later
                inode_map[st.st_ino] = to_path;
            }
        }

        // do copy?
        if (copy)
        {
            const size_t buf_size = 65536; // 64 KB
            size_t i = 0, x;
            char buf[buf_size];

            ifstream in(from_path, ios::in | ios::binary);
            ofstream out(to_path, ios::out | ios::trunc | ios::binary);

            if (!in.good() || !out.good())
            {
                throw EFileSystemCopy("FORTE_COPY_FAIL|||" + from_path + "|||" + to_path);
            }

            while (in.good())
            {
                in.read(buf, buf_size);
                const size_t r = in.gcount();
                for (x=0; x<r && buf[x] == 0; x++);
                if (x == r) out.seekp(in.tellg(), ios::beg);            // leave a hole
                else out.write(buf, r);

                if ((progressCallback) && ((++i % 160) == 0))  // every 10 MB
                {
                    progressCallback(size_copied + in.tellg());
                }

                // TODO: replace this as g_shutdown has been removed!
                // if (g_shutdown)
                // {
                //     throw EFileSystemCopy("FORTE_COPY_FAIL_SHUTDOWN|||" +
                //                           from_path + "|||" + to_path);
                // }
            }

            in.close();
            out.close();

            // set attributes
            truncate(to_path, st.st_size);
            chown(to_path, st.st_uid, st.st_gid);
            chmod(to_path, st.st_mode & 07777);
            times[0].tv_sec = st.st_atim.tv_sec;
            times[0].tv_usec = st.st_atim.tv_nsec / 1000;
            times[1].tv_sec = st.st_mtim.tv_sec;
            times[1].tv_usec = st.st_mtim.tv_nsec / 1000;
            utimes(to_path, times);
        }

        // count size copied
        size_copied += st.st_size;
    }
    else if (S_ISCHR(st.st_mode) ||
             S_ISBLK(st.st_mode) ||
             S_ISFIFO(st.st_mode) ||
             S_ISSOCK(st.st_mode))
    {
        // create special file
        if (mknod(to_path, st.st_mode, st.st_rdev) != 0)
        {
            hlog(HLOG_WARN, "copy: could not create special file: %s", to_path.c_str());
            // not worthy of an exception
        }
    }
    else
    {
        // skip unknown types
        hlog(HLOG_WARN, "copy: skipping file of unknown type %#x: %s",
             st.st_mode, to_path.c_str());
        // not worthy of an exception
    }

    // progress
    if (progressCallback) progressCallback(size_copied);
}

FString FileSystemImpl::MakeTemporaryFile(const FString& nameTemplate) const
{
    FTRACE;

    FString fileName(nameTemplate);
    if (fileName.Right(6) != "XXXXXX")
    {
        fileName.append("XXXXXX");
    }

    char cName[fileName.size() + 1];

    cName[fileName.size()] = 0;
    strncpy(cName, fileName.c_str(), fileName.size());

    int file;
    if ((file = mkstemp(cName)) == -1)
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }

    while(::close(file) == -1 && errno == EINTR);

    return FString(cName, sizeof(cName)-1);
}

void FileSystemImpl::Truncate(const FString& path, off_t size) const
{
    FTRACE;

    if (truncate(path.c_str(), size) == -1)
    {
        SystemCallUtil::ThrowErrNoException(errno);
    }
}
