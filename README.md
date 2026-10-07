# CAD Design Project 2 – Clock Buffer Insertion (`cbi`)

學號:B11215061

## 環境
Ubuntu 24.04、GCC、C++17,只使用標準函式庫,不需要網路。

## 編譯
```sh
mkdir -p build && cd build
cmake ..
make
```
產生的執行檔為 `build/cbi`。

## 執行
```sh
./build/cbi INPUT_FILE OUTPUT_FILE
# 範例
./build/cbi inputs/input001.cbi results/output001.cbi
./build/cbi ../abc123/input001.cbi ../def456/output001.cbi   # 支援相對路徑
```
* 參數少於兩個時,程式會印出錯誤訊息並以代碼 1 結束。
* 輸出資料夾不存在時會自動建立。
* 標準輸出格式:`T_max: <整數>, T_min: <整數>, Score: <整數>`
* 設定 `CBI_VERBOSE=1` 後執行,會把各階段的 cost / skew / score 印到 **stderr**(供報告使用):
  ```sh
  CBI_VERBOSE=1 ./build/cbi inputs/input001.cbi results/output001.cbi
  ```
* 測資檔名為三位數編號(`input001.cbi`),輸出檔名需對應(`output001.cbi`)。

## 原始碼結構
| 檔案 | 用途 |
|---|---|
| `src/types.h` | 資料結構(節點編號:0 = SRC,1..n = sink,n+1.. = buffer) |
| `src/parser.*` | 讀取 `.cbi` 輸入檔 |
| `src/evaluator.*` | 合法性檢查,計算 Tmax / Tmin / Score |
| `src/solver.*` | 初始合法解與 Score 優化 |
| `src/writer.*` | 輸出 `.buffer` / `.level` 區段 |
| `src/main.cpp` | 命令列入口 |

## 演算法摘要
1. **初始合法解**:候選方案包含「SRC 直接連所有 sink」,以及「由下往上的二分法分群」(嘗試多種群組大小上限)。每群放置一個最便宜且可行的 buffer,位置取群內座標中位數,並調整到未被佔用的整數座標;若 SRC 的長度限制不足,則插入中繼 buffer。另有「最近鄰分群」候選(小/中型輸入,或二分法找不到合法解時使用)。若某層完全無法合併(例如 buffer 可驅動距離極短),則在每個節點前插入一個往 SRC 方向前進的中繼 buffer,讓節點逐步靠攏後再合併。從合法候選中選出 Score 最低者。
2. **貪婪局部搜尋**(時間預算的前 25%):移除 buffer、移動 buffer。
3. **增量式模擬退火**(至預算的 92%):隨機移動包含「移動 buffer」「把節點重新分配給別的父節點」「交換兩個 sink 的父節點」「在同層節點上方插入 buffer」「移除 buffer」。狀態直接在原地修改,只更新受影響的父節點(子節點數、總長度、型別)與受影響的子樹(到達時間);所有 sink 的到達時間放在 multiset 中,Tmax / Tmin 為 O(1);被拒絕的改動由日誌還原,因此每次改動的成本遠小於重算整棵樹。溫度依實際經過時間指數下降,起始溫度由隨機試探的 |ΔScore| 平均值決定。
4. **最終貪婪微調**,使用剩餘時間。

**保持合法**:每個候選改動都會先為每個 buffer 重選最便宜的可行型別(型別只影響 fanout / 長度上限與成本,所以降級與必要時的升級都在這一步自動完成),通過檢查才可能被接受;最後的解再用完整檢查器 `evaluate()` 驗證一次。

**除錯模式**:設定 `CBI_DEBUG=1` 時,退火過程會定期把增量狀態轉成完整的樹,用完整檢查器核對 Score,不一致就中止(用來驗證增量計算的正確性)。

**時間預算**:預設 40 秒(限制為 60 秒),以實際經過時間計算。可用環境變數 `CBI_TIME=秒數` 覆寫,方便實驗。小測資會因迭代次數上限提早結束。

