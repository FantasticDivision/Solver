# Thommeax minimum tests

Three benchmark scenarios (`Simple`, `SemiRealistic`, `Chaotic`) supplied by
Thommeax. Each folder has:

- `boxes.json` — candidate box types, in FitSolver's wire format.
- `items.json` — item types. Unlike a real FitSolver request, each entry may
  carry a `Quantity` (how many physical units of that item to pack) since
  that's how the reference data was authored; `tests/integration/
  thommeax_benchmark_test.cpp` expands that into individually-coded units
  before handing the request to `Packer`.
- `RESULTS.md` — the reference result, packed with
  [dvdoug/boxpacker](https://github.com/dvdoug/boxpacker) 4.3 on PHP 8.4.
  This is the benchmark: **our engine should place every unit using no more
  boxes than the reference did.**

`thommeax_benchmark_test.cpp` runs all three scenarios through
`fitsolver::Packer` and asserts:

- every unit is placed (nothing left in `unplaced_items`), and
- the number of boxes used is `<=` the reference's box count.

The same `boxes.json` / `items.json` pairs are also the basis for the
portal-side import JSON used to test these scenarios end-to-end through the
FitPortal UI — see `fantastic-portal`'s equivalent `docs/thommeax-import-
examples/` (generated from these files: each item's `Quantity` becomes an
`Order.Lines` entry instead of a request-level field).
