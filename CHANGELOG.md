## v2.2
- 移除 send_cnt，避免overflow
- 修改 OBU_packet_rx_bsm 中的 memcpy 之 len
- 將 J2735 BSM 加入

## v2.3
111 案先行版
- 新增 VMS 功能
- 新增補償策略 增加 4 個補償策略與兩周期補償
- 更改 EVSP touching area 格式
- 新增 RSU 部屬包裝功能 make deploy

## v2.6.0
113案場middleware
最新更動

EVSP
1. 新增多車種封包識別 (救護車/消防車/警車)
2. 新增EVSP觸碰點多次觸發閾值機制
3. EVSP多車種觸發新增同相秒數聯集機制
4. 新增行人倒數關閉/開啟機制

CMS
1. 新增點燈回報功能
2. 新增EVSP多車種紀錄
