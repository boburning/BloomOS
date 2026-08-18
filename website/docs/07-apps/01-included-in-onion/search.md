---
slug: /apps/search
---

# Search and Filter

Global game search and keyword filtering are not currently included in
BloomOS.

The inherited implementation was available as source, but its upstream
repository did not declare an authoritative project license. A commit pin and
reproducible build are not enough to establish permission to redistribute that
code and its assets, so BloomOS excludes the submodule and both package
integrations.

Game List Options still provides **Refresh list** for rebuilding an individual
system's ROM cache. Existing ROMs, favorites, play activity, saves, and cached
artwork are not deleted by this packaging change.

Search can return after BloomOS adopts an authoritatively licensed project or
ships an independently implemented replacement with tests for large libraries,
Unicode names, archive entries, favorites, and every supported device family.
