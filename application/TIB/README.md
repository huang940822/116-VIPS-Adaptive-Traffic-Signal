# Traffic Information Broadcaster (TIB)

Broadcast SPaT & MAP packets.<br>
Create two threads for regular transferring SPaT and MAP packets respectively.

## MAP Packet
在一開始讀 config 內容並在每次 plan 轉換更新 ConnectTo lane。
## SPaT Packet
會在 step 轉換的時候更新。

## TIB config 設定
總共有三個 table 要設定 `LaneSet_table`、`LaneSet_connectsTo_table` 跟 `SignalGroupID_table`。詳細的填寫方式在 `config_example` 中。 `config_example` 描述高發二路，`config_example_2` 描述平實四街與後甲三街口。

### LaneSet_table
`LaneSet_table` 在描述車道的地理資訊，有方向車道的 GPS 點位，其中 `ApproachId` 也有分群概念，把那些條車道分成同一個方向
### LaneSet_connectsTo_table
`LaneSet_connectsTo_table` 在描述 ingress 的車道可以連接到哪一條 egress 的車道
### SignalGroupID_table
`SignalGroupID_table` 在描述路口總共有多少個綠燈，如果都是一般的三燈的紅綠燈就只會有一個

### 繪製地圖
可以使用 Google 我的地圖繪製車道點位，在用匯出資料下載 CSV 檔的方式填寫。
![messageImage_1707196534127](https://hackmd.io/_uploads/rkt5lSyj6.jpg)