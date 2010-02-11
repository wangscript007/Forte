#ifndef __FString_h
#define __FString_h

#include <string>
#include <vector>
#include <set>
#include <math.h>
#include <arpa/inet.h>

#define NOPOS	std::string::npos
namespace Forte
{
    class FStringFC { // type for the FString Format Constructor 'flag' 
    public:
        FStringFC() {};
    }; 

    class FString : public std::string
    {
    public:
        FString();
        void Format(const char *format, ...) __attribute__((format(printf,2,3)));
        void vFormat(const char *format, int size, va_list ap);
        FString(const FString& other) { static_cast<std::string&>(*this) = other; }
        FString(const std::string& other) { static_cast<std::string&>(*this) = other; }
        FString(const char *str) { if (str) static_cast<std::string&>(*this) = str; }
        FString(const char chr) { static_cast<std::string&>(*this) = chr; }
        FString(unsigned int i) { Format("%u", i); }
        FString(int i) { Format("%d", i); }
        FString(unsigned long i) { Format("%lu", i); }
        FString(long long i) { Format("%lld", i); }
        FString(unsigned long long i) { Format("%llu", i); }
        FString(const std::vector<unsigned int> &intvec);
        FString(const std::set<unsigned int> &intset);
        FString(const std::vector<int> &intvec);
        FString(const std::set<int> &intset);
        FString(const struct sockaddr *sa);
        FString(const FStringFC &f, const char *format, ...)  __attribute__((format(printf,3,4)));
        virtual ~FString();

    public:
        // operators
        inline operator const char*() const { return c_str(); }
        inline char& operator [](int p) { return static_cast<std::string*>(this)->operator[](p); }

        // string helpers
        FString& Replace(const char* find, const char* replace);
        inline FString& Trim(const char *strip_char = " \t\r\n") { return TrimLeft(strip_char).TrimRight(strip_char); }
        FString& TrimLeft(const char* strip_chars = " \t\r\n");
        FString& TrimRight(const char* strip_chars = " \t\r\n");
        FString& MakeUpper();
        FString& MakeLower();
        FString Left(int n) const { return substr(0, n); }
        FString Mid(int p, int n = (int)NOPOS) const { if (p >= (int)length()) return ""; else return substr(p, n); }
        FString Right(int n) const { if (n > (int)length()) return *this; else return substr(length() - n, n); }
        inline int Compare(const char *str) const { return strcmp(c_str(), str); }
        inline int CompareNoCase(const char *str) const {
            return strcasecmp(c_str(), str);
        }
        inline unsigned int asUnsignedInteger(void) const { return strtoul(c_str(), NULL, 0); }
        inline int asInteger(void) const { return strtol(c_str(), NULL, 0); }
        inline unsigned long long asUInt64(void) const { return strtoull(c_str(), NULL, 0); }
        inline unsigned long asUInt32(void) const { return strtoul(c_str(), NULL, 0); }
        inline long long asInt64(void) const { return strtoll(c_str(), NULL, 0); }
        inline long asInt32(void) const { return strtol(c_str(), NULL, 0); }

        inline bool isNumeric(void) const
            {
                if (empty()) return false;
                char *c; 
                strtod(c_str(), &c); 
                return (*c == '\0'); 
            }
        inline bool isUnsignedNumeric(void) const
            {
                if (empty()) return false;
                char *c; 
                return strtod(c_str(), &c) >= 0.0 && (*c == '\0');
            }

        /// Split a multi-line string into single line components.  Line endings are
        /// automatically detected.  If trim is true, external whitespace will be trimmed
        /// from each line.  Line endings may be CR, LF, or CRLF, and must all be the same.
        int lineSplit(std::vector<FString> &lines, bool trim = false) const;

        /// Split a delimited string into its components.  Delimiters will not appear in
        /// the output.  If trim is true, external whitespace will be trimmed 
        /// from each component.
        int explode(const char *delim, std::vector<FString> &components, bool trim=false, const char* strip_chars = " \t\r\n") const;
        int explode(const char *delim, std::vector<std::string> &components, bool trim=false, const char* strip_chars = " \t\r\n") const;

        /// Combine multiple components into a single string.  Glue will be used
        /// between each component.
        FString& implode(const char *glue, const std::vector<FString> &components);
        FString& implode(const char *glue, const std::vector<std::string> &components);
        FString& implode(const char *glue, const std::set<FString> &components);

        /// Scale Computing's split() and join(), similar to the above implode() and explode()
        std::vector<FString> split(const char *separator, size_t max_parts = 0) const;
        static FString join(const std::vector<FString>& array, const char *separator);
        static FString join(const std::vector<std::string>& array, const char *separator);
        static FString join(const std::vector<const char*>& array, const char *separator);

        // file I/O
        static FString & LoadFile(const char *filename, unsigned int max_len, FString &out);
        static void SaveFile(const char *filename, const FString &in);
    };

    inline FString operator +(const FString& str, const FString& add) { FString ret = str; ret += add; return ret; }
    inline FString operator +(const FString& str, const char* add) { FString ret; ret = str; ret += add; return ret; }
    inline FString operator +(const char* add, const FString& str) { FString ret; ret = add; ret += str; return ret; }
    inline FString operator +(const FString& str, const char add) { FString ret; ret = str; ret += add; return ret; }
    inline FString operator +(const char add, const FString& str) { FString ret; ret = add; ret += str; return ret; }
};
#endif
