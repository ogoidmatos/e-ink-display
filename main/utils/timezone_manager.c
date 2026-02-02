
#include "timezone_manager.h"
#include <search.h>

#include <sys/time.h>
#include <sys/unistd.h>

void zones_hash_init(void)
{
	hcreate(ZONE_TABLE_SIZE);
	for (size_t i = 0; i < ZONE_TABLE_SIZE; ++i) {
		ENTRY e;
		e.key = (char*)zone_table[i].zone_name;
		e.data = (void*)zone_table[i].tz_string;
		hsearch(e, ENTER);
	}
}

const char* find_tz_by_zone(const char* zone_name)
{
	ENTRY e, *ep;
	e.key = (char*)zone_name;
	ep = hsearch(e, FIND);
	if (ep)
		return (const char*)ep->data;
	return NULL;
}

uint8_t convert_time_to_timezone(const char* zone_name,
								 const char* time_str,
								 char* output_time_string)
{
	const char* tz_string = find_tz_by_zone(zone_name);
	if (!tz_string) {
		return 1; // Time zone not found
	}

	// Set the TZ environment variable to the desired time zone
	if (setenv("TZ", tz_string, 1) != 0) {
		return 1; // Failed to set environment variable
	}
	tzset();

	// Parse the input time string - eg, 2026-01-20T19:14:59.289Z
	struct tm tm_time = { 0 };
	strptime(time_str, "%Y-%m-%dT%H:%M:%S", &tm_time);

	// Not great way to get the system libs to convert the time to time in the timezone, but it
	// works
	time_t raw_time = mktime(&tm_time);
	struct tm* local_time = localtime(&raw_time);

	// Format the converted time back to string
	strftime(output_time_string, 6, "%H:%M", local_time);
	return 0;
}