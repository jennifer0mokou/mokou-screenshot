# AI 接口

设置→AI 选择 OpenAI 兼容服务。本地 OCR 由 [独立 worker](OCR.md) 提供。

## 连接与提示词

| 设置 | 用途 |
| --- | --- |
| API Base URL | 有效 HTTP／HTTPS 地址，自动补 /chat/completions |
| 视觉模型 | 识图与视觉 OCR |
| 翻译模型 | 同一 API 下的文本模型；留空使用视觉模型 |
| API Key | 非空时加入 Bearer Authorization，当前进程内存保存 |
| 译文／回答语言 | 翻译目标及识图回答语言 |
| 系统提示词 | 非空时作为 system 消息 |
| OCR／翻译／识图提示词 | 各任务指令，与固定的输出协议一起发送 |

普通偏好与提示词持久化，Key 不序列化。每项提示词最多 16000 字符。Base URL 不含内嵌账号、查询参数或 fragment；服务需支持标准 Chat Completions 外壳。

## 翻译

RecognitionController 复用当前 OCR 结果；没有结果时先执行所选 OCR。请求只含文本：

```json
{
  "model":"translation-model",
  "stream":false,
  "messages":[{"role":"user","content":"翻译说明和协议，后接 [{\"id\":0,\"text\":\"original line\"}]"}]
}
```

读取 choices[0].message.content 中的 JSON 字符串：

```json
{"translations":[{"id":0,"text":"译文"}]}
```

每个 ID 完整出现一次。程序按 ID 合并译文，保留原文和坐标。配置校验同时考虑翻译模型及所需 OCR 的视觉模型。

## 识图与视觉 OCR

只发送当前选区和已绘制标注：

```json
{
  "model":"vision-model",
  "stream":false,
  "messages":[{"role":"user","content":[
    {"type":"text","text":"任务与返回格式"},
    {"type":"image_url","image_url":{"url":"data:image/png;base64,..."}}
  ]}]
}
```

识图读取普通文字，在截图内面板显示。视觉 OCR 读取 JSON：

```json
{"coordinate_system":"normalized","box_format":"xywh","blocks":[{"text":"original line","bbox":[0.08,0.20,0.72,0.10]}]}
```

| 字段 | 值 |
| --- | --- |
| coordinate_system | normalized（默认）、pixels、percent、1000 |
| box_format | xywh（默认）或 xyxy |
| bbox | 四个数，或含 x／y／width／height 的对象 |

像素位置按本次图片真实尺寸转换。轻微边缘误差裁切修正；没有可靠位置的文字保留为选区旁的可选结果，其他块照常显示。单位不明确时保留原文，不猜测定位。

## 传输与校验

兼容普通本地服务，不强制 JSON mode 或模型专用参数。接受 Markdown JSON 围栏。翻译 ID、文字结构、坐标、大小与数量由 textresult 校验。

传输超时 120 秒、总请求上限 180 秒，响应 16 MiB、文字块 2000 个。取消与析构都会终止请求，旧结果不进入新会话。编辑改变内容或选区时，RecognitionController 取消对应请求。

格式依据 [Chat Completions](https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/create)。位置和 ID 是本应用协议，原始图像／模型正文不写日志。
