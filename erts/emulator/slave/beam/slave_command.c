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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "sys.h"
#include "slave_command.h"
#include "erl_bif_table.h"
#include "epiphany.h"

/* Set by the master */
EPIPHANY_SRAM_DATA struct slave_command_buffers *volatile slave_command_buffers;

struct slave_timer_state slave_pause_timers(void)
{
    struct slave_timer_state ret;
    ret.config = e_reg_read(E_REG_CONFIG);
    e_ctimer_stop(E_CTIMER_0);
    e_ctimer_stop(E_CTIMER_1);
    return ret;
}

void slave_resume_timers(struct slave_timer_state state)
{
    e_ctimer_start(E_CTIMER_0, (state.config >> 4) & 0xf);
    e_ctimer_start(E_CTIMER_1, (state.config >> 8) & 0xf);
}

static void
await_command_buffers(void)
{
    if (slave_command_buffers == NULL)
	erts_printf("Waiting for command buffers to become available\n");
    while (slave_command_buffers == NULL);
}

void
erts_master_setup(void)
{
    struct master_command_setup cmd = {
	.target = &loader_target_self,
	.num_instructions = num_instructions,
	.bif_table = bif_table,
	.bif_size = BIF_SIZE,
    };

    erts_master_send_command(MASTER_COMMAND_SETUP, &cmd, sizeof(cmd));
}

void
erts_master_setup_core(ErtsSchedulerData *esdp)
{
    struct master_command_setup_core cmd = {
	.x_reg_array = esdp->x_reg_array,
    };

    erts_master_send_command(MASTER_COMMAND_SETUP_CORE, &cmd, sizeof(cmd));
}

void
erts_master_send_command(enum master_command code, const void *data, size_t size)
{
    await_command_buffers();
    erts_fifo_write_blocking(&slave_command_buffers->master, &code, sizeof(code));
    erts_fifo_write_blocking(&slave_command_buffers->master, data, size);
}

void
erts_master_syscall(enum slave_syscall no, void *arg)
{
    struct slave_timer_state ts = slave_pause_timers();
    ASSERT(epiphany_in_dram(arg));
    ASSERT(no > 0);
    await_command_buffers();
    slave_command_buffers->syscall_arg = arg;
    slave_command_buffers->syscall = no;
    while(slave_command_buffers->syscall != 0) {
	if (no == SLAVE_SYSCALL_READY)
	    epiphany_sleep_us(500);
    }
    slave_resume_timers(ts);
}

int
erts_dispatch_slave_commands(Process *c_p)
{
    struct erl_fifo *fifo;
    enum slave_command cmd;
    size_t available;
    ASSERT(slave_command_buffers);
    fifo = &slave_command_buffers->slave;

    available = erts_fifo_available(fifo);
    if (available < sizeof(enum slave_command)) return 0;
    erts_fifo_peek(fifo, &cmd, sizeof(enum slave_command));
    available -= sizeof(enum slave_command);

#define MESSAGE(TYPE, NAME)					\
    TYPE NAME;							\
    if (available < sizeof(NAME)) return 0;			\
    erts_fifo_skip(fifo, sizeof(enum slave_command));		\
    erts_fifo_read_blocking(fifo, &NAME, sizeof(NAME))

    switch (cmd) {
    case SLAVE_COMMAND_MESSAGE: {
	MESSAGE(struct slave_command_message, msg);
	slave_serve_message(c_p, &msg);
	return 1;
    }
    case SLAVE_COMMAND_EXIT: {
	MESSAGE(struct slave_command_exit, msg);
	slave_serve_exit(c_p, &msg);
	return 1;
    }
    case SLAVE_COMMAND_TIMEOUT: {
	MESSAGE(struct slave_command_timeout, msg);
	slave_serve_timeout(c_p, &msg);
	return 1;
    }
    default:
	erl_exit(1,
		 "Cannot pop unrecognized message %d from master fifo\n",
		 (int)cmd);
    }
#undef MESSAGE
}
