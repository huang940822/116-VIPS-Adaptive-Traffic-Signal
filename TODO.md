### log 更新
- [ ] 用原本的 log_file_write(log_content); 在長度過長時會有機會在會後沒有補上 '\0' 導致在寫 log 發生問題發生要改成 log_file_write("%s", log_content); 或是其他方式。

### Heartbear 更新
- [ ] 探討 oslink 裡面的 heartbeat 傳送要跟一般的接收封包轉傳的 socket 需要不一樣
- [ ] 修改為甚麼是用 Heartbeat_PORT 來判斷是否是 heartbeat 在 net_UDP_accept() 下
- [ ] 探討是否都使用 Is_Heartbeat 判斷

### traffic_signal_status_t 同步問題
- [ ] 因為 traffic_signal_status_t 這個結構包含了很多 TC 的封包資訊，來自不同的封包的資訊包含不同的資訊，如果 planId 更換了，第一時間其他的資訊可能還沒更新，像是時置計畫內容，所以就會讀到錯誤的內容。