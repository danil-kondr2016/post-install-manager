#pragma once

#include "arena.h"
#include "repo.h"

uint32_t execute_command_chain(union command *cmd, struct arena scratch);
uint32_t execute_command(union command *cmd, struct arena scratch);
