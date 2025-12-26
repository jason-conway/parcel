#include "wire-raw.h"
#include "wire.h"

wire_t *init_wire_from_session_key(session_key_t *session_key)
{
    size_t len = sizeof(session_key_t);
    return init_wire(TYPE_SESSION_KEY, session_key, &len);
}
