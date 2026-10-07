---
layout: doc
title: sockets / socket_get_option
---
# socket_get_option

### NAME

    socket_get_option - get socket option values

### SYNOPSIS

    mixed socket_get_option(int socket, int option);

### DESCRIPTION

    Retrieves the current value of a socket option. This efun allows you to
    query socket configuration parameters, particularly those related to
    TLS/SSL connections.

### ARGUMENTS

- `socket` - The socket descriptor returned by socket_create()
- `option` - The option constant to query (see Options below)

### OPTIONS

#### SO_TLS_VERIFY_PEER (1)

Returns whether TLS peer certificate verification is enabled.

- **Returns**: integer
  - `0` - Peer verification disabled
  - `1` - Peer verification enabled

#### SO_TLS_SNI_HOSTNAME (2)

Returns the Server Name Indication (SNI) hostname set for the socket.

- **Returns**: string or integer `0`
  - The configured SNI hostname, or `0` if not set
  - An explicitly set empty string remains an empty string
  - TLS client setup consumes this option after configuring SNI and peer identity;
    subsequent calls return `0`, not the active TLS session's hostname

#### SO_TLS_CERT (3) and SO_TLS_KEY (4)

Return the configured certificate or private-key path, respectively, or integer
`0` if not set. These options return paths, not file contents.

### RETURN VALUE

The return type depends on the option being queried:
- SO_TLS_VERIFY_PEER returns an integer (0 or 1)
- SO_TLS_SNI_HOSTNAME, SO_TLS_CERT and SO_TLS_KEY return a string or integer `0`
- Returned numbers are defined values: an unset option is not `undefinedp()`
- Returned strings remain valid after the option changes or the socket closes

### ERRORS

- Generates an error if the descriptor is negative or outside the allocated table
- Generates an error if the option is unknown
- A closed, cleared slot still inside the table returns `0` for each supported option

This getter is not a live-socket or ownership check and does not call `valid_socket`.
Use `socket_status` to inspect socket state.

### EXAMPLES

**Query socket TLS settings:**
```c
void check_socket_config(int sock) {
    int verify_peer;
    string sni_hostname;

    verify_peer = socket_get_option(sock, SO_TLS_VERIFY_PEER);
    sni_hostname = socket_get_option(sock, SO_TLS_SNI_HOSTNAME);

    write(sprintf("Socket %d TLS verification: %s\n",
                  sock, verify_peer ? "enabled" : "disabled"));
    write(sprintf("Socket %d SNI hostname: %s\n",
                  sock, sni_hostname));
}
```

**Verify socket configuration before connecting:**
```c
void safe_connect(string hostname, int port) {
    int sock;

    sock = socket_create(STREAM_TLS, "read_callback", "close_callback");
    socket_set_option(sock, SO_TLS_VERIFY_PEER, 1);
    socket_set_option(sock, SO_TLS_SNI_HOSTNAME, hostname);

    // Verify settings were applied
    if (socket_get_option(sock, SO_TLS_VERIFY_PEER) != 1) {
        write("Warning: TLS verification not enabled!\n");
        return;
    }

    if (socket_get_option(sock, SO_TLS_SNI_HOSTNAME) != hostname) {
        write("Warning: SNI hostname mismatch!\n");
        return;
    }

    socket_connect(sock, hostname + ":" + port, "connected_callback");
}
```

**Debug socket configuration:**
```c
void debug_socket(int sock) {
    mapping opts = ([]);

    opts["verify_peer"] = socket_get_option(sock, SO_TLS_VERIFY_PEER);
    opts["sni_hostname"] = socket_get_option(sock, SO_TLS_SNI_HOSTNAME);

    write("Socket configuration:\n" + dump_value(opts));
}
```

**Conditional behavior based on socket settings:**
```c
void handle_connection(int sock) {
    if (socket_get_option(sock, SO_TLS_VERIFY_PEER) == 0) {
        log_message("WARNING: Connecting without certificate verification");
        // Maybe add additional validation or logging
    }

    // Proceed with connection logic
    socket_connect(sock, "server.example.com:443", "on_connect");
}
```

### SEE ALSO

- [socket_set_option](socket_set_option.md) - Set socket options
- [socket_create](socket_create.md) - Create a socket
- [socket_status](socket_status.md) - Get socket status

### NOTES

**Option Constants:**
The option constants should be defined in your mudlib include files:
```c
#define SO_TLS_VERIFY_PEER    1
#define SO_TLS_SNI_HOSTNAME   2
#define SO_TLS_CERT           3
#define SO_TLS_KEY            4
```

**Default Values:**
- All options initially contain `0`
- Explicitly set SO_TLS_VERIFY_PEER to `1` before connecting when peer verification is required
- Unset string options return integer `0`, not an empty string

**Use Cases:**
- Debugging socket configuration
- Validating security settings before connection
- Conditional logic based on socket setup
- Logging and auditing connection parameters

### AVAILABILITY

Added in commit 1fd7f61 (2023). Requires the sockets package to be enabled.
