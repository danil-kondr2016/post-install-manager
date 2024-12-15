#pragma once

#include "arena.h"

uint32_t op_remove_file(char *file, struct arena scratch);
uint32_t op_remove_dir(char *file, struct arena scratch);
uint32_t op_copy_file(char *source, char *dest, struct arena scratch);
uint32_t op_copy_dir(char *source, char *dest, struct arena scratch);
uint32_t op_move(char *source, char *dest, struct arena scratch);
uint32_t op_rename(char *path, char *new_name, struct arena scratch);
uint32_t op_mkdir(char *path, struct arena scratch);
