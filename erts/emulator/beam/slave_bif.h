/*
 * %CopyrightBegin%
 *
 * Copyright Ericsson AB 2015. All Rights Reserved.
 *
 * The contents of this file are subject to the Erlang Public License,
 * Version 1.1, (the "License"); you may not use this file except in
 * compliance with the License. You should have received a copy of the
 * Erlang Public License along with this software. If not, it can be
 * retrieved online at http://www.erlang.org/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * %CopyrightEnd%
 *
 */

#ifndef SLAVE_BIF_H__
#define SLAVE_BIF_H__

#include "erl_term.h"
#include "erl_process.h"
#include "erl_bif_table.h"
#include "slave_command.h"

struct slave_syscall_bif {
    /* To master */
    int bif_no;
    Eterm args[3];
    /* Bidirectional */
    struct slave_state state;
    /* To slave */
    Uint32 state_flags;
    Eterm result;
} SLAVE_SHARED_DATA;

#ifdef ERTS_SLAVE
Eterm slave_syscall_bif(Uint bif_no, Process *p, Eterm args[], int arity);
#else
int erts_slave_serve_bif(struct slave *slave, struct slave_syscall_bif *arg);
#endif

#ifdef HIPE
/* This is the "X macro" pattern */
#define SLAVE_PROXIED_PRIMOPS_DEFINER				       \
    /* We'd prefer if race-free table access was available natively */ \
    X(hipe_find_na_or_make_stub, 3)				       \
    X(hipe_nonclosure_address, 2)

enum slave_primop {
    SLAVE_PRIMOP_INVALID, /* 0 cannot be used */
#define X(Name, Arity) SLAVE_PRIMOP_ ## Name,
    SLAVE_PROXIED_PRIMOPS_DEFINER
#undef X
    SLAVE_PRIMOP_BOUND,
};
#define SLAVE_PRIMOP_SIZE (SLAVE_PRIMOP_BOUND-1)

#endif

#endif /* SLAVE_BIF_H__ */
