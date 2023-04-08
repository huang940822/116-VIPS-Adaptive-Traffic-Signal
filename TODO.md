### log 更新
- [ ] 用原本的 log_file_write(log_content); 在長度過長時會有機會在會後沒有補上 '\0' 導致在寫 log 發生問題發生要改成 log_file_write("%s", log_content); 或是其他方式。