---
layout: doc
title: strings / rtrim
---
# rtrim

### 名称

    rtrim() - 移除字符串右侧的空白字符或其他预定义字符

### 语法

    string rtrim( string str );
    string rtrim( string str, string ch);

### 描述

    移除字符串 `str` 右侧的空格或 `ch` 中的其他字符，并返回一个新的字符串。

    可选的第二个参数是 Unicode 字符集合，不是原始字节集合。详见 trim(3)。

### 参考

    trim(3), ltrim(3)
