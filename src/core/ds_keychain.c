#include "ds_keychain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#else
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

const char *ds_keychain_backend_name(void) {
#if defined(_WIN32) || defined(__APPLE__)
    return "keychain";
#else
    return "protected file";
#endif
}

bool ds_keychain_is_strong(void) {
#if defined(_WIN32) || defined(__APPLE__)
    return true;
#else
    return false;
#endif
}

/* --- base64, for the sealed blob on Windows ----------------------------- */

static const char kB64e[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *b64_encode(const unsigned char *in, size_t n) {
    size_t out_n = ((n + 2) / 3) * 4;
    char *out = (char *)malloc(out_n + 1);
    if (!out) return NULL;
    size_t i, j = 0;
    for (i = 0; i + 2 < n; i += 3) {
        unsigned v = ((unsigned)in[i] << 16) | ((unsigned)in[i + 1] << 8) | in[i + 2];
        out[j++] = kB64e[(v >> 18) & 63];
        out[j++] = kB64e[(v >> 12) & 63];
        out[j++] = kB64e[(v >> 6) & 63];
        out[j++] = kB64e[v & 63];
    }
    if (i < n) {
        unsigned v = (unsigned)in[i] << 16;
        bool two = (i + 1 < n);
        if (two) v |= (unsigned)in[i + 1] << 8;
        out[j++] = kB64e[(v >> 18) & 63];
        out[j++] = kB64e[(v >> 12) & 63];
        out[j++] = two ? kB64e[(v >> 6) & 63] : '=';
        out[j++] = '=';
    }
    out[j] = '\0';
    return out;
}

static int b64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static unsigned char *b64_decode(const char *in, size_t *out_n) {
    size_t n = strlen(in);
    unsigned char *out = (unsigned char *)malloc(n / 4 * 3 + 3);
    if (!out) return NULL;
    size_t j = 0;
    unsigned acc = 0;
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
        if (in[i] == '=') break;
        int v = b64_value(in[i]);
        if (v < 0) continue;
        acc = (acc << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[j++] = (unsigned char)((acc >> bits) & 0xFF);
        }
    }
    *out_n = j;
    return out;
}

/* --- macOS --------------------------------------------------------------- */

#if defined(__APPLE__)

bool ds_keychain_set(const char *service, const char *account, const char *secret,
                     char *err, size_t cap) {
    CFStringRef svc = CFStringCreateWithCString(NULL, service, kCFStringEncodingUTF8);
    CFStringRef acct = CFStringCreateWithCString(NULL, account, kCFStringEncodingUTF8);
    CFDataRef data = CFDataCreate(NULL, (const UInt8 *)secret, (CFIndex)strlen(secret));
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrService, svc);
    CFDictionaryAddValue(query, kSecAttrAccount, acct);
    CFDictionaryAddValue(query, kSecAttrAccessible, kSecAttrAccessibleWhenUnlockedThisDeviceOnly);
    CFDictionaryAddValue(query, kSecValueData, data);

    OSStatus st = SecItemAdd(query, NULL);
    if (st == errSecDuplicateItem) {
        CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(NULL, 0,
                                                                &kCFTypeDictionaryKeyCallBacks,
                                                                &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(attrs, kSecValueData, data);
        st = SecItemUpdate(query, attrs);
        CFRelease(attrs);
    }
    CFRelease(query);
    CFRelease(data);
    CFRelease(acct);
    CFRelease(svc);

    if (st != errSecSuccess) {
        if (err) snprintf(err, cap, "keychain store failed (OSStatus %d)", (int)st);
        return false;
    }
    return true;
}

bool ds_keychain_get(const char *service, const char *account, char *out, size_t cap,
                     char *err, size_t errcap) {
    out[0] = '\0';
    CFStringRef svc = CFStringCreateWithCString(NULL, service, kCFStringEncodingUTF8);
    CFStringRef acct = CFStringCreateWithCString(NULL, account, kCFStringEncodingUTF8);
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrService, svc);
    CFDictionaryAddValue(query, kSecAttrAccount, acct);
    CFDictionaryAddValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionaryAddValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFDataRef data = NULL;
    OSStatus st = SecItemCopyMatching(query, (CFTypeRef *)&data);
    CFRelease(query);
    CFRelease(acct);
    CFRelease(svc);

    if (st == errSecItemNotFound) return false;
    if (st != errSecSuccess || !data) {
        if (err) snprintf(err, errcap, "keychain read failed (OSStatus %d)", (int)st);
        return false;
    }
    CFIndex n = CFDataGetLength(data);
    const UInt8 *p = CFDataGetBytePtr(data);
    size_t copy = (size_t)n < cap - 1 ? (size_t)n : cap - 1;
    memcpy(out, p, copy);
    out[copy] = '\0';
    CFRelease(data);
    return true;
}

bool ds_keychain_delete(const char *service, const char *account, char *err, size_t cap) {
    CFStringRef svc = CFStringCreateWithCString(NULL, service, kCFStringEncodingUTF8);
    CFStringRef acct = CFStringCreateWithCString(NULL, account, kCFStringEncodingUTF8);
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrService, svc);
    CFDictionaryAddValue(query, kSecAttrAccount, acct);
    OSStatus st = SecItemDelete(query);
    CFRelease(query);
    CFRelease(acct);
    CFRelease(svc);
    if (st != errSecSuccess && st != errSecItemNotFound) {
        if (err) snprintf(err, cap, "keychain delete failed (OSStatus %d)", (int)st);
        return false;
    }
    return true;
}

/* --- Windows ------------------------------------------------------------- */

#elif defined(_WIN32)

bool ds_keychain_set(const char *service, const char *account, const char *secret,
                     char *err, size_t cap) {
    (void)service;
    (void)account;
    DATA_BLOB in, out;
    in.pbData = (BYTE *)secret;
    in.cbData = (DWORD)strlen(secret);
    if (!CryptProtectData(&in, L"DeepSeek API key", NULL, NULL, NULL, 0, &out)) {
        if (err) snprintf(err, cap, "DPAPI protect failed (err %lu)", GetLastError());
        return false;
    }
    char *b64 = b64_encode(out.pbData, out.cbData);
    LocalFree(out.pbData);
    if (!b64) {
        if (err) snprintf(err, cap, "out of memory");
        return false;
    }
    /* the sealed blob is written by ds_config beside this file */
    char path[600];
    const char *local = getenv("LOCALAPPDATA");
    snprintf(path, sizeof path, "%s\\dsaudio\\sealed", local ? local : ".");
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(b64);
        if (err) snprintf(err, cap, "cannot write %s", path);
        return false;
    }
    fwrite(b64, 1, strlen(b64), f);
    fclose(f);
    free(b64);
    return true;
}

bool ds_keychain_get(const char *service, const char *account, char *out, size_t cap,
                     char *err, size_t errcap) {
    (void)service;
    (void)account;
    out[0] = '\0';
    char path[600];
    const char *local = getenv("LOCALAPPDATA");
    snprintf(path, sizeof path, "%s\\dsaudio\\sealed", local ? local : ".");
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char b64[8192];
    size_t n = fread(b64, 1, sizeof b64 - 1, f);
    fclose(f);
    b64[n] = '\0';
    size_t raw_n = 0;
    unsigned char *raw = b64_decode(b64, &raw_n);
    if (!raw) return false;
    DATA_BLOB in, plain;          /* not "out": that is the char * parameter */
    in.pbData = raw;
    in.cbData = (DWORD)raw_n;
    if (!CryptUnprotectData(&in, NULL, NULL, NULL, NULL, 0, &plain)) {
        free(raw);
        if (err) snprintf(err, errcap, "DPAPI unprotect failed (err %lu)", GetLastError());
        return false;
    }
    size_t copy = plain.cbData < cap - 1 ? plain.cbData : cap - 1;
    memcpy(out, plain.pbData, copy);
    out[copy] = '\0';
    LocalFree(plain.pbData);
    free(raw);
    return true;
}

bool ds_keychain_delete(const char *service, const char *account, char *err, size_t cap) {
    (void)service;
    (void)account;
    char path[600];
    const char *local = getenv("LOCALAPPDATA");
    snprintf(path, sizeof path, "%s\\dsaudio\\sealed", local ? local : ".");
    if (DeleteFileA(path)) return true;
    if (GetLastError() == ERROR_FILE_NOT_FOUND) return true;
    if (err) snprintf(err, cap, "cannot delete %s", path);
    return false;
}

/* --- Linux --------------------------------------------------------------- */

#else

static void key_path(char *out, size_t cap) {
    const char *home = getenv("HOME");
    snprintf(out, cap, "%s/.config/dsaudio/key", home ? home : ".");
}

bool ds_keychain_set(const char *service, const char *account, const char *secret,
                     char *err, size_t cap) {
    (void)service;
    (void)account;
    char path[600], dir[600];
    key_path(path, sizeof path);
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        /* recursive mkdir; no shell, so a odd $HOME cannot inject */
        for (char *q = dir + 1; *q; q++) {
            if (*q == '/') { *q = '\0'; mkdir(dir, 0700); *q = '/'; }
        }
        mkdir(dir, 0700);
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        if (err) snprintf(err, cap, "cannot write %s", path);
        return false;
    }
    size_t n = fwrite(secret, 1, strlen(secret), f);
    fclose(f);
    chmod(path, 0600);
    if (n != strlen(secret)) {
        if (err) snprintf(err, cap, "write failed: %s", path);
        return false;
    }
    return true;
}

bool ds_keychain_get(const char *service, const char *account, char *out, size_t cap,
                     char *err, size_t errcap) {
    (void)service;
    (void)account;
    out[0] = '\0';
    char path[600];
    key_path(path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    if (n == 0) return false;
    return true;
}

bool ds_keychain_delete(const char *service, const char *account, char *err, size_t cap) {
    (void)service;
    (void)account;
    char path[600];
    key_path(path, sizeof path);
    if (unlink(path) == 0 || errno == ENOENT) return true;
    if (err) snprintf(err, cap, "cannot delete %s", path);
    return false;
}

#endif
