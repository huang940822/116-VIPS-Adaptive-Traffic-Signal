# Warning Algorithm (WA) 交接文件

## Config 設定
```txt
WA_WARNING_RANGE 150 # unit: m; 需要警示的範圍設定 e.g. 衝突組合兩輛車皆在距路口 150 m 範圍內則判斷有碰撞風險

WA_WARNING_FREQ 0.5 # unit: s; 間隔多久做一次警示訊息等級之決策

COLLECTOR_TO_ARTERIAL_WARNING_RANGE 50 # unit: m; 在支道對幹道版本下的警示範圍，若衝突組合車輛皆在 50 m 內，則支道禮讓幹道

COLLECTOR_TO_ARTERIAL 1 # 0: Collector vs Collector, 1: Collector vs Arterial; 設定當前路口為支道對支道模式/支道對幹道模式

MAIN_DIRECTION 0 # 0: N-S, 1: E-W; 設定幹道之方位

INTERSECTION_CENTER_LAT 22.9956183 # 路口中心點經度

INTERSECTION_CENTER_LON 120.2369896 # 路口中心點緯度
```

## 演算法介紹
本演算法主要是透過行駛在各方向道路上第一台車輛距離路口的距離以及行駛速度，來判斷競相車輛是否會有碰撞風險，若是有則依據碰撞風險等級，顯示對應的警示訊息。  

具體細節請參閱: [警示演算法簡報](https://docs.google.com/presentation/d/1SF05yyeVqvOhMrucHTR0DNpITzKlsx7Y/edit?slide=id.p1#slide=id.p1)

## 演算法細節

### 第一部分：找出 Leading Vehicle
實現一個名為 `WA_on_camera_packet_rx` 的 callback function，並註冊於 `.on_camera_packet_rx` 類型的 callback，每當 middleware 接收到 camera 提供的資料，則會觸發這個 callback function。其處理流程如下圖所示：  
<img src="https://github.com/user-attachments/assets/ef73147f-fe14-4d47-9927-b4da23765bb4" width="30%">

> [!NOTE]
> CCI 是舊稱，現為 wa_vehicle_t

找出每個方向的第一輛車後，會將之存放在 `leading_vehicles` 中，用以做後續第二部分的邏輯判斷。

以下為 leading_vehicles 的結構
```c
/* in WA.h */
typedef struct {
    double speed;
    double distance;
} wa_vehicle_t;
```

### 第二部分：判斷各方向警示等級

這部分是以一個 timer thread 來實現，以 2Hz 的頻率來以 `leading_vehicles` 中記錄的資料判斷警示等級。

> 註：判斷警示等級的流程會因版本（支道對支道 / 支道對幹道）不同而有所區別，以下分別說明：

#### 支道對支道模式

演算法流程圖如下：
<img src="https://github.com/user-attachments/assets/59dddd5b-2a67-4979-b450-6695553adf06" width="30%">


說明：

- 檢查每個方向的 leading vehicle 是否在 150 公尺碰撞區域內（可在 config 設定）。
- 若有車輛在範圍內，則以距離與速度計算 `TTI`（Time to Intersection），並設置 `vehicle_in_range = true`。
- 若有任一台車輛在碰撞區域中，繼續計算可能碰撞組合的 PET（Post-Encroachment Time）。

可能產生碰撞的方向組合如下圖：
(0: 北, 1: 東, 2:南, 3: 西)

<img src="https://github.com/user-attachments/assets/5d868f02-213f-4692-b96d-aac31dee6e7c" width="30%">


依據 PET 值進一步決定警示等級：

- 所有 PET > 3：顯示 lv1 警示。
- 恰好一組 PET < 3：先到者 lv2，後到者 lv3。
- 多組 PET < 3：所有方向皆為 lv3。

#### 支道對幹道模式

流程圖如下：
<img src="https://github.com/user-attachments/assets/481b6777-035a-411f-aae8-3228ae6d145f" width="30%">



與支道對支道版本不同之處：

- 若兩車皆在 50 公尺範圍內（可在 config 設定），則：
  - 幹道方向顯示 lv2 警示。
  - 支道方向顯示 lv3 警示。
- 若任一車輛不在 50 公尺內，則退回使用先到後到邏輯決定警示等級（與支道對支道相同）。

## 結語

本演算法以簡潔的規則有效判斷路口潛在碰撞風險，並結合多種情境與配置參數，能靈活應對多樣化的交通場景。若需進一步擴展或調整，請根據 `WA_config` 與 `WA.c` 中的實作進行修改。
