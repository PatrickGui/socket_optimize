CC = gcc
CFLAGS = -Wall -O2

SRC_DIR = src
BIN_DIR = bin

UNIX_SERVER = $(BIN_DIR)/unix_server
UNIX_CLIENT = $(BIN_DIR)/unix_client
NETLINK_SERVER = $(BIN_DIR)/netlink_server
NETLINK_CLIENT = $(BIN_DIR)/netlink_client

UNIX_SERVER_SRC = $(SRC_DIR)/unix_server.c
UNIX_CLIENT_SRC = $(SRC_DIR)/unix_client.c
NETLINK_SERVER_SRC = $(SRC_DIR)/netlink_server.c
NETLINK_CLIENT_SRC = $(SRC_DIR)/netlink_client.c

all: $(UNIX_SERVER) $(UNIX_CLIENT) $(NETLINK_SERVER) $(NETLINK_CLIENT)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(UNIX_SERVER): $(UNIX_SERVER_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(UNIX_CLIENT): $(UNIX_CLIENT_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(NETLINK_SERVER): $(NETLINK_SERVER_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(NETLINK_CLIENT): $(NETLINK_CLIENT_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean