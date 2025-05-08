#!/bin/bash
echo "Building server..."
# clang ws_server.c -o server \
clang test_server.c -o test_server \
$(pkg-config --cflags --libs libwebsockets) \
-I/opt/homebrew/opt/openssl@3/include \
-L/opt/homebrew/opt/openssl@3/lib \
-lssl -lcrypto

echo "Build Finished!"