# Makefile for compiling server.c

CC = gcc                   # 編譯器
CFLAGS = -Wall              # 顯示所有警告訊息
LIBS = -lwebsockets         # 需要連結的函式庫
TARGET = test_server             # 編譯出來的執行檔名稱
SRC = test_server.c              # 原始碼檔案

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)