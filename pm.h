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

#include <sys/queue.h>

#include "stats.h"
#include "tmv.h"

#define PM_15M_TIMER 900

typedef tmv_t PMTimestamp;

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
	UInteger16          index;
	PMTimestamp         PMTime;
};

/* E2E and P2P */
struct pm_clock_stats {
	TAILQ_ENTRY(pm_clock_stats) list;
	struct pm_head      head;
	UInteger8           measurementValid;
	UInteger8           periodComplete;
	struct stats        *masterSlaveDelay;
	struct stats        *slaveMasterDelay;
	struct stats        *meanPathDelay;
	struct stats        *offsetFromMaster;
};

/* P2P only */
struct pm_port_stats {
	TAILQ_ENTRY(pm_port_stats) list;
	struct pm_head      head;
	struct stats        *meanLinkDelay;
};

/* E2E and P2P */
struct pm_port_counters {
	TAILQ_ENTRY(pm_port_counters) list;
	struct pm_head      head;
	UInteger32          counter[N_MSG_COUNTERS];
};

struct pm_clock_recordlist {
	TAILQ_HEAD(pm_clock_15_stats_head, pm_clock_stats) record15_stats;
	TAILQ_HEAD(pm_clock_24_stats_head, pm_clock_stats) record24_stats;
};

struct pm_port_recordlist {
	TAILQ_HEAD(pm_port_15_stats_head, pm_port_stats) record15_stats;
	TAILQ_HEAD(pm_port_24_stats_head, pm_port_stats) record24_stats;
	TAILQ_HEAD(pm_port_15_counters_head, pm_port_counters) record15_cnt;
	TAILQ_HEAD(pm_port_24_counters_head, pm_port_counters) record24_cnt;
};

/**
 * Creates stats instances for clock statistics.
 * @param cr Handle to current record.
 * @return   Zero on success, non-zero if the message is invalid.
 */
int pm_create_clock_stats(struct pm_clock_stats *cr);

/**
 * Creates stats instances for port statistics.
 * @param cr Handle to current record.
 * @return   Zero on success, non-zero if the message is invalid.
 */
int pm_create_port_stats(struct pm_port_stats *cr);

/**
 * Reset the stats instances for clock statistics.
 * @param cr Handle to current record.
 */
void pm_reset_clock_stats(struct pm_clock_stats *cr);

/**
 * Reset the stats instances for port statistics.
 * @param cr Handle to current record.
 */
void pm_reset_port_stats(struct pm_port_stats *cr);

/**
 * Destroys stats instances for clock statistics.
 * @param cr Handle to current record.
 */
void pm_destroy_clock_stats(struct pm_clock_stats *cr);

/**
 * Destroys stats instances for port statistics.
 * @param cr Handle to current record.
 */
void pm_destroy_port_stats(struct pm_port_stats *cr);

/**
 * Clear the record list and frees all the memory.
 * @param rl Handle to clock recordlist.
 */
void pm_free_clock_recordlist(struct pm_clock_recordlist *rl);

/**
 * Clear the record list and frees all the memory.
 * @param rl Handle to port recordlist.
 */
void pm_free_port_recordlist(struct pm_port_recordlist *rl);

/**
 * Update clock stats 15 minutes and 24 hour recordlist.
 * @param cr Handle to current record to store.
 * @param rl Handle to recordlist.
 * @return   Zero on success, non-zero if the message is invalid.
 */
int pm_update_clock_stats_recordlist(struct pm_clock_stats *cr,
				     struct pm_clock_recordlist *rl);

/**
 * Update port stats 15 minutes and 24 hour recordlist.
 * @param cr Handle to current record to store.
 * @param rl Handle to recordlist.
 * @return   Zero on success, non-zero if the message is invalid.
 */
int pm_update_port_stats_recordlist(struct pm_port_stats *cr,
				    struct pm_port_recordlist *rl);

/**
 * Update port counters 15 minutes and 24 hour recordlist.
 * @param cr Handle to current record to store.
 * @param rl Handle to recordlist.
 * @return   Zero on success, non-zero if the message is invalid.
 */
int pm_update_port_counters_recordlist(struct pm_port_counters *cr,
				       struct pm_port_recordlist *rl);

#endif
