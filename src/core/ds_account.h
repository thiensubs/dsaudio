// ds_account.h - the cheap key check: DeepSeek's user balance endpoint.
#ifndef DS_ACCOUNT_H
#define DS_ACCOUNT_H

#include "ds_config.h"
#include "ds_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_balance {
    bool is_available;
    char currency[16];
    char total[64];
    char granted[64];
    char topped_up[64];
    char err[320];
    bool ok;
} ds_balance;

bool ds_balance_query(const ds_config *cfg, long timeout_s, ds_balance *out);

#ifdef __cplusplus
}
#endif

#endif
