/**
 * @file stats.c
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
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "stats.h"

struct stats {
	unsigned int num;
	double min;
	double max;
	double mean;
	double sum_sqr;
	double sum_diff_sqr;
};

struct stats_series {
	int length;
	int index;
	int count;
	struct stats *instance[];
};

struct stats *stats_create(void)
{
	struct stats *stats;

	stats = calloc(1, sizeof *stats);
	return stats;
}

void stats_destroy(struct stats *stats)
{
	free(stats);
}

void stats_add_value(struct stats *stats, double value)
{
	double old_mean = stats->mean;

	if (!stats->num || stats->max < value)
		stats->max = value;
	if (!stats->num || stats->min > value)
		stats->min = value;

	stats->num++;
	stats->mean = old_mean + (value - old_mean) / stats->num;
	stats->sum_sqr += value * value;
	stats->sum_diff_sqr += (value - old_mean) * (value - stats->mean);
}

unsigned int stats_get_num_values(struct stats *stats)
{
	return stats->num;
}

int stats_get_result(struct stats *stats, struct stats_result *result)
{
	if (!stats->num) {
		return -1;
	}

	result->min = stats->min;
	result->max = stats->max;
	result->max_abs = stats->max > -stats->min ? stats->max : -stats->min;
	result->mean = stats->mean;
	result->rms = sqrt(stats->sum_sqr / stats->num);
	result->stddev = sqrt(stats->sum_diff_sqr / stats->num);

	return 0;
}

void stats_reset(struct stats *stats)
{
	memset(stats, 0, sizeof *stats);
}

struct stats_series *stats_series_create(int len)
{
	struct stats_series *serie;
	int i;

	serie = calloc(1, sizeof (*serie) + len * sizeof (struct stats*));
	if (!serie) {
		goto out;
	}

	serie->length = len;
	for (i = 0; i < len; i++) {
		serie->instance[i] = stats_create();
		if (!serie->instance[i]) {
			goto destroy;
		}
	}

	return serie;
destroy:
	stats_series_destroy(serie);
out:
	return NULL;
}

void stats_series_destroy(struct stats_series *serie)
{
	int i;

	for (i = 0; i < serie->length; ++i) {
		stats_destroy(serie->instance[i]);
	}
	free(serie);
}

void stats_series_add_value(struct stats_series *serie, double value)
{
	stats_add_value(serie->instance[serie->index], value);
}

int stats_series_get_index(struct stats_series *serie)
{
	return serie->index;
}

int stats_series_get_result(struct stats_series *serie, struct stats_result *result)
{
	int i;
	for (i = 0; i < serie->length && i < serie->count; ++i) {
		if (stats_get_result(serie->instance[i], &result[i])) {
			return -1;
		}
	}

	return 0;
}

void stats_series_advance(struct stats_series *serie)
{
	serie->index = (1 + serie->index) % serie->length;
	stats_reset(serie->instance[serie->index]);
	if (serie->count < serie->length) {
		serie->count++;
	}
}

