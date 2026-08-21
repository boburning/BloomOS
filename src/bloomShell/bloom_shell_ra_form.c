#include "bloom_shell_ra_form.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static char *active_buffer(BloomShellRaForm *form, size_t *capacity)
{
    if (form->field == BLOOM_SHELL_RA_FIELD_USERNAME) {
        *capacity = sizeof(form->username);
        return form->username;
    }
    *capacity = sizeof(form->token);
    return form->token;
}

void bloom_shell_ra_form_init(BloomShellRaForm *form)
{
    if (form == NULL)
        return;
    memset(form, 0, sizeof(*form));
    bloom_ui_keyboard_init(&form->keyboard);
}

int bloom_shell_ra_form_move(BloomShellRaForm *form, int horizontal, int vertical)
{
    return form == NULL ? 0 : bloom_ui_keyboard_move(&form->keyboard, horizontal, vertical);
}

void bloom_shell_ra_form_cycle_mode(BloomShellRaForm *form)
{
    if (form != NULL)
        bloom_ui_keyboard_cycle_mode(&form->keyboard);
}

void bloom_shell_ra_form_toggle_field(BloomShellRaForm *form)
{
    if (form != NULL)
        form->field = form->field == BLOOM_SHELL_RA_FIELD_USERNAME
                          ? BLOOM_SHELL_RA_FIELD_TOKEN
                          : BLOOM_SHELL_RA_FIELD_USERNAME;
}

int bloom_shell_ra_form_append(BloomShellRaForm *form)
{
    if (form == NULL)
        return -1;
    size_t capacity = 0;
    char *buffer = active_buffer(form, &capacity);
    return bloom_ui_text_append(buffer, capacity, bloom_ui_keyboard_character(&form->keyboard));
}

int bloom_shell_ra_form_backspace(BloomShellRaForm *form)
{
    if (form == NULL)
        return -1;
    size_t capacity = 0;
    char *buffer = active_buffer(form, &capacity);
    return bloom_ui_text_backspace(buffer, capacity);
}

int bloom_shell_ra_form_label(const BloomShellRaForm *form, char *label, size_t label_size)
{
    if (form == NULL || label == NULL || label_size == 0)
        return -1;
    int length;
    if (form->field == BLOOM_SHELL_RA_FIELD_USERNAME)
        length = snprintf(label, label_size, "Username: %s", form->username);
    else {
        size_t token_length = strnlen(form->token, sizeof(form->token));
        if (token_length >= label_size || token_length + strlen("Token: ") >= label_size)
            return -1;
        length = snprintf(label, label_size, "Token: ");
        for (size_t index = 0; index < token_length; ++index)
            label[length++] = '*';
        label[length] = '\0';
    }
    return length >= 0 && (size_t)length < label_size ? 0 : -1;
}

static int write_all(int descriptor, const char *value, size_t length)
{
    while (length > 0) {
        ssize_t written = write(descriptor, value, length);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return -1;
        value += written;
        length -= (size_t)written;
    }
    return 0;
}

int bloom_shell_ra_form_submit(const char *bloom_ra_path, BloomShellRaForm *form)
{
    if (form == NULL)
        return -1;
    if (bloom_ra_path == NULL || bloom_ra_path[0] != '/' || form->username[0] == '\0' ||
        form->token[0] == '\0') {
        memset(form->token, 0, sizeof(form->token));
        return -1;
    }
    int input[2];
    if (pipe(input) != 0) {
        memset(form->token, 0, sizeof(form->token));
        return -1;
    }
    pid_t child = fork();
    if (child < 0) {
        close(input[0]);
        close(input[1]);
        memset(form->token, 0, sizeof(form->token));
        return -1;
    }
    if (child == 0) {
        close(input[1]);
        if (dup2(input[0], STDIN_FILENO) < 0)
            _exit(127);
        close(input[0]);
        execl(bloom_ra_path, bloom_ra_path, "account", "configure", form->username, "softcore",
              "automatic", (char *)NULL);
        _exit(127);
    }
    close(input[0]);
    struct sigaction ignored = {.sa_handler = SIG_IGN};
    struct sigaction previous;
    sigemptyset(&ignored.sa_mask);
    int signal_changed = sigaction(SIGPIPE, &ignored, &previous) == 0;
    int failed = write_all(input[1], form->token, strlen(form->token)) != 0 ||
                 write_all(input[1], "\n", 1) != 0;
    close(input[1]);
    if (signal_changed)
        sigaction(SIGPIPE, &previous, NULL);
    memset(form->token, 0, sizeof(form->token));
    int status = 0;
    for (int attempt = 0; attempt < 100; ++attempt) {
        pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child)
            return !failed && WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
        if (result < 0 && errno != EINTR)
            return -1;
        usleep(100000);
    }
    kill(child, SIGKILL);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
    return -1;
}

void bloom_shell_ra_form_clear(BloomShellRaForm *form)
{
    if (form != NULL)
        memset(form, 0, sizeof(*form));
}

int bloom_shell_ra_sign_out(const char *bloom_ra_path)
{
    if (bloom_ra_path == NULL || bloom_ra_path[0] != '/')
        return -1;
    pid_t child = fork();
    if (child < 0)
        return -1;
    if (child == 0) {
        execl(bloom_ra_path, bloom_ra_path, "account", "sign-out", (char *)NULL);
        _exit(127);
    }
    int status = 0;
    for (int attempt = 0; attempt < 100; ++attempt) {
        pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child)
            return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
        if (result < 0 && errno != EINTR)
            return -1;
        usleep(100000);
    }
    kill(child, SIGKILL);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
    return -1;
}
