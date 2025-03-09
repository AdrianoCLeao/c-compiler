#include "../../include/assembly/code_emission.h"
#include "../../include/assembly/assembly.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #define OUTPUT_EXTENSION ".exe"
    #define PATH_SEPARATOR '\\'
#else
    #define OUTPUT_EXTENSION ".out"
    #define PATH_SEPARATOR '/'
#endif

static char *get_output_binary_path(const char *source_file) {
    char *source_copy = strdup(source_file);
    if (!source_copy) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    char *filename = strrchr(source_copy, PATH_SEPARATOR);
    filename = filename ? filename + 1 : (char *)source_file;

    char *dot = strrchr(filename, '.');
    if (dot && strcmp(dot, ".c") == 0) {
        *dot = '\0';
    }

    size_t path_size = strlen(filename) + strlen(OUTPUT_EXTENSION) + 1;
    char *output_path = (char *)malloc(path_size);
    if (!output_path) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(source_copy);
        exit(1);
    }

    snprintf(output_path, path_size, "%s%s", filename, OUTPUT_EXTENSION);

    free(source_copy);
    return output_path;
}

void emit_code(const char *source_file) {
    char *assembly_path = get_output_assembly_path(source_file);
    char *binary_path = get_output_binary_path(source_file);

    printf("Compiling assembly: %s\n", assembly_path);

#ifdef _WIN32
    char command[512];
    snprintf(command, sizeof(command), "gcc -m64 -no-pie %s -o %s", assembly_path, binary_path);
#else
    char command[512];
    snprintf(command, sizeof(command), "as -o %s.o %s && ld %s.o -o %s", binary_path, assembly_path, binary_path, binary_path);
#endif

    if (system(command) != 0) {
        fprintf(stderr, "Error: Compilation failed.\n");
        free(assembly_path);
        free(binary_path);
        return;
    }

    printf("Executable created: %s\n", binary_path);

    free(assembly_path);
    free(binary_path);
}
