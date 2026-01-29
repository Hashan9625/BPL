/**
 * @version 1.0
 * @Author: HASAN MADHUSHANKHA
 * @Date: 1/23/2026,
 * @Description osRun
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_LINE 1024
#define MAX_LIBS 32
#define MAX_ALIAS 64
#define MAX_VARS 64
#define MAX_ARGS 32

typedef int (*func_ptr)(int argc, char *argv[]);

typedef struct
{
    char alias[MAX_ALIAS];
    void *handle;
    char path[MAX_LINE];
} Library;

typedef struct
{
    char name[MAX_ALIAS];
    char value[MAX_LINE];
} Variable;

Library libraries[MAX_LIBS];
int lib_count = 0;

Variable variables[MAX_VARS];
int var_count = 0;

// Function prototypes
void trim(char *str);
char *read_line(FILE *fp, bool interactive);
void process_line(char *line);
void cmd_use(char *args);
void cmd_rem(char *args);
void cmd_call(char *args);
void cmd_set(char *args);
char *substitute_variables(char *arg);
Library *find_library(const char *alias);
int find_function_in_library(Library *lib, const char *func_name, func_ptr *func);

// Trim whitespace from string
void trim(char *str)
{
    if (!str)
        return;

    char *start = str;
    while (isspace((unsigned char)*start))
        start++;

    if (*start == 0)
    {
        *str = 0;
        return;
    }

    char *end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end))
        end--;

    size_t len = end - start + 1;
    memmove(str, start, len);
    str[len] = 0;
}

// Read a line with line continuation support
char *read_line(FILE *fp, bool interactive)
{
    static char buffer[MAX_LINE * 4];
    buffer[0] = 0;

    char line[MAX_LINE];
    bool first_line = true;

    while (1)
    {
        if (interactive && first_line)
        {
            printf("> ");
            fflush(stdout);
        }
        else if (interactive)
        {
            printf(">> ");
            fflush(stdout);
        }

        if (!fgets(line, sizeof(line), fp))
        {
            if (strlen(buffer) > 0)
            {
                return buffer;
            }
            return NULL;
        }

        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
        {
            line[len - 1] = 0;
            len--;
        }

        // Check for line continuation
        bool has_continuation = false;
        if (len > 0 && line[len - 1] == '\\')
        {
            line[len - 1] = 0;
            has_continuation = true;
        }

        strcat(buffer, line);

        if (!has_continuation)
        {
            break;
        }

        first_line = false;
    }

    return buffer;
}

// Find library by alias
Library *find_library(const char *alias)
{
    for (int i = 0; i < lib_count; i++)
    {
        if (strcmp(libraries[i].alias, alias) == 0)
        {
            return &libraries[i];
        }
    }
    return NULL;
}

// Find function in library
int find_function_in_library(Library *lib, const char *func_name, func_ptr *func)
{
    *func = (func_ptr)dlsym(lib->handle, func_name);
    if (*func == NULL)
    {
        return -1;
    }
    return 0;
}

// USE command: load a shared library
void cmd_use(char *args)
{
    char path[MAX_LINE], alias[MAX_ALIAS];

    // Parse: use <so_path> as <alias>
    char *as_ptr = strstr(args, " as ");
    if (!as_ptr)
    {
        fprintf(stderr, "Error: Invalid syntax. Expected: use <path> as <alias>\n");
        return;
    }

    *as_ptr = 0;
    strcpy(path, args);
    strcpy(alias, as_ptr + 4);

    trim(path);
    trim(alias);

    if (strlen(path) == 0 || strlen(alias) == 0)
    {
        fprintf(stderr, "Error: Path and alias cannot be empty\n");
        return;
    }

    // Check if alias already exists
    if (find_library(alias))
    {
        fprintf(stderr, "Error: Alias '%s' is already used\n", alias);
        return;
    }

    // Load library
    void *handle = dlopen(path, RTLD_LAZY);
    if (!handle)
    {
        fprintf(stderr, "Error: %s\n", dlerror());
        return;
    }

    // Check if it's a valid shared library by trying to get some symbols
    // If dlopen succeeded, it's likely a valid shared library

    if (lib_count >= MAX_LIBS)
    {
        fprintf(stderr, "Error: Maximum number of libraries reached\n");
        dlclose(handle);
        return;
    }

    // Add to library list
    strcpy(libraries[lib_count].alias, alias);
    strcpy(libraries[lib_count].path, path);
    libraries[lib_count].handle = handle;
    lib_count++;

    printf("Library '%s' loaded as '%s'\n", path, alias);
}

// REM command: unload a library
void cmd_rem(char *args)
{
    trim(args);

    Library *lib = find_library(args);
    if (!lib)
    {
        fprintf(stderr, "Error: The alias '%s' is not found from list of loaded libraries\n", args);
        return;
    }

    // Close library
    dlclose(lib->handle);

    // Remove from list by shifting
    int idx = lib - libraries;
    for (int i = idx; i < lib_count - 1; i++)
    {
        libraries[i] = libraries[i + 1];
    }
    lib_count--;

    printf("Library '%s' unloaded\n", args);
}

// Substitute variables in argument
char *substitute_variables(char *arg)
{
    static char result[MAX_LINE];

    if (arg[0] != '$')
    {
        return arg;
    }

    char var_name[MAX_ALIAS];
    strcpy(var_name, arg + 1);

    for (int i = 0; i < var_count; i++)
    {
        if (strcmp(variables[i].name, var_name) == 0)
        {
            strcpy(result, variables[i].value);
            return result;
        }
    }

    return arg;
}

// CALL command: call a function
void cmd_call(char *args)
{
    char func_spec[MAX_LINE];
    char *argv[MAX_ARGS];
    int argc = 0;

    trim(args);

    // Parse function name and arguments
    char *token = strtok(args, " \t");
    if (!token)
    {
        fprintf(stderr, "Error: Function name not specified\n");
        return;
    }

    strcpy(func_spec, token);

    // Parse arguments
    while ((token = strtok(NULL, " \t")) != NULL && argc < MAX_ARGS)
    {
        argv[argc++] = substitute_variables(token);
    }

    // Check if alias is specified
    char *dot_ptr = strchr(func_spec, '.');
    char alias[MAX_ALIAS] = "";
    char func_name[MAX_LINE];

    if (dot_ptr)
    {
        *dot_ptr = 0;
        strcpy(alias, func_spec);
        strcpy(func_name, dot_ptr + 1);
    }
    else
    {
        strcpy(func_name, func_spec);
    }

    func_ptr func = NULL;
    Library *found_lib = NULL;

    if (strlen(alias) > 0)
    {
        // Look in specific library
        found_lib = find_library(alias);
        if (!found_lib)
        {
            fprintf(stderr, "Error: The library (referred by alias '%s') is not loaded\n", alias);
            return;
        }

        if (find_function_in_library(found_lib, func_name, &func) != 0)
        {
            fprintf(stderr, "Error: The function '%s' is not found in library '%s'\n", func_name, alias);
            return;
        }
    }
    else
    {
        // Look in most recently loaded library that has this function
        for (int i = lib_count - 1; i >= 0; i--)
        {
            if (find_function_in_library(&libraries[i], func_name, &func) == 0)
            {
                found_lib = &libraries[i];
                break;
            }
        }

        if (!found_lib)
        {
            fprintf(stderr, "Error: The function '%s' is not found\n", func_name);
            return;
        }
    }

    // Call the function
    printf("Calling %s(%d args)...\n", func_name, argc);
    int result = func(argc, argv);

    if (result != 0)
    {
        fprintf(stderr, "Error: Function returned an error code (%d)\n", result);
    }
    else
    {
        printf("Function %s completed successfully\n", func_name);
    }
}

// SET command: set a variable
void cmd_set(char *args)
{
    trim(args);

    char *eq_ptr = strchr(args, '=');
    if (!eq_ptr)
    {
        fprintf(stderr, "Error: Invalid syntax. Expected: set <var_name>=<value>\n");
        return;
    }

    *eq_ptr = 0;
    char var_name[MAX_ALIAS];
    char var_value[MAX_LINE];

    strcpy(var_name, args);
    strcpy(var_value, eq_ptr + 1);

    trim(var_name);
    trim(var_value);

    // Check if variable exists, update it
    for (int i = 0; i < var_count; i++)
    {
        if (strcmp(variables[i].name, var_name) == 0)
        {
            strcpy(variables[i].value, var_value);
            printf("Variable '%s' updated to '%s'\n", var_name, var_value);
            return;
        }
    }

    // Add new variable
    if (var_count >= MAX_VARS)
    {
        fprintf(stderr, "Error: Maximum number of variables reached\n");
        return;
    }

    strcpy(variables[var_count].name, var_name);
    strcpy(variables[var_count].value, var_value);
    var_count++;

    printf("Variable '%s' set to '%s'\n", var_name, var_value);
}

// Process a single line
void process_line(char *line)
{
    if (!line || strlen(line) == 0)
    {
        return;
    }

    // Remove comments
    char *comment = strchr(line, '#');
    if (comment)
    {
        *comment = 0;
    }

    trim(line);

    if (strlen(line) == 0)
    {
        return;
    }

    // Parse command
    char command[32];
    char *space = strchr(line, ' ');

    if (space)
    {
        size_t cmd_len = space - line;
        if (cmd_len >= sizeof(command))
            cmd_len = sizeof(command) - 1;
        strncpy(command, line, cmd_len);
        command[cmd_len] = 0;
        space++;
    }
    else
    {
        strcpy(command, line);
        space = "";
    }

    if (strcmp(command, "use") == 0)
    {
        cmd_use(space);
    }
    else if (strcmp(command, "rem") == 0)
    {
        cmd_rem(space);
    }
    else if (strcmp(command, "call") == 0)
    {
        cmd_call(space);
    }
    else if (strcmp(command, "set") == 0)
    {
        cmd_set(space);
    }
    else if (strcmp(command, "quit") == 0)
    {
        exit(0);
    }
    else
    {
        fprintf(stderr, "Error: Unknown command '%s'\n", command);
    }
}

int main(int argc, char *argv[])
{
    bool interactive = false;
    FILE *fp = NULL;

    if (argc < 2)
    {
        // Interactive mode
        printf("SO Test Interpreter - Interactive Mode\n");
        printf("Commands: use, rem, call, set, quit\n");
        interactive = true;
        fp = stdin;
    }
    else
    {
        // Script mode
        fp = fopen(argv[1], "r");
        if (!fp)
        {
            fprintf(stderr, "Error: Cannot open script file '%s'\n", argv[1]);
            return 1;
        }
    }

    char *line;
    while ((line = read_line(fp, interactive)) != NULL)
    {
        process_line(line);
    }

    // Clean up
    for (int i = 0; i < lib_count; i++)
    {
        dlclose(libraries[i].handle);
    }

    if (fp != stdin)
    {
        fclose(fp);
    }

    return 0;
}