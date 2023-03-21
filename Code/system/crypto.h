#ifndef CRYPTO_H
#define CRYPTO_H

//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wunknown-pragmas"
//#pragma GCC diagnostic ignored "-Wunused-variable"
//#pragma GCC diagnostic ignored "-Wunused-function"
//#pragma GCC diagnostic ignored "-Wunused-parameter"
//#pragma GCC diagnostic ignored "-Wpragmas"
//#pragma GCC diagnostic ignored "-Wextra"
//#pragma GCC diagnostic pop


/*                                    *
 *	      CMAC-AES algorithm          *
 *                                    */

#include <crypto++/osrng.h>
using CryptoPP::AutoSeededRandomPool;

#include <crypto++/cryptlib.h>
using CryptoPP::Exception;

#include <crypto++/cmac.h>
using CryptoPP::CMAC;

#include <crypto++/aes.h>
using CryptoPP::AES;

#include <crypto++/hex.h>
using CryptoPP::HexDecoder;
using CryptoPP::HexEncoder;

#include <crypto++/filters.h>
using CryptoPP::HashFilter;
using CryptoPP::HashVerificationFilter;
using CryptoPP::SignatureVerificationFilter;
using CryptoPP::SignerFilter;
using CryptoPP::StringSink;
using CryptoPP::StringSource;

#include <crypto++/secblock.h>
using CryptoPP::SecByteBlock;

#include <crypto++/pssr.h>
using CryptoPP::PSS;

#include <crypto++/sha.h>
using CryptoPP::SHA1;

#include <crypto++/files.h>
using CryptoPP::FileSink;
using CryptoPP::FileSource;

#include <crypto++/whrlpool.h>
#include <crypto++/modes.h>


#include <iostream>
#include <iomanip>
#include <fstream>
//#include <string>
#include <math.h>
#include <cstdlib>
#include <vector>
#include <unistd.h>
#include <sys/time.h>


//CMAC-AES key generator
//Return a keyPairHex where just the priv key is set
inline std::string CmacGenerateHexKey()
{
    std::string privKey;
    AutoSeededRandomPool prng;
    SecByteBlock key(AES::DEFAULT_KEYLENGTH);
    prng.GenerateBlock(key, key.size()); // Key is of type SecByteBlock.

    privKey.clear();
    StringSource ss(key, key.size(), true, new HexEncoder(new StringSink(privKey)));              
    std::cout << "Key: " << privKey;
    return privKey;
}

// CMAC-AES Signature generator
// Return a token, mac, to verify the a message.
inline std::string CmacSignString(const std::string &aPrivateKeyStrHex, 
				  const std::string &aMessage)
{
    std::string mac = "";

    //KEY TRANSFORMATION. https://stackoverflow.com/questions/26145776/string-to-secbyteblock-conversion

    SecByteBlock privKey((const unsigned char *)(aPrivateKeyStrHex.data()), aPrivateKeyStrHex.size());

    CMAC<AES> cmac(privKey.data(), privKey.size());

    StringSource ss1(aMessage, true, new HashFilter(cmac, new StringSink(mac)));

    return mac;
}

// CMAC-AES Verification function
inline bool CmacVerifyString(const std::string &aPublicKeyStrHex,
                             const std::string &aMessage,
                             const std::string &mac)
{
    bool res = false;

    // KEY TRANSFORMATION
    //https://stackoverflow.com/questions/26145776/string-to-secbyteblock-conversion
    try
    {
    	SecByteBlock privKey((const unsigned char *)(aPublicKeyStrHex.data()), aPublicKeyStrHex.size());

    	CMAC<AES> cmac(privKey.data(), privKey.size());
    	const int flags = HashVerificationFilter::THROW_EXCEPTION | HashVerificationFilter::HASH_AT_END;

   	// MESSAGE VERIFICATION
    	StringSource ss3(aMessage + mac, true, new HashVerificationFilter(cmac, NULL, flags));
	res = true;
	std::cout << "Verified message" << std::endl;
    }
    catch(const CryptoPP::Exception& e)
    {
	std::cerr << e.what() << std::endl;
    }
    return res;
}



#endif // CRYPTO_H
