/**
 * @file count.h
 * @brief Implements various counters.
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
#ifndef HAVE_COUNT_H
#define HAVE_COUNT_H

/** Opaque type */
struct count;
struct count_series;

#define COUNT_MAX_COUNTERS 17

/**
 * Create a new instance of counters.
 * @param size Size of counters array.
 * @return     A pointer to a new count on success, NULL otherwise.
 */
struct count *count_create(int size);

/**
 * Destroy an instance of count.
 * @param count Pointer to count obtained via @ref count_create().
 */
void count_destroy(struct count *count);

/**
 * Add a new value to the count.
 * @param count Pointer to count obtained via @ref count_create().
 * @param type  Type of the counter.
 */
void count_update(struct count *count, int type);

struct count_result {
	int size;
	unsigned int counter[COUNT_MAX_COUNTERS];
};

/**
 * Obtain the results of the calculated statistics.
 * @param count  Pointer to count obtained via @ref count_create().
 * @param result Pointer to count_result to store the results.
 * @return       Zero on success, non-zero if no values were added.
 */
int count_get_result(struct count *count, struct count_result *result);

/**
 * Reset all statistics.
 * @param count Pointer to count obtained via @ref count_create().
 */
void count_reset(struct count *count);

/**
 * Create a new instance of a statistic series.
 * @param len Length of count serie.
 * @param size Size of counters array in serie.
 * @return    A pointer to a new count_series on success, NULL otherwise.
 */
struct count_series *count_series_create(int len, int size);

/**
 * Destroy an instance of count_series.
 * @param serie Pointer to count_series obtained via @ref count_series_create().
 */
void count_series_destroy(struct count_series *serie);

/**
 * Add a new value to the count.
 * @param serie Pointer to count_series obtained via @ref count_series_create().
 * @param type  Type of the counter.
 */
void count_series_update(struct count_series *serie, int type);

/**
 * Get current index in the serie.
 * @param serie Pointer to count_series obtained via @ref count_series_create().
 * @return      Current index in the serie.
 */
int count_series_get_index(struct count_series *serie);

/**
 * Obtain the results of the collected counters in a serie.
 * @param serie  Pointer to count_series obtained via @ref count_series_create().
 * @param result Pointer to count_result array to store the results.
 * @return       Zero on success, non-zero if no values were added.
 */
int count_series_get_result(struct count_series *serie, struct count_result *result);

/**
 * Advance to next count in series.
 * @param serie Pointer to count_series obtained via @ref count_series_create().
 */
void count_series_advance(struct count_series *serie);

#endif
