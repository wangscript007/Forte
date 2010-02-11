#ifndef __base64_h_
#define __base64_h_

#include "FString.h"
#include "Exception.h"

EXCEPTION_SUBCLASS(CForteException, CForteBase64Exception);

class CBase64 {
public:
    static int Encode(const char *data, int size, FString &out);
    static int Decode(const char *in, FString &out);
};


#endif
