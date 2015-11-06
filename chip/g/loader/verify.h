#ifndef _F_VERIFY_H__
#define _F_VERIFY_H__

// Verify a RSA PKCS1.5 signature against an expected sha256.
// Unlocks for execution upon success.
void LOADERKEY_verify(uint32_t keyid, const uint32_t* signature, const uint32_t* sha256);

#endif  // _F_VERIFY_H__
