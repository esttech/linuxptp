/**
 * @file stats.h
 * @brief Implements various statistics.
 * @note Copyright (C) 2013 Miroslav Lichvar <mlichvar@redhat.com>
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
#ifndef HAVE_STATS_H
#define HAVE_STATS_H

/** Opaque type */
struct stats;
struct stats_series;

/**
 * Create a new instance of statistics.
 * @return A pointer to a new stats on success, NULL otherwise.
 */
struct stats *stats_create(void);

/**
 * Destroy an instance of stats.
 * @param stats Pointer to stats obtained via @ref stats_create().
 */
void stats_destroy(struct stats *stats);

/**
 * Add a new value to the stats.
 * @param stats Pointer to stats obtained via @ref stats_create().
 * @param value The measured value.
 */
void stats_add_value(struct stats *stats, double value);

/**
 * Get the number of values collected in the stats so far.
 * @param stats Pointer to stats obtained via @ref stats_create().
 * @return      The number of values.
 */
unsigned int stats_get_num_values(struct stats *stats);

struct stats_result {
	double min;
	double max;
	double max_abs;
	double mean;
	double rms;
	double stddev;
};

/**
 * Obtain the results of the calculated statistics.
 * @param stats  Pointer to stats obtained via @ref stats_create().
 * @param result Pointer to stats_result to store the results.
 * @return       Zero on success, non-zero if no values were added.
 */
int stats_get_result(struct stats *stats, struct stats_result *result);

/**
 * Reset all statistics.
 * @param stats Pointer to stats obtained via @ref stats_create().
 */
void stats_reset(struct stats *stats);

/**
 * Create a new instance of a statistic series.
 * @param len Length of stats serie.
 * @return    A pointer to a new stats_series on success, NULL otherwise.
 */
struct stats_series *stats_series_create(int len);

/**
 * Destroy an instance of stats_series.
 * @param serie Pointer to stats_series obtained via @ref stats_series_create().
 */
void stats_series_destroy(struct stats_series *serie);

/**
 * Add a new value to the stats.
 * @param serie Pointer to stats_series obtained via @ref stats_series_create().
 * @param value The measured value.
 */
void stats_series_add_value(struct stats_series *serie, double value);

/**
 * Get current index in the serie.
 * @param serie Pointer to stats_series obtained via @ref stats_series_create().
 * @return      Current index in the serie.
 */
int stats_series_get_index(struct stats_series *serie);

/**
 * Obtain the results of the calculated statistics in a serie.
 * @param serie  Pointer to stats_series obtained via @ref stats_series_create().
 * @param result Pointer to stats_result array to store the results.
 * @return       Zero on success, non-zero if no values were added.
 */
int stats_series_get_result(struct stats_series *serie, struct stats_result *result);

/**
 * Advance to next stats in series.
 * @param serie Pointer to stats_series obtained via @ref stats_series_create().
 */
void stats_series_advance(struct stats_series *serie);

#endif
