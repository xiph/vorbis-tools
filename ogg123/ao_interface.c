#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "ogg123.h"

devices_t *append_device(devices_t * devices_list, int driver_id,
			 ao_option * options, char *filename)
{
    devices_t *head = devices_list;

    if (devices_list != NULL) {
	while (devices_list->next_device != NULL)
	    devices_list = devices_list->next_device;
	devices_list = devices_list->next_device =
	    malloc(sizeof(devices_t));
    } else {
	head = devices_list = (devices_t *) malloc(sizeof(devices_t));
    }
    devices_list->driver_id = driver_id;
    devices_list->options = options;
    devices_list->filename = filename;
    devices_list->device = NULL;
    devices_list->next_device = NULL;

    return devices_list;
}

void devices_write(void *ptr, size_t size, devices_t * d)
{
    while (d != NULL) {
	ao_play(d->device, ptr, size);
	d = d->next_device;
    }
}

int add_option(ao_option ** op_h, const char *optstring)
{
    char *key, *value;
    int result;

    key = strdup(optstring);
    if (key == NULL)
	return 0;

    value = strchr(key, ':');
    if (value == NULL) {
	free(key);
	return 0;
    }

    /* split by replacing the separator with a null */
    *value++ = '\0';

    result = ao_append_option(op_h, key, value);
    free(key);

    return (result);
}
