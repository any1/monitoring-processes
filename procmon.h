#pragma once

struct procmon_entry {
	struct procmon_entry* next;
	unsigned int lock_word;
};

int procmon_init(void);
void procmon_destroy(void);

void procmon_add(struct procmon_entry* entry);
void procmon_remove(struct procmon_entry* entry);

int procmon_is_alive(struct procmon_entry* entry);
