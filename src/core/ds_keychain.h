// ds_keychain.h - the API key lives in the platform credential store, not in a
// plain text config file.
//
//   macOS   Security.framework, kSecClassGenericPassword
//   Windows DPAPI (CryptProtectData), the sealed blob kept in the config file
//   Linux   a 0600 file (there is no keychain without a secret service); the UI
//           says so instead of claiming more than the platform gives
#ifndef DS_KEYCHAIN_H
#define DS_KEYCHAIN_H

#include "ds_core.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { DS_KEYCHAIN_SERVICE = 0 };
enum { DS_KEYCHAIN_ACCOUNT = 0 };

bool ds_keychain_set(const char *service, const char *account, const char *secret,
                     char *err, size_t cap);
bool ds_keychain_get(const char *service, const char *account,
                     char *out, size_t cap, char *err, size_t errcap);
bool ds_keychain_delete(const char *service, const char *account, char *err, size_t cap);

/* "keychain" on macOS and Windows, "protected file" on Linux. */
const char *ds_keychain_backend_name(void);
bool ds_keychain_is_strong(void);

#ifdef __cplusplus
}
#endif

#endif
