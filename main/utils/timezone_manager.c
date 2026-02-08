#include "timezone_manager.h"
#include <search.h>
#include <stdio.h>

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

uint8_t convert_time_to_local(const char* zone_name, time_t time, struct tm* output_local_time)
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

	struct tm* local_time = localtime(&time);
	if (!local_time) {
		return 1; // Failed to convert time
	}

	*output_local_time = *local_time;
	return 0;
}

uint8_t tm_to_hour_min(const struct tm* timeinfo, char* output_time_string)
{
	if (strftime(output_time_string, 6, "%H:%M", timeinfo) == 0) {
		return 1; // Failed to format time
	}
	return 0;
}

void time_difference(const char* time_str1, const char* time_str2, char* output_time_string)
{
	struct tm tm_time1 = { 0 };
	struct tm tm_time2 = { 0 };

	strptime(time_str1, "%Y-%m-%dT%H:%M:%S", &tm_time1);
	strptime(time_str2, "%Y-%m-%dT%H:%M:%S", &tm_time2);

	time_t raw_time1 = mktime(&tm_time1);
	time_t raw_time2 = mktime(&tm_time2);

	double diff_seconds = difftime(raw_time2, raw_time1);
	int hours = (int)(diff_seconds / 3600);
	int minutes = (int)((diff_seconds - (hours * 3600)) / 60);

	if (hours == 0) {
		sprintf(output_time_string, "%d min", minutes);
	} else if (minutes == 0) {
		if (hours == 1) {
			sprintf(output_time_string, "%d hour", hours);
		} else {
			sprintf(output_time_string, "%d hours", hours);
		}
	} else {
		sprintf(output_time_string, "%d h %d min", hours, minutes);
	}
}