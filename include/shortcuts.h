#ifndef SCHISM_SHORTCUTS_H_
#define SCHISM_SHORTCUTS_H_

#include "headers.h"

struct key_event;
typedef struct cfg_file cfg_file_t;

void shortcuts_reset_defaults(void);
void shortcuts_load(cfg_file_t *cfg);
void shortcuts_save(cfg_file_t *cfg);
void shortcuts_remap_key_event(struct key_event *k);

int shortcuts_count(void);
const char *shortcuts_action_name(int idx);
int shortcuts_get_binding(int idx, int *sym, int *mod);
int shortcuts_set_binding(int idx, int sym, int mod);
int shortcuts_find_conflict(int idx, int sym, int mod);
int shortcuts_is_capture_mode(void);

const char *shortcuts_format_binding(int sym, int mod, char *buf, size_t buflen);

#endif

