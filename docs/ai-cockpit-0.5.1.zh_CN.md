[English](ai-cockpit-0.5.1.md)

# AI 驾驶舱 0.5.1

社区更新合入个人版 0.4.8 的界面与同步改进，保留用户自行配置 NVS 接口的流程。
本构建不编入私人接口或密钥。按 README 先配置接口，再蓝牙配网；写入配置分区
会清空该分区的旧内容。

## 操作和变化

长按确定进入或退出 SETTINGS，上下选择 WI-FI、SYNC、BLE、LIGHT、SLEEP，
短按确定操作。LIGHT 依次为 10/20/35/50/70/100%，默认 35%。SLEEP 依次为
15/30/60/120/300 秒及 ALWAYS ON，默认 30 秒。设置保存，开机仍默认息屏；
常亮只表示唤醒后不自动息屏。息屏继续同步。荧光色更柔和、背景更深，
大数字新增立体明暗效果。信息页导航保持原操作。

同步数据与状态分开投递，刷新期间的重复按键合并，支持当前 DNS 地址切换及
短时 HTTPS 连接复用。保留证书和域名校验。连接与响应阶段缩短等待，但底层
阻塞操作仍可能超过配置超时，不能承诺瞬时同步或完全消除网络错误。
过期数据仍显示 CACHED。Mac 后端成功后每 300 秒查询，上游失败后 60 秒重试，
错误日志不包含账号正文或凭据。

## 验证范围

个人版 0.4.8 实测覆盖启动、获取 HTTPS 最新数据、重复按键合并、五行设置及
15 秒息屏。用户确认自动地址切换后显示 SYNCED。仍有间歇性 TLS 超时，
实际连接复用尚未在设备上确认。本公开版保留 NVS 配置，需要单独通过完整
主机、构建及归档校验；不宣称已刷入设备。发布图片沿用已检查的上版合成数据渲染。

## 构建结果

Build: PASS. Host tests: PASS. Archive verification: PASS.
Device tests: NOT RUN for this community artifact.

Full SHA-256: `fcd1bff693d7ee5d6fe8eb88ca1cce8cfe8d06e7974d77b8fcefdb354e6aa61d`

ELF SHA-256: `4bfacd8cae84597c08e7426acda74a5574344c4fa2f8130d6bcf088f41fd3dfb`
