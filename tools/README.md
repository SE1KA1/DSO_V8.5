# 源码注释工具

为 `User/` 下 `.c`/`.h` 批量注入 `@file` / `@brief` 文件头。

## 用法

```bash
node tools/add_source_annotations.js          # 写入缺少 @file 的文件
node tools/add_source_annotations.js --dry-run  # 仅统计
```

## 配置

- `file_annotations.json` — 核心模块的精确 `@brief` 文案（相对 `User/` 路径为键）
- 未在 JSON 中列出的 BSP 通用驱动会自动生成「安富莱 BSP …；DSO 主链路未启用」类说明

## 刷新文件索引（交接文档附录）

```bash
node tools/generate_file_index.js
```

生成为 `docs/FILE_INDEX.generated.md`。
