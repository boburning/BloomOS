#ifndef BLOOM_SHELL_RA_FORM_H
#define BLOOM_SHELL_RA_FORM_H

#include "../bloomUi/bloom_ui_core.h"

#include <stddef.h>

typedef enum {
    BLOOM_SHELL_RA_FIELD_USERNAME = 0,
    BLOOM_SHELL_RA_FIELD_TOKEN,
} BloomShellRaField;

typedef struct {
    BloomUiKeyboardFocus keyboard;
    BloomShellRaField field;
    char username[64];
    char token[128];
} BloomShellRaForm;

void bloom_shell_ra_form_init(BloomShellRaForm *form);
int bloom_shell_ra_form_move(BloomShellRaForm *form, int horizontal, int vertical);
void bloom_shell_ra_form_cycle_mode(BloomShellRaForm *form);
void bloom_shell_ra_form_toggle_field(BloomShellRaForm *form);
int bloom_shell_ra_form_append(BloomShellRaForm *form);
int bloom_shell_ra_form_backspace(BloomShellRaForm *form);
int bloom_shell_ra_form_label(const BloomShellRaForm *form, char *label, size_t label_size);
int bloom_shell_ra_form_submit(const char *bloom_ra_path, BloomShellRaForm *form);
int bloom_shell_ra_sign_out(const char *bloom_ra_path);
void bloom_shell_ra_form_clear(BloomShellRaForm *form);

#endif
