#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "arena.h"

bool test_is_directory(char *path, struct arena scratch);
bool test_exists(char *path, struct arena scratch);
