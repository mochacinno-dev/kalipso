/*
 * Kalipso Language - A minimal compiled language simpler than C
 * 
 * Features:
 * - Only one type: int (64-bit)
 * - Simple syntax: no semicolons, no types in declarations
 * - Built-in print and input functions
 * - Direct compilation to C (transpiler)
 * 
 * Example Kalipso program (.kpso):
 *   x = 5
 *   y = 10
 *   result = x + y
 *   print result
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_VARS 256
#define MAX_LINE 1024
#define MAX_TOKENS 100
#define MAX_LINES 10000

typedef struct {
    char name[64];
    int used;
} Variable;

Variable vars[MAX_VARS];
int var_count = 0;
char *output_lines[MAX_LINES];
int output_count = 0;

void error(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}

int find_or_add_var(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) return i;
    }
    if (var_count >= MAX_VARS) error("Too many variables");
    strcpy(vars[var_count].name, name);
    vars[var_count].used = 1;
    return var_count++;
}

int is_identifier(const char *s) {
    if (!isalpha(s[0]) && s[0] != '_') return 0;
    for (int i = 1; s[i]; i++) {
        if (!isalnum(s[i]) && s[i] != '_') return 0;
    }
    return 1;
}

void tokenize(char *line, char tokens[][64], int *count) {
    *count = 0;
    char *p = line;
    while (*p && *count < MAX_TOKENS) {
        while (isspace(*p)) p++;
        if (!*p) break;
        
        int i = 0;
        if (isalnum(*p) || *p == '_') {
            while ((isalnum(*p) || *p == '_') && i < 63) {
                tokens[*count][i++] = *p++;
            }
        } else {
            tokens[*count][i++] = *p++;
        }
        tokens[*count][i] = '\0';
        (*count)++;
    }
}

void add_output_line(const char *line) {
    if (output_count >= MAX_LINES) error("Too many lines");
    output_lines[output_count] = strdup(line);
    output_count++;
}

void compile_line(char *line) {
    if (strlen(line) == 0) return;
    
    char tokens[MAX_TOKENS][64];
    int token_count;
    tokenize(line, tokens, &token_count);
    
    if (token_count == 0) return;
    
    char output[MAX_LINE];
    
    // Handle print statement
    if (strcmp(tokens[0], "print") == 0) {
        if (token_count < 2) error("print needs an argument");
        strcpy(output, "    printf(\"%lld\\n\", ");
        
        // Print all tokens after "print"
        for (int i = 1; i < token_count; i++) {
            if (is_identifier(tokens[i])) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "v%d", find_or_add_var(tokens[i]));
                strcat(output, tmp);
            } else {
                strcat(output, tokens[i]);
            }
            if (i < token_count - 1) strcat(output, " ");
        }
        strcat(output, ");");
        add_output_line(output);
        return;
    }
    
    // Handle input statement
    if (strcmp(tokens[0], "input") == 0) {
        if (token_count != 2) error("input needs one variable");
        if (!is_identifier(tokens[1])) error("input needs a variable name");
        int idx = find_or_add_var(tokens[1]);
        snprintf(output, sizeof(output), "    scanf(\"%%lld\", &v%d);", idx);
        add_output_line(output);
        return;
    }
    
    // Handle assignment: var = expression
    if (token_count >= 3 && strcmp(tokens[1], "=") == 0) {
        if (!is_identifier(tokens[0])) error("Left side must be a variable");
        int var_idx = find_or_add_var(tokens[0]);
        
        snprintf(output, sizeof(output), "    v%d = ", var_idx);
        for (int i = 2; i < token_count; i++) {
            if (is_identifier(tokens[i])) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "v%d", find_or_add_var(tokens[i]));
                strcat(output, tmp);
            } else {
                strcat(output, tokens[i]);
            }
            if (i < token_count - 1) strcat(output, " ");
        }
        strcat(output, ";");
        add_output_line(output);
        return;
    }
    
    error("Invalid statement");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input.kpso>\n", argv[0]);
        return 1;
    }
    
    FILE *in = fopen(argv[1], "r");
    if (!in) {
        perror("Failed to open input file");
        return 1;
    }
    
    // Extract base filename (without .kpso extension)
    char base_name[256];
    strcpy(base_name, argv[1]);
    char *dot = strrchr(base_name, '.');
    if (dot && strcmp(dot, ".kpso") == 0) {
        *dot = '\0';
    }
    
    // Compile each line first
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), in)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip comments and empty lines
        char *p = line;
        while (isspace(*p)) p++;
        if (*p == '#' || *p == '\0') continue;
        
        compile_line(p);
    }
    fclose(in);
    
    // Now write the output C file
    char output[256];
    snprintf(output, sizeof(output), "%s.c", base_name);
    FILE *out = fopen(output, "w");
    if (!out) {
        perror("Failed to create output file");
        return 1;
    }
    
    // Write C code
    fprintf(out, "#include <stdio.h>\n\n");
    fprintf(out, "int main() {\n");
    
    // Declare variables
    if (var_count > 0) {
        fprintf(out, "    long long ");
        for (int i = 0; i < var_count; i++) {
            fprintf(out, "v%d = 0", i);
            if (i < var_count - 1) fprintf(out, ", ");
        }
        fprintf(out, ";\n");
    }
    
    // Write all compiled lines
    for (int i = 0; i < output_count; i++) {
        fprintf(out, "%s\n", output_lines[i]);
        free(output_lines[i]);
    }
    
    fprintf(out, "    return 0;\n");
    fprintf(out, "}\n");
    
    fclose(out);
    
    printf("Generated C code: %s\n", output);
    printf("Compiling...\n");
    
    // Compile the C file to executable
    char exe_name[256];
    char compile_cmd[512];
    
#ifdef _WIN32
    snprintf(exe_name, sizeof(exe_name), "%s.exe", base_name);
    snprintf(compile_cmd, sizeof(compile_cmd), "gcc %s -o %s", output, exe_name);
#else
    snprintf(exe_name, sizeof(exe_name), "%s", base_name);
    snprintf(compile_cmd, sizeof(compile_cmd), "gcc %s -o %s", output, exe_name);
#endif
    
    int result = system(compile_cmd);
    if (result != 0) {
        fprintf(stderr, "Compilation failed!\n");
        return 1;
    }
    
    printf("Success! Created executable: %s\n", exe_name);
    printf("Run with: ./%s\n", exe_name);
    
    return 0;
}