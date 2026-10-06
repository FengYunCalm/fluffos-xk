---
layout: doc
title: filesystem / write_file
---
# write_file

### 名称

    write_file() - 将字符串写入到文件中

### 语法

    int write_file( string file, string str, int flag );

### 描述

    将字符串 `str` 追加到文件 `file` 中，写入成功返回 1，失败返回  0 。如果参数 `flag` 是 1，写入方式是覆盖而不是追加。

标志位 1 表示覆盖，标志位 2 表示 gzip 输出；`flag` 为 3 时覆盖 gzip 文件。空字符串是合法输入。

只有完整写入且流关闭成功才返回 1。写入、刷新或关闭失败返回 0；打开失败仍按原合同抛出运行时错误。
失败时目标文件可能已经改变，`write_file` 不保证原子替换。

### 参考

    read_file(3), write_buffer(3), file_size(3)

### 翻译

    雪风(i@mud.ren)
