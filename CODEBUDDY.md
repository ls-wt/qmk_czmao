# CZMAO DS17 QMK 固件项目背景（常驻上下文，由 CODEBUDDY.md 自动注入每次会话）

路径：`keyboards/czmao/ds17/`

## 子型号与 USB ID
- `r3`、`r6`：vid `0x4A17` / pid `0x4A18`（bootloader 写在 `keyboard.json`，值 `stm32duino`）
- `r6mmd`（keyboard_name `MMD_KM17`）：vid `0x4A79` / pid `0x4A79`
- `r6.1`：从 r6 复制出的新固件，计划 vid `0x4A17` / pid `0x4A19`、bootloader 改 `uf2`

## numlock 白灯爆闪修复
- 病根：keymap 用分片中间节点的 `rgb_matrix_indicators_advanced_user`，与每帧重写的动画抢 LED 0 → 高速闪烁。
- 修法：换成整帧渲染后只调一次的 basic `rgb_matrix_indicators_user`（numlock 最后写、覆盖任意层色），对任意动画都稳。
- 状态：`r3` / `r6` / `r6mmd` 已完成；`r6.1` 待套用（目前仍是老 `_advanced_user` 写法）。

## UF2 注意
- `bootloader: "uf2"` 的固件 flash 偏移与 stm32duino 不同，需先在芯片烧 QMK UF2 Bootloader，否则偏移错→跑飞→USB 不识别。

## VIA 定义服务器
- `via.micah.vip` 实际跑在 `47.103.64.134`（aaPanel/nginx）。
- 域名 `micah.vip` 的 DNS 解析到 `198.18.31.185`（保留/黑洞段），说明域名过期或 A 记录坏；服务器本身正常（直连 IP 能返回站点与 VIA 定义 JSON）。
- VIA vpid 公式：`vid<<16 | pid`（r6.1 = 1243040281，对应 `definitions/v3/1243040281.json`）。

## 备注
- 主 C 文件名不强制等于目录名（r6/r6mmd 主文件叫 `ds17.c`，QMK 按目录 glob `*.c` 编译）。
- 本仓库 ssh 访问 `47.103.64.134` 的凭证据此前提过已清理（cmdkey /delete），需要时重新提供。
