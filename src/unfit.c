
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>

#define FIT_MAGIC 0x2E464954

struct gheader
{
	unsigned char size;
	unsigned char protocol_version;
	unsigned short profile_version;
	unsigned int data_size;
	unsigned int fit;
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

struct msg_list defines = { NULL, NULL };

static void
add_def(struct msg_list *list, struct msg_def *entry)
{
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
	add_def(&defines, def);
	dump_defs(&defines);
	return p;
}

void
print_name_20(int id)
{
	if (id == 3) {
		printf("{heart-rate}");
	} else if (id == 6) {
		printf("{speed}");
	} else {
		printf("{%d}", id);
	}
}

void
print_name(int id)
{
	printf("{%d}", id);
}

static void
show_msg_20(const unsigned char *data, int type, int name, int size)
{
	unsigned short us;
	short ss;
	unsigned int u32;
	int s32;
	char *x;

	print_name_20(name);

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

struct hr
{
	char timestamp[32];
	int heart_rate;
	int speed;
	int cadence;
	int altitude;
	int distance;
	int temperature;
};

static void
get_timestamp(char *buf, int bufsz, unsigned int stamp)
{
	struct tm *tm;
	time_t tt;

	tt = stamp;
	tm = gmtime(&tt);

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
	struct hr *snap;

	snap = malloc(sizeof(struct hr));
	if (snap)
		memset(snap, 0, sizeof(struct hr));


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
				snap->distance = value;
			else if (name == 6)
				snap->speed = value;
			else if (name == 13)
				snap->temperature = value;
			else if (name == 253)
				get_timestamp(snap->timestamp, sizeof(snap->timestamp), value);
			else
				printf("UNK(%d,%d,%d) ", name, size, type);
		}
	}
	printf("\n");
    /* Distance converted to metres */
	printf("DATA: %s,%d,%d,%d,%d,%d,%d\n", snap->timestamp, snap->heart_rate, snap->cadence, snap->speed, snap->altitude, snap->distance/100, snap->temperature);
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
		if (def->num == 20) {
			show_msg_20(data, def->fields[i] & 0xFF, name, size);
		} else {
			show_data(data, def->fields[i] & 0xFF, name, size);
		}

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
		// Abnormal header
		printf("Abnormal header -- TODO\n");
	} else {
		// Normal header
		if ((p[0] & 0x40) == 0x40) {
			printf("Definition\n");
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
			printf("DATA Message (type = %d)\n", msg_type);
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
		printf("\nNEXT RECORD @ %08X (%d)\n\n", p-data, p-data);
		if ((p = parse_record(p)) == NULL) {
			printf("ERROR\n");
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

int
main(int argc, const char *argv[])
{
	int i;

	for (i=1; i<argc; i++) {
		decode_file(argv[i]);
	}
	return 0;
}
