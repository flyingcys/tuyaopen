/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2000-2001  Qualcomm Incorporated
 *  Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *  Copyright (C) 2002-2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 */

#ifndef __HCI_LIB_H
#define __HCI_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

struct hci_request {
    uint16_t ogf;
    uint16_t ocf;
    int event;
    void *cparam;
    int clen;
    void *rparam;
    int rlen;
};

static INLINE void hci_set_bit(int nr, void *addr)
{
    *((uint32_t *)addr + (nr >> 5)) |= (1 << (nr & 31));
}

static INLINE void hci_clear_bit(int nr, void *addr)
{
    *((uint32_t *)addr + (nr >> 5)) &= ~(1 << (nr & 31));
}

static INLINE int hci_test_bit(int nr, void *addr)
{
    return *((uint32_t *)addr + (nr >> 5)) & (1 << (nr & 31));
}

/* HCI filter tools */
static INLINE void hci_filter_clear(struct hci_filter *f)
{
    memset(f, 0, sizeof(*f));
}

static INLINE void hci_filter_set_ptype(int t, struct hci_filter *f)
{
    hci_set_bit((t == HCI_VENDOR_PKT) ? 0 : (t & HCI_FLT_TYPE_BITS), &f->type_mask);
}

static INLINE void hci_filter_clear_ptype(int t, struct hci_filter *f)
{
    hci_clear_bit((t == HCI_VENDOR_PKT) ? 0 : (t & HCI_FLT_TYPE_BITS), &f->type_mask);
}

static INLINE int hci_filter_test_ptype(int t, struct hci_filter *f)
{
    return hci_test_bit((t == HCI_VENDOR_PKT) ? 0 : (t & HCI_FLT_TYPE_BITS), &f->type_mask);
}

static INLINE void hci_filter_all_ptypes(struct hci_filter *f)
{
    memset((void *)&f->type_mask, 0xff, sizeof(f->type_mask));
}

static INLINE void hci_filter_set_event(int e, struct hci_filter *f)
{
    hci_set_bit((e & HCI_FLT_EVENT_BITS), &f->event_mask);
}

static INLINE void hci_filter_clear_event(int e, struct hci_filter *f)
{
    hci_clear_bit((e & HCI_FLT_EVENT_BITS), &f->event_mask);
}

static INLINE int hci_filter_test_event(int e, struct hci_filter *f)
{
    return hci_test_bit((e & HCI_FLT_EVENT_BITS), &f->event_mask);
}

static INLINE void hci_filter_all_events(struct hci_filter *f)
{
    memset((void *)f->event_mask, 0xff, sizeof(f->event_mask));
}

static INLINE void hci_filter_set_opcode(int opcode, struct hci_filter *f)
{
    f->opcode = opcode;
}

static INLINE void hci_filter_clear_opcode(struct hci_filter *f)
{
    f->opcode = 0;
}

static INLINE int hci_filter_test_opcode(int opcode, struct hci_filter *f)
{
    return (f->opcode == opcode);
}

#ifdef __cplusplus
}
#endif

#endif /* __HCI_LIB_H */
