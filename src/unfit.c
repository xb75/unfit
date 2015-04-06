
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <arpa/inet.h>

#define FIT_MAGIC 0x2E464954

#define UNFIT_VERSION "0.1"

#define TRUE 1
#define FALSE 0

struct gheader
{
	unsigned char size;
	unsigned char protocol_version;
	unsigned short profile_version;
	unsigned int data_size;
	unsigned int fit;
};

struct entry
{
	struct entry *next;
	struct entry *prev;
};

struct list
{
	struct entry *first;
	struct entry *last;
};

struct msg_def
{
	struct msg_def *next;
	struct msg_def *prev;
	unsigned int num;
	unsigned int type;
	unsigned int size;
	unsigned int fields[1];
};

struct msg_list
{
	struct msg_def *first;
	struct msg_def *last;
};

struct data_point
{
	struct data_point *next;
	struct data_point *prev;
	unsigned int timestamp;
	unsigned int heart_rate;
	unsigned int cadence;
	unsigned int speed;
	unsigned int distance;
	unsigned int altitude;
	unsigned int temperature;
};

struct data_list
{
	struct data_point *first;
	struct data_point *last;
};

struct msg_list defines = { NULL, NULL };
struct data_list g_data = { NULL, NULL };

/* Global variables dictate how program behaves */
int g_altitude;
int g_cadence;
int g_speed;
int g_distance;
int g_heartrate;
int g_temperature;
int g_timestamps;
int g_force_write;
int g_debug;
int g_add_missing;
char *g_time_format;
char *g_output;

static void
list_add(void *l, void *e)
{
	struct list *list = l;
	struct entry *entry = e;

	if (list->last)
	{
		list->last->next = entry;
	}
	else
	{
		list->first = entry;
	}
	entry->prev = list->last;
	entry->next = NULL;
	list->last = entry;
}

static struct msg_def *
find_type(struct msg_list *list, unsigned int type)
{
	struct msg_def *entry;

	entry = list->first;
	while (entry)
	{
		if (entry->type == type)
			return entry;
		entry = entry->next;
	}
	return NULL;
}

static void
dump_defs(struct msg_list *list)
{
	struct msg_def *entry;
	int i = 0;
	int j;

	if (g_debug == FALSE)
		return;

	if ((entry = list->first) == NULL)
	{
		printf("Empty list\n");
		return;
	}
	while (entry)
	{
		printf("DEF[%d] [n=%d] [t=%d] [s=%d] ", i, entry->num, entry->type, entry->size);
		for (j=0; j <entry->size; j++)
		{
			printf(" [%d %d %d] ", (entry->fields[j] >>16) & 0xFF, (entry->fields[j] >>8) & 0xFF, (entry->fields[j]) & 0xFF);
		}
		printf("\n");
		entry = entry->next;
		i++;
	}
}

int
read_header(FILE *fp, struct gheader *header)
{
	int rc;

	rc = fread(header, sizeof(struct gheader), 1, fp);
	if (rc != 1) {
		return -1;
	}

	if (header->fit != htonl(FIT_MAGIC)) {
		return -1;
	}

	if (header->size > 12) {
		fseek(fp, header->size - 12, SEEK_CUR);
	}
	if (g_debug)
		printf("Data size: %d\n", header->data_size);

	return 0;
}

int
read_data(FILE *fp, struct gheader *header, unsigned char **data)
{
	int rc;

	if (header->data_size <= 0 || header->data_size >= (1024*1024*10)) {
		return -1;
	}

	*data = malloc (header->data_size);
	if (*data == NULL) {
		return -1;
	}

	rc = fread(*data, 1, header->data_size, fp);
	if (rc != header->data_size) {
		return -1;
	}
	return 0;
}

void
dump_hex(const unsigned char *data, int size)
{
	int i;

	for (i=0; i<size; i++) {
		if (i == 0) {
		}
		else if ((i & 15) == 0) {
			printf("\n");
		}
		else if ((i & 3) == 0) {
			printf(" ");
		}
		else if ((i & 15) == 15) {
			//printf("\n");
		}
		printf("%02X", data[i]);
	}
	if ((i & 15) != 1) {
		printf("\n");
	}
}

const unsigned char *
create_type(int num, int type, int size, const unsigned char *p)
{
	struct msg_def *def;
	int i;
	int j;

	def = malloc(sizeof(struct msg_def) + (size-1)*sizeof(int));

	def->type = type;
	def->num = num;
	def->size = size;
	for (i=0; i<size; i++) {
		def->fields[i] = 0;
		for (j=0; j<3; j++) {
			def->fields[i] <<= 8;
			def->fields[i] |= p[0];
			p++;
		}
	}
	list_add(&defines, def);
	dump_defs(&defines);
	return p;
}

static void
print_name(int id)
{
	printf("{%d}", id);
}

static void
show_data(const unsigned char *data, int type, int name, int size)
{
	unsigned short us;
	short ss;
	unsigned int u32;
	int s32;
	char *x;

	print_name(name);

	switch (type)
	{
		case 0x00:
			printf("(enum)%d %02X\n", data[0], data[0]);
			break;
		case 0x01:
			printf("(s8)%d %02X\n", (signed)data[0], data[0]);
			break;
		case 0x02:
			printf("(u8)%d %02X\n", data[0], data[0]);
			break;
		case 0x07:
			x = malloc(size + 1);
			if (x) {
				memcpy(x, data, size);
				x[size] = '\0';
				printf("(string)%s\n", x);
				free(x);
			} else {
				printf("(string)[malloc(%d+1) failed]\n", size);
			}
			break;
		case 0x83: // 131
			ss = (int)data[0]*256 + data[1];
			printf("(short)%d %04X\n", ss, ss);
			break;
		case 0x84: // 132
			us = (unsigned int)data[0]*256 + data[1];
			printf("(unsigned short)%d %04X\n", us, us);
			break;
		case 0x85: // 133
			s32 = ((int)data[0]<<24) + ((unsigned int)data[1]<<16) + ((unsigned int)data[2] << 8) + data[3];
			printf("(int32)%d %08X\n", s32, s32);
			break;
		case 0x86: // 134
		case 0x8C: // 140
			u32 = ((unsigned int)data[0]<<24) + ((unsigned int)data[1]<<16) + ((unsigned int)data[2] << 8) + data[3];
			printf("(unsigned int32)%d %08X\n", u32, u32);
			break;
		default:
			printf("Unknown type %d %X\n", type, type);
			break;
	}
}

static const unsigned char *
get_value(int size, int type, const unsigned char *data, void *valuep)
{
	int *intp;

	intp = valuep;

	switch (type)
	{
		case 0x00:
		case 0x01:
		case 0x02:
			*intp = data[0];
			break;
		case 0x84: // 132
			*intp = data[1]*256 + data[0];
			break;
		case 0x86: // 134
			*intp = data[3]*256*256*256 + data[2]*256*256 + data[1]*256 + data[0];
			break;
		default:
			printf("TYPE = %d \n", type);
			break;
	}
	return data + size;
}

static void
get_timestamp(char *buf, int bufsz, unsigned int stamp)
{
	struct tm *tm;
	time_t tt;

	tt = stamp;
	tm = gmtime(&tt);

	if (g_time_format)
		strftime(buf, bufsz, g_time_format, tm);
	else
		strftime(buf, bufsz, "%Y-%m-%d %H:%M:%S", tm);
}

static const unsigned char *
show_data20(struct msg_def *def, const unsigned char *data)
{
	int name;
	int size;
	int type;
	int value;
	int i;
	struct data_point *snap;

	snap = malloc(sizeof(struct data_point));
	if (snap)
		memset(snap, 0, sizeof(struct data_point));


	for (i=0; i<def->size; i++)
	{
		name = ((def->fields[i] >> 16) & 0xFF);
		size = ((def->fields[i] >> 8) & 0xFF);
		type = ((def->fields[i]) & 0xFF);
		data = get_value(size, type, data, &value);
		if (snap)
		{
			if (name == 2)
				snap->altitude = value;
			else if (name == 3)
				snap->heart_rate = value;
			else if (name == 4)
				snap->cadence = value;
			else if (name == 5)
				snap->distance = value / 100; /* Convert to metres */
			else if (name == 6)
				snap->speed = (value * 3600) / 1000;
			else if (name == 13)
				snap->temperature = value;
			else if (name == 253)
				snap->timestamp = value + 631065600;
			else if (g_debug)
				printf("UNK(%d,%d,%d) ", name, size, type);
		}
	}
	list_add(&g_data, snap);
	return data;
}

const unsigned char *
parse_data(struct msg_def *def, const unsigned char *data)
{
	int i;
	int size;
	int name;

	if (def->num == 20)
	{
		return show_data20(def, data);
	}

	for (i=0; i<def->size; i++)
	{
		name = ((def->fields[i] >> 16) & 0xFF);
		size = ((def->fields[i] >> 8) & 0xFF);
		if (g_debug)
			show_data(data, def->fields[i] & 0xFF, name, size);

		data += size;
	}
	return data;
}

const unsigned char *
parse_record(const unsigned char *data)
{
	struct msg_def *def;
	const unsigned char *p;
	int n_fields;
	int msg_num;
	int msg_type;

	p = data;

	if ((p[0] & 0x80) == 0x80) {
		if (g_debug)
		{
			printf("Abnormal header -- TODO\n");
		}
	} else {
		// Normal header
		if ((p[0] & 0x40) == 0x40) {
			msg_type = p[0] & 0x0F;

			p++;
			// reserved = p[0];
			// arch = p[1];
			msg_num = ((unsigned int)(p[3])) << 8 | p[2]; // XXX ???
			n_fields = p[4];

			p += 5;

			p = create_type(msg_num, msg_type, n_fields, p);

			return p;
		} else {
			msg_type = p[0] & 0xF;
			def = find_type(&defines, msg_type);
			if (def == NULL)
			{
				printf("ERROR - unknown type %d\n", msg_type);
			}
			else
			{
				p = parse_data(def, p+1);
				return p;
			}
		}
	}

	printf("Parse record starting with %02X %02X %02X\n", p[0], p[1], p[2]);

	p++;

	return p;
}

int
dump_data(struct gheader *header, const unsigned char *data)
{
	const unsigned char *p;

	p = data;
	while (p < data + header->data_size)
	{
		if ((p = parse_record(p)) == NULL) {
			printf("unfit: parse error @%d\n", data - p);
			break;
		}
	}


	return 0;
}

int
decode_file(const char *fname)
{
	FILE *fp;
	struct gheader header;
	unsigned char *data;

	fp = fopen(fname, "rb");
	if (!fp) {
		fprintf(stderr, "Can't open file \"%s\"\n", fname);
		return -1;
	}

	if (read_header(fp, &header) != 0) {
		fclose(fp);
		return -1;
	}

	if (read_data(fp, &header, &data) != 0) {
		fclose(fp);
		return -1;
	}

	dump_data(&header, data);
	free(data);

	fclose(fp);
	return 0;
}

static void
set_default_config(void)
{
	g_altitude = TRUE;
	g_cadence = TRUE;
	g_speed = TRUE;
	g_distance = TRUE;
	g_heartrate = TRUE;
	g_temperature = TRUE;
	g_timestamps = TRUE;
	g_force_write = FALSE;
	g_debug = FALSE;
	g_add_missing = FALSE;

	g_time_format = NULL;
	g_output = NULL;
}

static int
parse_config_item(const char *option, const char *next, int cmdline)
{
	if (strcmp(option, "--no-altitude") == 0)
		g_altitude = FALSE;
	else if (strcmp(option, "--altitude") == 0)
		g_altitude = TRUE;
	else if (strcmp(option, "--no-speed") == 0)
		g_speed = FALSE;
	else if (strcmp(option, "--speed") == 0)
		g_speed = TRUE;
	else if (strcmp(option, "--no-cadence") == 0)
		g_cadence = FALSE;
	else if (strcmp(option, "--cadence") == 0)
		g_cadence = TRUE;
	else if (strcmp(option, "--no-distance") == 0)
		g_distance = FALSE;
	else if (strcmp(option, "--distance") == 0)
		g_distance = TRUE;
	else if (strcmp(option, "--no-time-stamps") == 0)
		g_timestamps = FALSE;
	else if (strcmp(option, "--time-stamps") == 0)
		g_timestamps = TRUE;
	else if (strcmp(option, "--no-temperature") == 0)
		g_temperature = FALSE;
	else if (strcmp(option, "--temperature") == 0)
		g_temperature = TRUE;
	else if (strcmp(option, "--no-heart-rate") == 0)
		g_heartrate = FALSE;
	else if (strcmp(option, "--heart-rate") == 0)
		g_heartrate = TRUE;
	else if (strcmp(option, "--time-format") == 0 && next && next[0] != '\0')
	{
		g_time_format = strdup(next);
		return 1;
	}
	else if (strcmp(option, "--no-debug") == 0)
		g_debug = FALSE;
	else if (strcmp(option, "--debug") == 0)
		g_debug = TRUE;
	else if (strcmp(option, "--missing-seconds") == 0 || strcmp(option, "-m") == 0)
		g_add_missing = TRUE;
	else if (strcmp(option, "--no-force-write") == 0)
		g_force_write = FALSE;
	else if (strcmp(option, "--force-write") == 0 || strcmp(option, "-f") == 0)
		g_force_write = TRUE;
	else if (strcmp(option, "--default-config") == 0)
		set_default_config();
	else if (strcmp(option, "--version") == 0 || strcmp(option, "-v") == 0)
		fprintf(stderr, "unfit: version " UNFIT_VERSION "\n");
	else if (strcmp(option, "-o") == 0 && next != NULL && next[0] != '\0')
	{
		g_output = strdup(next);
		return 1;
	}
	else
		return -1;

	return 0;
}

static void
load_config_file(const char *filename)
{
	FILE *fp;
	char option[256];
	int line = 0;
	char *p;

	fp = fopen(filename, "r");
	if (!fp)
		return; /* No problem - doesn't need to exist */

	while (fgets(option, sizeof(option), fp) == option)
	{
		line++;

		if (option[0])
			option[strlen(option)-1] = '\0';

		if (option[0] == '#')
			continue;

		p = option;
		/* Find second word on line */
		while (*p != '\0')
		{
			if (*p == ' ')
			{
				*p = '\0';
				p++;
				/* Skip additional spaces */
				while (*p == ' ')
					p++;
				break;
			}
			p++;
		}

		if (parse_config_item(option, p, FALSE) == -1)
			fprintf(stderr, "Didn't understand line %d in file %s [%s]\n", line, filename, option);
	}
	fclose(fp);
}

static void
load_personal_config(void)
{
	struct passwd passwd;
	struct passwd *pw;
	const char *p;
	char s[384];
	char cfg_file[384];

	p = getenv("HOME");
	if (p == NULL)
	{
		if (getpwuid_r(getuid(), &passwd, s, sizeof(s), &pw) != 0)
			return;
		p = pw->pw_dir;
	}
	snprintf(cfg_file, sizeof(cfg_file), "%s/.unfit", p);
	load_config_file(cfg_file);
}

static void
load_server_config(void)
{
	load_config_file("/etc/unfit");
}

static void
print_column_headers(FILE *handle)
{
	char *columns[10] = { NULL };
	int i = 0;
	int j;

	if (g_timestamps)
		columns[i++] = "Time";

	if (g_heartrate)
		columns[i++] = "Heart-Rate";

	if (g_cadence)
		columns[i++] = "Cadence";

	if (g_speed)
		columns[i++] = "Speed";

	if (g_distance)
		columns[i++] = "Distance";

	if (g_temperature)
		columns[i++] = "Temperature";

	if (g_altitude)
		columns[i++] = "Altitude";

	for (j = 0; j < i-1 ; j++)
		fprintf(handle, "%s,", columns[j]);

	if (j != 0)
		fprintf(handle, "%s\n", columns[j]);
}

static void
print_row(FILE *handle, struct data_point *snap)
{
	char *columns[10] = { NULL };
	int i = 0;
	int j;
	char timestamp[128];
	char heart_rate[32];
	char cadence[32];
	char speed[32];
	char distance[32];
	char temperature[32];
	char altitude[32];

	if (g_timestamps)
		{
		get_timestamp(timestamp, sizeof(timestamp), snap->timestamp);
		columns[i++] = timestamp;
		}

	if (g_heartrate)
		{
		snprintf(heart_rate, sizeof(heart_rate), "%u", snap->heart_rate);
		columns[i++] = heart_rate;
		}

	if (g_cadence)
		{
		snprintf(cadence, sizeof(cadence), "%u", snap->cadence);
		columns[i++] = cadence;
		}

	if (g_speed)
		{
		snprintf(speed, sizeof(speed), "%0.3f", (float)(snap->speed)/1000);
		columns[i++] = speed;
		}

	if (g_distance)
		{
		snprintf(distance, sizeof(distance), "%0.3f", (float)(snap->distance)/1000);
		columns[i++] = distance;
		}

	if (g_temperature)
		{
		snprintf(temperature, sizeof(temperature), "%u", snap->temperature);
		columns[i++] = temperature;
		}

	if (g_altitude)
		{
		snprintf(altitude, sizeof(altitude), "%u", snap->altitude);
		columns[i++] = altitude;
		}

	for (j = 0; j < i-1 ; j++)
		fprintf(handle, "%s,", columns[j]);

	if (j != 0)
		fprintf(handle, "%s\n", columns[j]);
}

int
interpolate(int first, int second, int i, int count)
{
	int diff = (second - first) * 100;

	diff *= i;
	diff /= count;

	diff /= 100;
	
	return first + diff;
}

static void
dump_all_data(void)
{
	FILE *handle;
	struct data_point *snap;
	struct data_point *prev;
	struct data_point rec;
	int i;
	int diff;

	if (g_output == NULL || strcmp(g_output, "-") == 0)
		handle = stdout;
	else
	{
		if (g_force_write == FALSE)
		{
			if ((handle = fopen(g_output, "r")) != NULL)
			{
				fclose(handle);
				fprintf(stderr, "unfit ERROR: File exists [%s]\n", g_output);
				return;
			}
		}
		if ((handle = fopen(g_output, "w")) == NULL)
		{
			fprintf(stderr, "unfit ERROR: Can't open file [%s]\n", g_output);
			return;
		}
	}

	print_column_headers(handle);

	prev = NULL;
	for (snap = g_data.first; snap; snap = snap->next)
	{
		if (g_add_missing && prev && snap->timestamp - prev->timestamp > 1)
		{
			diff = snap->timestamp - prev->timestamp;
			for (i = prev->timestamp + 1 ; i < snap->timestamp ; i++)
			{
				/* Generate intermediate data */
				rec.timestamp = i;
				rec.speed = interpolate(prev->speed, snap->speed, rec.timestamp - prev->timestamp, diff);
				rec.heart_rate = interpolate(prev->heart_rate, snap->heart_rate, rec.timestamp - prev->timestamp, diff);
				rec.cadence = interpolate(prev->cadence, snap->cadence, rec.timestamp - prev->timestamp, diff);
				rec.distance = interpolate(prev->distance, snap->distance, rec.timestamp - prev->timestamp, diff);
				rec.temperature = interpolate(prev->temperature, snap->temperature, rec.timestamp - prev->timestamp, diff);
				rec.altitude = interpolate(prev->altitude, snap->altitude, rec.timestamp - prev->timestamp, diff);
				print_row(handle, &rec);
			}
		}
		print_row(handle, snap);
		prev = snap;
	}

	if (handle != stdout)
		fclose(handle);
}

int
main(int argc, const char *argv[])
{
	int i;
	int rc;

	set_default_config();
	load_server_config();
	load_personal_config();

	for (i=1; i<argc; i++) {
		rc = parse_config_item(argv[i], argv[i+1], TRUE);
		if (rc == -1)
			decode_file(argv[i]);
		else if (rc > 0)
			i += rc;
	}
	dump_all_data();

	if (g_output)
		free(g_output);
	if (g_time_format)
		free(g_time_format);
	return 0;
}

