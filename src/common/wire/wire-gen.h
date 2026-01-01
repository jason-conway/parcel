#pragma once

#ifndef TYPE_BEGIN_DEF
    #define TYPE_BEGIN_DEF(name, T) \
        typedef struct name##_t { \
            uint8_t type; \
            uint8_t len[8];
#endif

#ifndef TYPE_END_DEF
    #define TYPE_END_DEF(name, T) \
            uint8_t data[]; \
        } __attribute__((packed)) name##_t;
#endif

#ifndef GEN_STD_FUNCS
    #define GEN_STD_FUNCS(name, T) \
        T name##_get_type(const name##_t *name) \
        { \
            return name->type; \
        } \
        void name##_set_type(name##_t *name, T type) \
        { \
            name->type = type; \
        } \
        size_t name##_get_wire_length(const name##_t *name) \
        { \
            return wire_pack64(name->len); \
        } \
        size_t name##_get_payload_length(const name##_t *name) \
        { \
            return wire_pack64(name->len) - sizeof(name##_t); \
        } \
        void *name##_get_data(name##_t *name) \
        { \
            return name->data; \
        } \
        void name##_set_data(name##_t *name, const void *data, size_t len) \
        { \
            memcpy(name->data, data, len); \
        } \
        void name##_set_len(name##_t *name, size_t len) \
        { \
            wire_unpack64(name->len, len); \
        } \
        name##_t *init_##name(T type, size_t len) \
        { \
            const size_t msg_len = sizeof(name##_t) + len; \
            name##_t *name = xcalloc(msg_len); \
            name##_set_type(name, type); \
            name##_set_len(name, msg_len); \
            return name; \
        }
#endif

#ifndef GEN_FIELD_FUNCS
    #define GEN_FIELD_FUNCS(name, T1, field, T2, RT) \
        RT name##_get_##field(const name##_t *name) \
        { \
            return (RT)name->field; \
        } \
        void name##_set_##field(name##_t *name, RT field) \
        { \
            name->field = (T2)field; \
        }
#endif

#ifndef GEN_4BYTE_GETTER_SETTER_FUNCS
    #define GEN_4BYTE_GETTER_SETTER_FUNCS(name, field, RT) \
        RT name##_get_##field(const name##_t *name) \
        { \
            return wire_pack32(name->field); \
        } \
        void name##_set_##field(name##_t *name, RT field) \
        { \
            wire_unpack32(name->field, (uint32_t)field); \
        }
#endif

#ifndef GEN_2BYTE_GETTER_SETTER_FUNCS
    #define GEN_2BYTE_GETTER_SETTER_FUNCS(name, field, RT) \
        RT name##_get_##field(const name##_t *name) \
        { \
            return wire_pack16(name->field); \
        } \
        void name##_set_##field(name##_t *name, RT field) \
        { \
            wire_unpack16(name->field, (uint16_t)field); \
        }
#endif

#ifndef GEN_GETTER_SETTER_HEADERS
    #define GEN_GETTER_SETTER_HEADERS(name, field, RT) \
        RT name##_get_##field(const name##_t *name); \
        void name##_set_##field(name##_t *name, RT field);
#endif



#ifndef GEN_STD_HEADERS
    #define GEN_STD_HEADERS(name, T) \
        typedef struct name##_t name##_t; \
        T name##_get_type(const name##_t *name); \
        void name##_set_type(name##_t *name, T type); \
        size_t name##_get_wire_length(const name##_t *name); \
        size_t name##_get_payload_length(const name##_t *name); \
        void *name##_get_data(name##_t *name); \
        void name##_set_data(name##_t *name, const void *data, size_t len); \
        void name##_set_len(name##_t *name, size_t len); \
        name##_t *init_##name(T type, size_t len);
#endif

#ifndef GEN_FIELD_HEADERS
    #define GEN_FIELD_HEADERS(name, T1, field, T2, RT) \
        RT name##_get_##field(const name##_t *name); \
        void name##_set_##field(name##_t *name, RT field);
#endif
