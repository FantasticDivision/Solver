# BoxPacker comparison benchmark

Generates 1000 random packing requests and runs each through both this
engine and [dvdoug/boxpacker](https://github.com/dvdoug/boxpacker) 4.3 (the
PHP library this port is modelled on -- see the root README's
"Architecture" section), to check two things:

1. fitsolver's average solve time is lower.
2. fitsolver never uses more boxes than dvdoug/boxpacker on the same input.

See the root [README.md](../../README.md)'s "Time complexity" and
"Benchmark" sections for the current results and what they mean. This
directory is what produced those numbers, and how to reproduce them.

## Requirements

- This repo already built (`cmake -S . -B build && cmake --build build
  --config Release`), for the C++ side.
- Docker, for the PHP side -- no local PHP/Composer install needed.
- Python 3, for the scenario generator and comparison script.

## Running it

```bash
# 1. Generate the scenarios (deterministic; skip this to reuse the checked-in scenarios.json)
python generate_scenarios.py --count 1000 --seed 1337

# 2. Install dvdoug/boxpacker (only needed once, or after bumping the version in php/composer.json)
docker run --rm -v "$(pwd)/php":/app -w /app composer:2 install --no-interaction

# 3. Run fitsolver over every scenario
cmake --build ../../build --config Release --target fitsolver_boxpacker_bench
../../build/Release/fitsolver_boxpacker_bench.exe scenarios.json fitsolver_results.csv

# 4. Run dvdoug/boxpacker over the same scenarios
docker run --rm -v "$(pwd)":/bench -w /bench/php --entrypoint php composer:2 \
  run_boxpacker.php /bench/scenarios.json /bench/boxpacker_results.csv

# 5. Compare
python compare.py
```

Step 2's `composer:2` image needs outbound HTTPS to Packagist. If your
network has TLS-inspecting security software (a corporate proxy, EDR
agent, etc.), the container won't have that root CA in its trust store by
default and the install will fail with a certificate error. Fix (don't
disable verification): export Windows' trusted roots and mount them in
before installing --

```powershell
$outDir = "$env:TEMP\winroot-ca"
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
foreach ($storePath in @("Cert:\LocalMachine\Root", "Cert:\CurrentUser\Root")) {
  Get-ChildItem $storePath | ForEach-Object {
    $b64 = [Convert]::ToBase64String($_.RawData, 'InsertLineBreaks')
    "-----BEGIN CERTIFICATE-----`n$b64`n-----END CERTIFICATE-----`n" |
      Set-Content -Path (Join-Path $outDir "$($_.Thumbprint).crt") -NoNewline -Encoding ascii
  }
}
```

```bash
docker run --rm -v "$(pwd)/php":/app -v "$CA_DIR":/usr/local/share/ca-certificates \
  -w /app --entrypoint sh composer:2 -c "update-ca-certificates && composer install --no-interaction"
```

## What's here

- `generate_scenarios.py` -- the scenario generator. Deliberately uses
  only the feature subset both engines are verified to agree on (item/box
  dimensions, weight, `AllowedRotation`, box `MaxWeight`/`BoxWeight`/
  `Active`/`MaximumBoxes`) -- no `BoxGroup`, since that's a segregation
  *policy* rather than a packing-density concern.
- `scenarios.json` -- the checked-in output of the generator at seed 1337,
  so the numbers in the root README are reproducible without relying on
  `random`'s stream staying identical across Python versions.
- `run_fitsolver.cpp` -- times each scenario through `fitsolver::Packer`
  directly (no CLI/process-spawn overhead), writing `fitsolver_results.csv`.
  Built via the `fitsolver_boxpacker_bench` CMake target (`EXCLUDE_FROM_ALL`,
  same as `fitsolver_scenario_bench`).
- `php/run_boxpacker.php` -- the same scenarios through dvdoug/boxpacker,
  writing `boxpacker_results.csv` in the same schema. Sets a 5-second
  per-scenario `DefaultTimeoutChecker` as a safety net -- see the note in
  that file and the root README's "Time complexity" section on why the
  weight-redistribution phase both engines share isn't guaranteed to
  converge.
- `compare.py` -- joins the two result files by scenario id and reports
  both claims, plus fitsolver's own worst-case time against the 1-second
  budget, to `comparison_report.md`.

## Units

FitSolver's wire format is doubles for width/length/depth (mm) and weight
(kg). dvdoug/boxpacker's `Item`/`Box` interfaces are integers, with weight
in **grams**, not kg -- `run_boxpacker.php`'s `grams()` helper converts
consistently on that side.
