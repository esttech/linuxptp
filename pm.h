/**
 * @file pm.h
 * @brief Performance monitoring
 * @note Copyright (C) 2018 Anders Selhammer <anders.selhammer@est.tech>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef HAVE_PM_H
#define HAVE_PM_H

#include "stats.h"
#include "count.h"
#include "tmv.h"

#define PM_15M_TIMER 900
#define PM_QHOUR_DAY 96
#define PM_QHOUR_LEN 97
#define PM_DAILY_LEN 2

enum {
	MASTER_SLAVE_DELAY,
	SLAVE_MASTER_DELAY,
	MEAN_PATH_DELAY,
	OFFSET_FROM_MASTER,
	N_CLOCK_STATS
};

enum {
	/* E2E and P2P */
	ANNOUNCE_TX,
	ANNOUNCE_RX,
	ANNOUNCE_FOREIGN_MASTER_RX,
	SYNC_TX,
	SYNC_RX,
	FOLLOWUP_TX,
	FOLLOWUP_RX,
	/* E2E only */
	DELAY_REQ_TX,
	DELAY_REQ_RX,
	DELAY_RESP_TX,
	DELAY_RESP_RX,
	/* P2P only */
	PDELAY_REQ_TX,
	PDELAY_REQ_RX,
	PDELAY_RESP_TX,
	PDELAY_RESP_RX,
	PDELAY_RESP_FOLLOWUP_TX,
	PDELAY_RESP_FOLLOWUP_RX,
	N_MSG_COUNTERS
};


struct pm_head {
	time_t              pm_time;
	UInteger8           invalid;
};

/* E2E and P2P */
struct pm_clock_stats {
	int                 cycle_index;
	struct pm_head      qhour_head[PM_QHOUR_LEN];
	struct pm_head      daily_head[PM_DAILY_LEN];
	struct stats_series *qhour[N_CLOCK_STATS];
	struct stats_series *daily[N_CLOCK_STATS];
};

/* P2P only */
struct pm_port_stats {
	struct stats_series *qhour;
	struct stats_series *daily;
};

/* E2E and P2P */
struct pm_port_counters {
	struct count_series *qhour;
	struct count_series *daily;
};

#endif
