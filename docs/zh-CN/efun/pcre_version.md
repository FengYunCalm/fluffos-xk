---
layout: doc
title: pcre / pcre_version
---
# pcre_version

`pcre` efun 使用 PCRE2 8 位库，最低版本为 10.42。原有 efun 名称、参数顺序、数值 flags 和 callback 行为保持不变；默认启用 UTF-8 校验，不启用 UCP 或 JIT。

### NAME

    pcre_version() - returns the version of the compiled PCRE library used

### SYNOPSIS

    string pcre_version(void);

### DESCRIPTION

    returns the version of the compiled PCRE library used

### SEE ALSO

    pcre_assoc(3), pcre_cache(3), pcre_extract(3), pcre_replace(3)
