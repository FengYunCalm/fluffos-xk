---
layout: doc
title: pcre / pcre_version
---
# pcre_version

The `pcre` efuns use the PCRE2 8-bit library, with a minimum supported version
of 10.42. The legacy efun names, argument order, numeric flags, and callback
behavior remain unchanged. UTF-8 validation is enabled; UCP and JIT are not enabled.

### NAME

    pcre_version() - returns the version of the compiled PCRE library used

### SYNOPSIS

    string pcre_version(void);

### DESCRIPTION

    returns the version of the compiled PCRE library used

### SEE ALSO

    pcre_assoc(3), pcre_cache(3), pcre_extract(3), pcre_replace(3)
