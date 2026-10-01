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

- 參數少於兩個時,程式會印出錯誤訊息並以代碼 1 結束。
- 輸出資料夾不存在時會自動建立。
- 標準輸出格式:`T_max: <整數>, T_min: <整數>, Score: <整數>`
- 設定 `CBI_VERBOSE=1` 後執行,會把各階段的 cost / skew / score 印到 **stderr**(供報告使用):
  ```sh
  CBI_VERBOSE=1 ./build/cbi inputs/input001.cbi results/output001.cbi
  ```
- 測資檔名為三位數編號(`input001.cbi`),輸出檔名需對應(`output001.cbi`)。

## 原始碼結構

| 檔案              | 用途                                                  |
| ----------------- | ----------------------------------------------------- |
| `src/types.h`     | 資料結構(節點編號:0 = SRC,1..n = sink,n+1.. = buffer) |
| `src/parser.*`    | 讀取 `.cbi` 輸入檔                                    |
| `src/evaluator.*` | 合法性檢查,計算 Tmax / Tmin / Score                   |
| `src/solver.*`    | 初始合法解與 Score 優化                               |
| `src/writer.*`    | 輸出 `.buffer` / `.level` 區段                        |
| `src/main.cpp`    | 命令列入口                                            |

## 演算法摘要

1. **初始合法解**:候選方案包含「SRC 直接連所有 sink」,以及「由下往上的二分法分群」(嘗試多種群組大小上限)。每群放置一個最便宜且可行的 buffer,位置取群內座標中位數,並調整到未被佔用的整數座標;若 SRC 的長度限制不足,則插入中繼 buffer。最後從合法候選中選出 Score 最低者。
2. **Score 優化(保持合法)**:依序進行「移除 buffer」與「移動 buffer」的爬山法。每個候選解都會重新挑選最便宜的可行 buffer 型別,並完整重新驗證合法性,合法且 Score 下降才接受。時間預算為 20 秒(限制為 60 秒)。
