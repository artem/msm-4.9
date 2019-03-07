/*
 *  fs/partitions/msdos.h
 */

#define MSDOS_LABEL_MAGIC		0xAA55
#define MSDOS_LABEL_MAGIC_REVERSE    0x55AA

int msdos_partition(struct parsed_partitions *state);

