
#include "wire-util.h"
#include "wire-gen.h"
#include "wire-file.h"
#include "wire.h"
#include "xplatform.h"
#include "log.h"
#include "xutils.h"

TYPE_BEGIN_DEF(file_msg, file_msg_type_t)
    uint8_t user[FILE_USERNAME_LENGTH];
    uint8_t filename[PATH_MAX];
    uint8_t gid[4];
    uint8_t uid[4];
    uint8_t mode[2];
TYPE_END_DEF(file_msg, file_msg_type_t)

GEN_STD_FUNCS(file_msg, file_msg_type_t)

GEN_4BYTE_GETTER_SETTER_FUNCS(file_msg, gid, gid_t)
GEN_4BYTE_GETTER_SETTER_FUNCS(file_msg, uid, uid_t)
GEN_2BYTE_GETTER_SETTER_FUNCS(file_msg, mode, mode_t)


void file_msg_set_user(file_msg_t *fm, const char *user)
{
    memset(fm->user, 0, sizeof(fm->user));
    const size_t len = strnlen(user, sizeof(fm->user) - 1);
    memcpy(fm->user, user, len);
}
bool file_msg_get_user(file_msg_t *fm, char *user)
{
    const size_t len = strnlen((const char *)fm->user, sizeof(fm->user));
    memcpy(user, fm->user, len);
    return len > 0;
}

void file_msg_set_filename(file_msg_t *f, const char *name)
{
    memset(f->filename, 0, sizeof(f->filename));
    size_t len = strnlen(name, sizeof(f->filename) - 1);
    memcpy(f->filename, name, len);
}

bool file_msg_get_filename(file_msg_t *f, char *out)
{
    size_t len = strnlen((const char *)f->filename, sizeof(f->filename) - 1);
    memcpy(out, f->filename, len);
    out[len] = '\0';
    return len > 0;
}

file_msg_t *file_msg_from_file(const char *user, const char *path)
{
    if (!path || !user) {
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    size_t size = xfilesize(path);
    if (!size || size > FILE_DATA_MAX_SIZE) {
        fclose(f);
        return NULL;
    }

    file_msg_type_t type = xfiletype(path) ? FILE_TYPE_BINARY : FILE_TYPE_TEXT;
    file_msg_t *fm = init_file_msg(type, size);

    file_msg_set_filename(fm, xconstbasename(path));
    file_msg_set_gid(fm, xgetgid(path));
    file_msg_set_uid(fm, xgetuid(path));
    file_msg_set_mode(fm, xgetmode(path));

    if (fread(fm->data, 1, size, f) != size) {
        fm = xfree(fm);
    }
    fclose(f);
    return fm;
}

bool file_msg_to_file(file_msg_t *fm, const char *directory)
{
    if (!fm) {
        return false;
    }

    char filename[PATH_MAX] = { 0 };
    if (!file_msg_get_filename(fm, filename)) {
        log_error("empty filename field");
        return false;
    }

    char *path = NULL;
    if (directory) {
        path = xstrcat(directory, "/", filename);
    }
    else {
        path = xstrdup(filename);
    }

    bool ok = false;
    FILE *f = fopen(path, "wb");
    if (f) {
        size_t size = file_msg_get_payload_length(fm);
        ok = fwrite(fm->data, 1, size, f) == size;
        ok &= !(fflush(f) || fclose(f));
    }
    mode_t mode = file_msg_get_mode(fm);
    ok &= xchmod(path, mode);

    xfree(path);
    return ok;
}

wire_t *init_wire_from_file_msg(file_msg_t *file_msg)
{
    size_t len = file_msg_get_wire_length(file_msg);
    return init_wire(TYPE_FILE, file_msg, &len);
}

