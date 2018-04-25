/**
 * @file count.h
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
#include <string.h>
#include <stdlib.h>

#include "count.h"

struct count {
	int size;
	unsigned int counter[];
};

struct count_series {
	int length;
	int index;
	int count;
	struct count *instance[];
};

struct count *count_create(int size)
{
	struct count *count;

	if (COUNT_MAX_COUNTERS < size) {
		return NULL;
	}
	count = calloc(1, sizeof (*count) + size * sizeof (unsigned int));
	if (count) {
		count->size = size;
	}
	return count;
}

void count_destroy(struct count *count)
{
	free(count);
}

void count_update(struct count *count, int type)
{
	count->counter[type]++;
}

int count_get_result(struct count *count, struct count_result *result)
{
	memcpy(result, count, sizeof (*count) + count->size * sizeof (unsigned int));

	return 0;
}

void count_reset(struct count *count)
{
	memset(count, 0, sizeof (*count) + count->size * sizeof (unsigned int));
}

struct count_series *count_series_create(int len, int size)
{
	struct count_series *serie;
	int i;

	serie = calloc(1, sizeof (*serie) + len *
		       (sizeof (struct count) + size * sizeof (unsigned int)));
	if (!serie) {
		goto out;
	}

	serie->length = len;
	for (i = 0; i < len; i++) {
		serie->instance[i] = count_create(size);
		if (!serie->instance[i]) {
			goto destroy;
		}
	}

	return serie;
destroy:
	count_series_destroy(serie);
out:
	return NULL;
}

void count_series_destroy(struct count_series *serie)
{
	int i;

	for (i = 0; i < serie->length; ++i) {
		count_destroy(serie->instance[i]);
	}
	free(serie);
}

void count_series_update(struct count_series *serie, int type)
{
	count_update(serie->instance[serie->index], type);
}

int count_series_get_index(struct count_series *serie)
{
	return serie->index;
}

int count_series_get_result(struct count_series *serie, struct count_result *result)
{
	int i;
	for (i = 0; i < serie->length && i < serie->count; ++i) {
		if (count_get_result(serie->instance[i], &result[i])) {
			return -1;
		}
	}

	return 0;
}

void count_series_advance(struct count_series *serie)
{
	serie->index = (1 + serie->index) % serie->length;
	count_reset(serie->instance[serie->index]);
	if (serie->count < serie->length) {
		serie->count++;
	}
}

