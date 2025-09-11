#ifndef CODE_EMISSION_H
#define CODE_EMISSION_H

#include "assembly.h"


int emit_code(const char *source_file);

int emit_executable_via_cc_pipe(AssemblyProgram *program, const char *source_file);

char *get_output_binary_path(const char *source_file);

int run_executable_and_print_exit(const char *binary_path);

#endif 
