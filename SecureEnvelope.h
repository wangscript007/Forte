#ifndef __secure_envelope_h
#define __secure_envelope_h

#include "Forte.h"
#ifndef FORTE_NO_OPENSSL
#include "SecureString.h"

// Secure Envelope is for the storage of sensitive account information (credit
// card numbers, etc).

// Example storage format:
// A credit card number 1234 5678 9012 3456 would be stored as:
// XXXXXXXXXXXX3456$nR9gHZAA/3v2PMqJ4cCpMGDdL-Q1XEiWsfe
// The characters prior to the '$' are the unencrypted 'obscured' version of the credit
// card number.  This is the only version of the number that may be displayed to anyone.
// The data after the '$' character is the RSA encrypted portion of the number.
// The encrypted portion excludes the last 4 numbers, as they already appear in plaintext
// and would facilitate a brute force attack on the encrypted version if they were also contained in
// the ciphertext.

EXCEPTION_SUBCLASS(CForteException, CForteSecureEnvelopeEncoderException);

class CSecureEnvelopeEncoder {
public:
    CSecureEnvelopeEncoder(const CKeyBuffer &publicKey) : mPublicKey(publicKey, NULL) {};
    virtual ~CSecureEnvelopeEncoder() {};

    // Encode an account number as described above.  numClear is the number of characters
    // to provide in cleartext in the obscured version of the account number.
    FString & encode(const FString &accountNumber, unsigned int numClear, FString &out);
protected:
    CPublicKey mPublicKey;
};

class CSecureEnvelopeDecoder {
public:
    CSecureEnvelopeDecoder(const CKeyBuffer &privateKey, const char *passphrase) :
        mPrivateKey(privateKey, passphrase) {};
    virtual ~CSecureEnvelopeDecoder() {};

    // Decode an account number.
    FString & decode(const FString &encoded, FString &out);

    // Get the obscured version of the account number (for display)
    static FString & obscured(const FString &encoded, FString &out);
    static bool isEncoded(const std::string &data);

protected:
    CPrivateKey mPrivateKey;
};

#endif
#endif
