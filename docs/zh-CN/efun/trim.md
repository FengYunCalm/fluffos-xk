---
layout: doc
title: strings / trim
---
# trim

### 名称

    trim() - 移除字符串两侧的空白字符或其他预定义字符

### 语法

    string trim( string str );
    string trim( string str, string ch);

### 描述

    移除字符串 `str` 两侧的空格或 `ch` 中的其他字符，并返回一个新的字符串。

    可选的第二个参数是 Unicode 字符集合，不是原始字节集合。`trim("《三字经》", "　")`
    会保留书名；U+3000 与 U+300A 共享 UTF-8 前缀，但属于不同字符。

### 参考

    ltrim(3), rtrim(3)
