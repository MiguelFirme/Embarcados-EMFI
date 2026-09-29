#ifndef ENFI_PROTOCOL_H
#define ENFI_PROTOCOL_H

// Parses one complete ASCII command. Replies are newline-terminated over USB CDC.
void protocol_handle_line(char *line);

#endif
