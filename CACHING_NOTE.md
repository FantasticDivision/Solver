# Caching branch — note for Umair

Hi Umair — this branch holds Aaron's solution to the packing-result caching
problem. It was originally developed on the `MaxWeight` branch alongside some
unrelated work and has been split out here on its own so it's easier to
review/pick up independently.

## What's here

- `src/SolutionCache.h` / `.cpp` — a thread-safe, LRU-bounded lookup table
  (`SolutionCache`) that memoizes `Packer::Pack` results, keyed by a
  canonical, order-independent hash of the item/box content
  (`CanonicalCacheKey`).
- `PackCached(packer, items, boxes, cache)` — the convenience entry point:
  look up a cached result first, else solve and store it.
- `tests/unit/solutioncache_test.cpp` — unit tests for the cache (eviction,
  touch/promotion, capacity-0 disables caching) and `PackCached` (cache
  reuse, separate entries per distinct problem).

See `src/SolutionCache.h`'s header comment for the full design notes,
including a documented caveat: the cache key depends only on item/box
*content*, not on which `ItemSorter`/`BoxSorter` was injected, so don't share
one `SolutionCache` across callers using different sorters for otherwise
identical item/box content.

Note this branch also carries the entropy-based sorting work
(`EntropyItemSorter`/`EntropyBoxSorter`/`PackingEntropy`) and the
`MaxWeight`-required change, since it branched off `MaxWeight` before those
were split apart — the caching code above is the part that matters for this
review.
