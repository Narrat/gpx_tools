/* This file is part of the package gpx_tools
 * - Main source file for gpxrewrite tool
 * 
 * gpx_tools is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 * 
 * gpx_tools is distributed in the hope that it will be useful but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#define NEED_STDIO_H
#define NEED_STDLIB_H
#define NEED_STRING_H
#include "include.h"
#include "ini_settings.h"
#include "mem_str.h"
#include "waypoint.h"


typedef struct app_data 
{
	FILE *fpout;
	SettingsStruct *settings;
	int wrote_newline;
} AppData;


char *GetFormattedCacheType(Waypoint_Info *wpi)
{
	char *formattedType = NULL;
	int i;

	AppendStringN(&formattedType, &(wpi->WaypointXML[wpi->type2_off]),
			wpi->type2_len);
    DEBUG(formattedType);

	if (formattedType == NULL || strlen(formattedType) == 0) 
	{
		AppendStringN(&formattedType, &(wpi->WaypointXML[wpi->type_off]),
			wpi->type_len);
        DEBUG(formattedType);
	}

	if (formattedType == NULL || strlen(formattedType) == 0) {
		AppendString(&formattedType, "No_Type");
		return formattedType;
	}

	i = 0;
	while (formattedType[i] != ' ' && formattedType[i] != '-' &&
			formattedType[i] != '\0')
	{
		if (formattedType[i] == '|') {
			formattedType[i] = '_';
		}
		i ++;
	}
	formattedType[i] = '\0';

	if (strcmp(formattedType, "Cache") == 0)
	{
		freeMemory((void **) &formattedType);
		AppendString(&formattedType, "CITO_Event");
	}
	else if (strcmp(formattedType, "Letterbox") == 0 ||
			strcmp(formattedType, "Project") == 0)
	{
		formattedType[i] = '_';
	}

	return formattedType;
}


char *AssembleFormat(Waypoint_Info *wpi, AppData *ad, 
		char *Format, char *NameType)
{
	// Format is "Percent Code [Length]" where Length is optional
	// and can be 0 (auto-fit), or a number (up to that number)
	// 
	// %C = code as specified with "CACHETYPE_Prefix"
	// %Y = Cache type, spelled out
	// %I = ID (the xxxx part of GCxxxx)
	// %i = ID (Like %i but always removes first 2 chars)
	// %p = First 2 chars (what's removed by %i)
	// %D = Difficulty as 1 or 3 character string
	// %d = Difficulty as a single digit, 1-9
	// %T = Terrain as 1 or 3 character string
	// %t = Terrain as a single digit, 1-9
	// %S = Cache size, or No_Size preference if no size listed
	// %s = First letter of cache size
	// %O = Owner
	// %P = Placed by
	// %h = Hint
	// %N = Name of cache (smart truncated -- todo?)
	// %L = First letter of logs
	// 
	// %H = Hint (just for consistency)
	// %y = Same as %C
	// 
	// %b = Is there a bug?
	// %f = Did you find it?
	// %a = Is it active?
	// *** These use the preferences Bug_Yes, Bug_No, Found_Yes, Found_No,
	// Active_Yes, Active_No and defaults to the single Y or N character.
	// 
	// %% = Literal percent symbol
	// %0 - %9 = Literal number
	// %n = Newline character as detected from the XML
	// 
	// Anything else is copied verbatim

	int length, i, j, k, max_length, should_escape, strip_ws;
	char *out = NULL, *lastEnd, *current, *tmp = NULL, *tmp2 = NULL;
	char c, *allowed;

	AppendString(&tmp, NameType);
	AppendString(&tmp, "_Allowed_Chars");
	allowed = GetSetting(ad->settings, tmp);
	freeMemory((void **) &tmp);

	AppendString(&tmp, NameType);
	AppendString(&tmp, "_Max_Length");
	tmp2 = GetSetting(ad->settings, tmp);
	freeMemory((void **) &tmp);
	max_length = 0;
	if (tmp2 != NULL && tmp2[0] >= '0' && tmp2[0] <= '9')
	{
		i = 0;
		while (tmp2[i] >= '0' && tmp2[i] <= '9')
		{
			max_length *= 10;
			max_length += tmp2[i] - '0';
			i ++;
		}
	}
	tmp2 = NULL;

	lastEnd = Format;
	current = Format;
	should_escape = 1;
	DEBUG("AssembleFormat");
	while (*current != '\0')
	{
		if (*current != '%')
		{
			current ++;
		}
		else
		{
			// Copy the last chunk to out
			if (lastEnd != current)
			{
				AppendStringN(&out, lastEnd, current - lastEnd);
			}

			// Handle the % code
			length = -1;
			strip_ws = 1;
			current ++;
			switch (*current)
			{
				case 'a': // Active
					DEBUG("AssembleFormat a");
					if (wpi->available) 
					{
						AppendString(&tmp, GetSetting(ad->settings, "Active_Yes"));
					}
					else
					{
						AppendString(&tmp, GetSetting(ad->settings, "Active_No"));
					}
					break;

				case 'b': // Bugs
					DEBUG("AssembleFormat b");
					if (wpi->bugs)
					{
						AppendString(&tmp, GetSetting(ad->settings, "Bug_Yes"));
					}
					else
					{
						AppendString(&tmp, GetSetting(ad->settings, "Bug_No"));
					}
					break;

				case 'C': // Code, as specified with "CACHETYPE_Prefix"
				case 'y': // Added to be consistent
					DEBUG("AssembleFormat Cy");
					tmp2 = GetFormattedCacheType(wpi);
					AppendString(&tmp2, "_Prefix");
					AppendString(&tmp, GetSetting(ad->settings, tmp2));
					freeMemory((void **) &tmp2);
					break;

				case 'D': // Difficulty, full string
					DEBUG("AssembleFormat D");
					AppendStringN(&tmp, 
							&(wpi->WaypointXML[wpi->difficulty_off]),
							wpi->difficulty_len);
					break;

				case 'd': // Difficulty, as single digit
					DEBUG("AssembleFormat d");
					i = ChangeToSingleNumber(&(wpi->WaypointXML[wpi->difficulty_off]),
							wpi->difficulty_len);
					c = i + '0';
					AppendStringN(&tmp, &c, 1);
					break;

				case 'f': // Found it
					DEBUG("AssembleFormat f");
					if (wpi->available) 
					{
						AppendString(&tmp, GetSetting(ad->settings, "Found_Yes"));
					}
					else
					{
						AppendString(&tmp, GetSetting(ad->settings, "Found_No"));
					}
					break;

				case 'h': // Hint, full text
				case 'H': // Added to be consistent
					DEBUG("AssembleFormat Hh");
					AppendStringN(&tmp2, &(wpi->WaypointXML[wpi->hints_off]),
							wpi->hints_len);
					HTMLUnescapeString(&tmp2);
					AppendString(&tmp, tmp2);
					freeMemory((void **) &tmp2);
					break;

				case 'I': // ID (the xxxx part of GCxxxx)
					DEBUG("AssembleFormat I");
					AppendStringN(&tmp, &(wpi->WaypointXML[wpi->name_off]),
							wpi->name_len);
					if (wpi->name_len > 2 && strncmp(tmp, "GC", 2) == 0)
					{
						tmp[0] = ' ';
						tmp[1] = ' ';
					}
					break;

				case 'i': // Like %I except always removes 2 chars
					DEBUG("AssembleFormat i");
					AppendStringN(&tmp, &(wpi->WaypointXML[wpi->name_off]),
							wpi->name_len);
					if (wpi->name_len > 2)
					{
						tmp[0] = ' ';
						tmp[1] = ' ';
					}
					break;

				case 'L': // First letter of the logs
					DEBUG("AssembleFormat L");
					AppendString(&tmp, wpi->logSummary);
					break;

				case 'n': // Newline
					DEBUG("AssembleFormat n");
					strip_ws = 0;
					AppendString(&tmp, GetXMLNewline());
					break;

				case 'N': // Cache name
					DEBUG("AssembleFormat N");
					// TODO: smart truncated - see doc/smart_truncation
					AppendStringN(&tmp2, &(wpi->WaypointXML[wpi->urlname_off]),
							wpi->urlname_len);
					HTMLUnescapeString(&tmp2);
					AppendString(&tmp, tmp2);
					freeMemory((void **) &tmp2);
					break;

				case 'O': // Owner, full name
					DEBUG("AssembleFormat O");
					AppendStringN(&tmp2, &(wpi->WaypointXML[wpi->owner_off]),
							wpi->owner_len);
					HTMLUnescapeString(&tmp2);
					AppendString(&tmp, tmp2);
					freeMemory((void **) &tmp2);
					break;

				case 'P': // Placed by, full name
					DEBUG("AssembleFormat P");
					AppendStringN(&tmp2, &(wpi->WaypointXML[wpi->placedBy_off]),
							wpi->placedBy_len);
					HTMLUnescapeString(&tmp2);
					AppendString(&tmp, tmp2);
					freeMemory((void **) &tmp2);
					break;

				case 'p': // Prefix; first 2 characters
					DEBUG("AssembleFormat p");
					AppendStringN(&tmp, &(wpi->WaypointXML[wpi->name_off]),
							wpi->name_len);
					if (wpi->name_len >= 2)
					{
						tmp[3] = ' ';
					}
					break;

				case 'S': // Size, full string
					DEBUG("AssembleFormat S");
					AppendStringN(&tmp, &(wpi->WaypointXML[wpi->container_off]),
							wpi->container_len);
					break;

				case 's': // Size, first letter
					DEBUG("AssembleFormat s");
					if (wpi->container_len)
					{
						AppendStringN(&tmp, &(wpi->WaypointXML[wpi->container_off]),
								1);
					}
					else
					{
						DEBUG("D");
						AppendString(&tmp, GetSetting(ad->settings, "No_Size"));
						DEBUG("E");
					}
					break;

				case 'T': // Terrain, full string
					DEBUG("AssembleFormat T");
					AppendStringN(&tmp, &(wpi->WaypointXML[wpi->terrain_off]),
							wpi->terrain_len);
					break;

				case 't': // Terrain, as single digit
					DEBUG("AssembleFormat t");
					i = ChangeToSingleNumber(&(wpi->WaypointXML[wpi->terrain_off]),
							wpi->terrain_len);
					c = i + '0';
					AppendStringN(&tmp, &c, 1);
					break;

				case 'Y': // Cache type, full name
					DEBUG("AssembleFormat Y");
					AppendStringN(&tmp, &(wpi->WaypointXML[wpi->type2_off]),
							wpi->type2_len);
					break;

				case '%': // Literals
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					DEBUG("AssembleFormat $0123456789");
					AppendStringN(&tmp, current, 1);
					length = -2;
					break;

				default:
					DEBUG("AssembleFormat default");
					AppendStringN(&tmp, current - 1, 2);
					length = -2;
			}
			DEBUG(tmp);

			current ++;

			// Remove beginning whitespace and strip invalid characters
			i = 0;
			j = 0;
			DEBUG("Strip beginning whitespace and bad characters");
			if (tmp)
			{
				if (strip_ws) 
				{
					while (tmp[i] == '\t' || tmp[i] == '\r' || 
							tmp[i] == '\n' || tmp[i] == ' ')
					{
						i ++;
					}
				}

				while (tmp[i] != '\0')
				{
					c = tmp[i];
					if ((c >= '0' && c <= '9') ||
							(c >= 'A' && c <= 'Z') ||
							(c >= 'a' && c <= 'z') ||
							c == ' ' || c == '.' || c == '\r' || c == '\n')
					{
						// A "known good" letter
						// I may need to rethink the \r and \n later
						// to find a way to put them into the allowed
						// list
						tmp[j] = c;
						j ++;
					}
					else if (allowed == NULL)
					{
						// No allowed list, so let's say all are allowed
						tmp[j] = c;
						j ++;
					}
					else if (allowed[0] != '\0')
					{
						for (k = 0; k >= 0 && allowed[k] != '\0'; k ++)
						{
							if (allowed[k] == c)
							{
								// A letter specified in the ini file
								tmp[j] = c;
								j ++;
								// k gets incremented at the end of the
								// loop, so -1 won't work here
								k = -2;
							}
						}
					}
					i ++;
				}
				tmp[j] = '\0';
				j --;

				// Remove trailing whitespace
				if (strip_ws) 
				{
					while (tmp[j] == '\t' || tmp[j] == '\r' || 
							tmp[j] == '\n' || tmp[j] == ' ')
					{
						tmp[j] = '\0';
						j --;
					}
				}

				// Handle length specifiers (numbers after the code) if
				// length == -1
				if (length == -1 && *current >= '0' && *current <= '9')
				{
					DEBUG("Finding length");
					// Find and parse a number after the format code
					length = 0;
					while (*current >= '0' && *current <= '9')
					{
						length *= 10;
						length += *current - '0';
						current ++;
					}
				}
				if (length > 0)
				{
					// Static length - Easy
					if (strlen(tmp) > length)
						tmp[length] = '\0';
				}

				if (length == 0)
				{
					// AutoFit - we handle this a bit later.
					c = 0xFF;
					AppendStringN(&out, &c, 1);
					AppendString(&out, tmp);
					AppendStringN(&out, &c, 1);
				}
				else
				{
					AppendString(&out, tmp);
				}

				// Finish up
				freeMemory((void **) &tmp);
			}
			lastEnd = current;
		}
	}
	DEBUG("Copy remaining characters");

	// Copy any remaining characters
	if (lastEnd != current)
	{
		DEBUG("Copy remaining characters - Append");
		AppendStringN(&out, lastEnd, current - lastEnd);
	}

	if (! out) {
		// Make an empty string to return in case there was nothing
		c = 0x00;
		DEBUG("Copy remaining characters - Empty");
		AppendString(&out, "");
	}

	// Reformat the auto-size fields
	if (max_length > 0)
	{
		DEBUG("Copy remaining characters - Resize");
		AutoSizeString(out, max_length);

		if (strlen(out) > max_length) 
		{
			out[max_length] = '\0';
		}
	}

	return out;
}


// Static char or NULL if no change.
// Needs to be HTML escaped.
char *BuildSymTag(Waypoint_Info *wpi, AppData *ad)
{
	char *formattedType = NULL;
	char *keyName = NULL, *value = NULL, *foundStatus = NULL, *symbol = NULL;

	DEBUG("BuildSymTag");

	if (wpi->sym_len == 0)
	{
		DEBUG("Empty symbol length");
		return (char *) NULL;
	}

	formattedType = GetFormattedCacheType(wpi);
	AppendStringN(&symbol, &(wpi->WaypointXML[wpi->sym_off]), wpi->sym_len);
	DEBUG(symbol);
    DEBUG(formattedType);

	if (strcmp("Geocache Found", symbol) == 0)
	{
		DEBUG("Geocache Found");
		AppendString(&foundStatus, "Found");
	}
	else if (strcmp("Geocache", symbol) == 0)
	{
		DEBUG("Geocache (not found)");
		AppendString(&foundStatus, "Not_Found");
	}
	else if (strncmp(formattedType, "Waypoint_", 9) == 0)
	{
		DEBUG("Waypoint");
		value = GetSetting(ad->settings, formattedType);
		freeMemory((void **) &keyName);
		freeMemory((void **) &formattedType);
		freeMemory((void **) &symbol);
		return value;
	}

	// TYPE_SIZE_FOUND
	AppendString(&keyName, formattedType);
	AppendString(&keyName, "_");
	AppendStringN(&keyName, &(wpi->WaypointXML[wpi->container_off]),
			wpi->container_len);
	AppendString(&keyName, "_");
	AppendString(&keyName, foundStatus);
	DEBUG("Finding with TYPE_SIZE_FOUND");
	value = GetSetting(ad->settings, keyName);
	freeMemory((void **) &keyName);

	// TYPE_FOUND
	if (value == NULL) 
	{
		DEBUG("Finding with TYPE_FOUND");
		AppendString(&keyName, formattedType);
		AppendString(&keyName, "_");
		AppendString(&keyName, foundStatus);
		value = GetSetting(ad->settings, keyName);
		freeMemory((void **) &keyName);
	}

	// FOUND
	// Can only work if the <sym> element exists.
	if (value == NULL) 
	{
		DEBUG("Finding with FOUND");
		AppendString(&keyName, foundStatus);
		value = GetSetting(ad->settings, keyName);
		freeMemory((void **) &keyName);
	}

	freeMemory((void **) &formattedType);
	freeMemory((void **) &foundStatus);
	freeMemory((void **) &symbol);

	return value;
}


void WriteFormattedTags(Waypoint_Info *wpi, AppData *ad)
{
	char *format, *output = NULL;

	format = GetSetting(ad->settings, "Waypoint_Format");
	if (format != NULL)
	{
		output = AssembleFormat(wpi, ad, format, "Waypoint");
	}
	else
	{
		AppendStringN(&output, &(wpi->WaypointXML[wpi->name_off]),
				wpi->name_len);
	}

	HTMLEscapeString(&output);
	SwapWaypointString(wpi, wpi->name_off, wpi->name_len, output);
	freeMemory((void **) &output);

	format = GetSetting(ad->settings, "Desc_Format");
	if (format != NULL)
	{
		output = AssembleFormat(wpi, ad, format, "Desc");
	}
	else
	{
		AppendStringN(&output, &(wpi->WaypointXML[wpi->gcname_off]),
				wpi->gcname_len);
	}

	HTMLEscapeString(&output);

	if (wpi->gcname_off) 
	{
		SwapWaypointString(wpi, wpi->gcname_off, wpi->gcname_len, output);
	}

	if (wpi->desc_off)
	{
		SwapWaypointString(wpi, wpi->desc_off, wpi->desc_len, output);
	}

	freeMemory((void **) &output);

	output = BuildSymTag(wpi, ad);
	SwapWaypointString(wpi, wpi->sym_off, wpi->sym_len, output);
	// Don't free this one
}


void WriteDefaultSettings(SettingsStruct **head)
{
	WriteSetting(head, "Benchmark_Prefix", "X");
	WriteSetting(head, "CITO_Event_Prefix", "C");
	WriteSetting(head, "Earthcache_Prefix", "G");
	WriteSetting(head, "Event_Prefix", "E");
	WriteSetting(head, "Letterbox_Hybrid_Prefix", "B");
	WriteSetting(head, "Locationless_Prefix", "L");
	WriteSetting(head, "Mega_Prefix", "E");
	WriteSetting(head, "Multi_Prefix", "M");
	WriteSetting(head, "No_Type_Prefix", "W");
	WriteSetting(head, "Project_APE_Prefix", "A");
	WriteSetting(head, "Traditional_Prefix", "T");
	WriteSetting(head, "Unknown_Prefix", "U");
	WriteSetting(head, "Virtual_Prefix", "V");
	WriteSetting(head, "Webcam_Prefix", "W");

	WriteSetting(head, "Active_Yes", "Y");
	WriteSetting(head, "Active_No", "N");
	WriteSetting(head, "Bug_Yes", "Y");
	WriteSetting(head, "Bug_No", "N");
	WriteSetting(head, "Found_Yes", "Y");
	WriteSetting(head, "Found_No", "N");
}


void WaypointHandler(Waypoint_Info *wpi, void *extra_data)
{
	AppData *ad = (AppData *) extra_data;

	if (! ad->wrote_newline) 
	{
		ad->wrote_newline = 1;
		fputs(GetXMLNewline(), ad->fpout);
	}   

	WriteFormattedTags(wpi, ad);

	fputs(wpi->WaypointXML, ad->fpout);
}


void NonWaypointHandler(const char *txt, int len, void *extra_data)
{
	char *out = NULL;
	AppData *ad = (AppData *) extra_data;

	if (! ad->wrote_newline) 
	{
		ad->wrote_newline = 1;
		fputs(GetXMLNewline(), ad->fpout);
	}

	AppendStringN(&out, txt, len);
	fputs(out, ad->fpout);
	freeMemory((void **) &out);
}


int main(int argc, char **argv)
{
	FILE *fpin;
	AppData *ad;

	if (argc < 2 || argc > 4)
	{
		fprintf(stderr, "gpxrewrite - Part of %s\n", PACKAGE_STRING);
		fprintf(stderr, "Error:  Incorrect arguments\n");
		fprintf(stderr, "    gpxrewrite settings.ini   (reads file from stdin, writes to stdout)\n");
		fprintf(stderr, "    gpxrewrite settings.ini gpxfile.gpx   (writes to stdout)\n");
		fprintf(stderr, "    gpxrewrite settings.ini gpxfile.gpx output.gpx\n");
		exit(1);
	}

	DEBUG("Get memory");
	ad = getMemory(sizeof(AppData));
	DEBUG("Write default settings");
	WriteDefaultSettings(&(ad->settings));
	DEBUG("Read settings file");
	ReadSettings(&(ad->settings), argv[1]);

	if (argc >= 3)
	{
		DEBUG("Opening input file");
		fpin = fopen(argv[2], "r");
		if (! fpin)
		{
			fprintf(stderr, "Error opening input file: %s\n", argv[2]);
			exit(3);
		}
	}
	else
	{
		DEBUG("Using stdin");
		fpin = stdin;
	}

	if (argc >= 4)
	{
		DEBUG("Opening output file");
		ad->fpout = fopen(argv[3], "w");
		if (! ad->fpout)
		{
			fprintf(stderr, "Error opening output file: %s\n", argv[3]);
			exit(4);
		}
	}
	else
	{
		DEBUG("Using stdout");
		ad->fpout = stdout;
	}

	DEBUG("Writing XML header");
	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>", ad->fpout);
	DEBUG("Parsing XML");
	ParseXML(fpin, &WaypointHandler, &NonWaypointHandler, (void *) ad);
	DEBUG("Writing trailing newline");
	fputs(GetXMLNewline(), ad->fpout);

	if (fpin != stdin)
	{
		DEBUG("Closing input file");
		fclose(fpin);
	}

	if (ad->fpout != stdout)
	{
		DEBUG("Closing output file");
		fclose(ad->fpout);
	}

	DEBUG("Freeing settings");
	FreeSettings(&(ad->settings));
	DEBUG("Freeing memory");
	freeMemory((void **) &ad);

	return 0;
}
