#include "cable.h"
#include "wire.h"
#include "wire-ctrl.h"
#include "wire-util.h"

TYPE_BEGIN_DEF(ctrl_msg, ctrl_msg_type_t)
    uint8_t cnt[2];
TYPE_END_DEF(ctrl_msg, ctrl_msg_type_t)

GEN_STD_FUNCS(ctrl_msg, ctrl_msg_type_t)

GEN_2BYTE_GETTER_SETTER_FUNCS(ctrl_msg, cnt, size_t)

ctrl_msg_t *init_ctrl(size_t count, const uint8_t *renewed_key)
{
    const size_t key_sz = 32;
    ctrl_msg_t *ctrl = init_ctrl_msg(CTRL_DHKE, key_sz);
    ctrl_msg_set_cnt(ctrl, count);
    ctrl_msg_set_data(ctrl, renewed_key, key_sz);
    return ctrl;
}

wire_t *init_wire_from_ctrl_msg(ctrl_msg_t *ctrl_msg)
{
    size_t len = ctrl_msg_get_wire_length(ctrl_msg);
    return init_wire(TYPE_CTRL, ctrl_msg, &len);
}

cable_t *init_ctrl_key_cable(size_t count, const uint8_t *renewed_key, const uint8_t *ctrl_key)
{
    ctrl_msg_t *ctrl_msg = init_ctrl(count, renewed_key);
    wire_t *wire = init_wire_from_ctrl_msg(ctrl_msg);
    size_t len = wire_get_length(wire);
    xfree(ctrl_msg);
    encrypt_wire(wire, ctrl_key);
    cable_t *cable = init_cable(wire, &len);
    xfree(wire);
    return cable;
}
