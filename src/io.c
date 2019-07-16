#include <vitasdk.h>
#include <taihen.h>

#include "io.h"
#include "log.h"
#include "config.h"
#include "main.h"

int vg_io_find_eol(const char chunk[], int pos, int end) {
    for (int i = pos; i < end; i++) {
        if (chunk[i] == '\n') {
            return i;
        }
    }
    return -1;
}

const char *vg_io_status_code_to_string(vg_io_status_code_t code) {
    switch (code) {
        case IO_OK: return "All OK!";
        case IO_ERROR_LINE_TOO_LONG: return "Line too long.";
        case IO_ERROR_OPEN_FAILED: return "Failed to open the file.";
        case IO_ERROR_PARSE_INVALID_TOKEN: return "Invalid token.";
        case IO_ERROR_INTERPRETER_ERROR: return "Interpreter error.";
        default: return "?";
    }
}

vg_io_status_t vg_io_parse(const char *path, vg_io_status_t (*parse_line_fn)(const char line[])) {
    vg_io_status_t ret = {IO_OK, 0, 0};

    SceUID fd = sceIoOpen(path, SCE_O_RDONLY | SCE_O_CREAT, 0777);
    if (fd < 0) {
        ret.code = IO_ERROR_OPEN_FAILED;
        goto EXIT;
    }

    int chunk_size = 0;
    char chunk[IO_CHUNK_SIZE] = "";
    int pos = 0, end = 0;
    int line_counter = 0;

    while ((chunk_size = sceIoRead(fd, chunk, IO_CHUNK_SIZE)) > 1) {
#ifdef ENABLE_VERBOSE_LOGGING
        vg_log_printf("[IO] DEBUG: Parsing new chunk, size=%d\n", chunk_size);
#endif
        pos = 0;

        // Read all lines in chunk
        while (pos < chunk_size) {
            // Get next EOL
            end = vg_io_find_eol(chunk, pos, chunk_size);

            // Did not find EOL in current chunk & there is more to read?
            // Proceed by reading next sub-chunk
            if (end == -1 && chunk_size == IO_CHUNK_SIZE) {
#ifdef ENABLE_VERBOSE_LOGGING
                vg_log_printf("[IO] DEBUG: Didnt find EOL in this chunk, pos=%d, seek=%d\n",
                        pos, 0 - (IO_CHUNK_SIZE - pos));
#endif
                // Single line is > CONFIG_CHUNK_SIZE chars
                if (pos == 0) {
                    ret.code = IO_ERROR_LINE_TOO_LONG;
                    ret.line = line_counter;
                    goto EXIT_CLOSE;
                }

                // Seek back to where unfinished line started
                sceIoLseek(fd, 0 - (IO_CHUNK_SIZE - pos), SCE_SEEK_CUR);
                break;
            }
            // Found EOL, parse line
            else {
                line_counter++;

                // Last chunk, last line, no EOL?
                if (end == -1) {
#ifdef ENABLE_VERBOSE_LOGGING
                    vg_log_printf("[IO] WARN: Last line is missing EOL char!\n");
#endif
                    end = chunk_size;
                }

                // Ignore leading spaces and tabs
                while (pos < end && isspace(chunk[pos])) { pos++; }
                if (pos == end)
                    goto NEXT_LINE; // Empty line

                // Ignore comments
                if (chunk[pos] == '#')
                    goto NEXT_LINE;

                // Swap \n with \0
                chunk[end] = '\0';

                // Parse valid line
                ret = parse_line_fn(&chunk[pos]);
                if (ret.code != IO_OK) {
                    ret.line = line_counter;
                    goto EXIT_CLOSE;
                }

                // Swap back \n
                chunk[end] = '\n';

NEXT_LINE:
                pos = end + 1;
            }
        }

        // EOF
        if (chunk_size < IO_CHUNK_SIZE)
            break;
    }

EXIT_CLOSE:
    sceIoClose(fd);
EXIT:
    return ret;
}