#include "../../include/assembly/code_emission.h"
#include "../../include/assembly/assembly.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#else
#include <process.h>
#include <io.h>
#endif

#ifdef _WIN32
    #define OUTPUT_EXTENSION ".exe"
    #define PATH_SEPARATOR '\\'
#else
    #define OUTPUT_EXTENSION ".out"
    #define PATH_SEPARATOR '/'
#endif

char *get_output_binary_path(const char *source_file) {
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

static int run_cc_with_args(char *const argv[]) {
#ifndef _WIN32
    pid_t pid;
    int rc = posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ);
    if (rc != 0) {
        fprintf(stderr, "Error: posix_spawnp failed (rc=%d) for %s\n", rc, argv[0]);
        return rc;
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 1;
#else
    // On Windows, fall back to _spawnvp
    int rc = _spawnvp(_P_WAIT, argv[0], (const char * const*)argv);
    if (rc == -1) {
        perror("_spawnvp");
        return 1;
    }
    return rc;
#endif
}

int emit_code(const char *source_file) {
    char *assembly_path = get_output_assembly_path(source_file);
    char *binary_path = get_output_binary_path(source_file);

    printf("Compiling assembly file with cc: %s\n", assembly_path);

#if defined(__linux__)
    char *argv[] = { "cc", "-m64", "-no-pie", assembly_path, "-o", binary_path, NULL };
#elif defined(_WIN32)
    char *argv[] = { "gcc", assembly_path, "-o", binary_path, NULL };
#elif defined(__APPLE__)
    // macOS: target x86_64 since our backend emits x86_64 AT&T asm
    char *argv[] = { "cc", "-arch", "x86_64", assembly_path, "-o", binary_path, NULL };
#else
    // Other Unix
    char *argv[] = { "cc", assembly_path, "-o", binary_path, NULL };
#endif

    int rc = run_cc_with_args(argv);
    if (rc != 0) {
        fprintf(stderr, "Error: cc failed (rc=%d).\n", rc);
    } else {
        printf("Executable created: %s\n", binary_path);
    }

    free(assembly_path);
    free(binary_path);
    return rc;
}

int emit_executable_via_cc_pipe(AssemblyProgram *program, const char *source_file) {
    if (!program) {
        fprintf(stderr, "Error: No assembly program to emit.\n");
        return 1;
    }

    char *binary_path = get_output_binary_path(source_file);

#ifndef _WIN32
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        perror("pipe");
        free(binary_path);
        return 1;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    // Child: stdin from pipe read end
    posix_spawn_file_actions_adddup2(&actions, pipefd[0], 0);
    posix_spawn_file_actions_addclose(&actions, pipefd[1]);

#if defined(__linux__)
    char *argv[] = { "cc", "-m64", "-no-pie", "-x", "assembler", "-", "-o", binary_path, NULL };
#elif defined(__APPLE__)
    char *argv[] = { "cc", "-arch", "x86_64", "-x", "assembler", "-", "-o", binary_path, NULL };
    fprintf(stdout, "[cc] piping assembly: cc -arch x86_64 -x assembler - -o %s\n", binary_path);
#else
    char *argv[] = { "cc", "-x", "assembler", "-", "-o", binary_path, NULL };
#endif

    pid_t pid;
    int rc = posix_spawnp(&pid, argv[0], &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (rc != 0) {
        fprintf(stderr, "Error: posix_spawnp failed (rc=%d) for %s\n", rc, argv[0]);
        close(pipefd[0]);
        close(pipefd[1]);
        free(binary_path);
        return 1;
    }

    close(pipefd[0]);
    FILE *w = fdopen(pipefd[1], "w");
    if (!w) {
        perror("fdopen");
        close(pipefd[1]);
        // Ensure child exits due to EOF on stdin
        int status = 0; waitpid(pid, &status, 0);
        free(binary_path);
        return 1;
    }
    write_assembly_to_stream(program, w);
    fclose(w);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        free(binary_path);
        return 1;
    }
    if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
        fprintf(stderr, "Error: cc failed while assembling from pipe.\n");
        free(binary_path);
        return 1;
    }
    printf("Executable created: %s\n", binary_path);
    free(binary_path);
    return 0;
#else
    // Windows: fall back to piping via _popen to gcc
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "gcc -x assembler - -o \"%s\"", binary_path);
    FILE *p = _popen(cmd, "w");
    if (!p) {
        perror("_popen");
        free(binary_path);
        return 1;
    }
    write_assembly_to_stream(program, p);
    int rc = _pclose(p);
    if (rc != 0) {
        fprintf(stderr, "Error: gcc failed (rc=%d).\n", rc);
        free(binary_path);
        return rc;
    }
    printf("Executable created: %s\n", binary_path);
    free(binary_path);
    return 0;
#endif
}

int run_executable_and_print_exit(const char *binary_path) {
#ifndef _WIN32
    pid_t pid;
    const char *prog = binary_path;
    char stack_buf[PATH_MAX];
    if (strchr(binary_path, '/') == NULL) {
        snprintf(stack_buf, sizeof(stack_buf), "./%s", binary_path);
        prog = stack_buf;
    }
    char *argv[] = { (char *)prog, NULL };
    int rc = posix_spawnp(&pid, prog, NULL, NULL, argv, environ);
    if (rc != 0) {
        fprintf(stderr, "Error: failed to spawn %s (rc=%d)\n", binary_path, rc);
        return 127;
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 127;
    }
    int exit_code = 0;
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
        printf("Program exited with code %d\n", exit_code);
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        printf("Program terminated by signal %d\n", sig);
        exit_code = 128 + sig;
    } else {
        printf("Program ended abnormally.\n");
        exit_code = 127;
    }
    return exit_code;
#else
    const char *argv[] = { binary_path, NULL };
    int rc = _spawnvp(_P_WAIT, binary_path, argv);
    if (rc == -1) {
        perror("_spawnvp");
        return 127;
    }
    printf("Program exited with code %d\n", rc);
    return rc;
#endif
}
