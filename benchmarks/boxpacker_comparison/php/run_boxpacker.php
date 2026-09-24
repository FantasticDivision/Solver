<?php
/**
 * Runs every scenario in scenarios.json through dvdoug/boxpacker (the
 * reference engine FitSolver's VolumePacker is a faithful port of -- see
 * FantasticSolver/README.md), timing each solve individually, and writes
 * boxpacker_results.csv with the same schema run_fitsolver.cpp produces:
 *   id,total_units,placed,unplaced,boxes_used,solve_ms
 *
 * Usage: php run_boxpacker.php <scenarios.json> <results.csv>
 */

declare(strict_types=1);

require __DIR__ . '/vendor/autoload.php';

use DVDoug\BoxPacker\Box;
use DVDoug\BoxPacker\DefaultTimeoutChecker;
use DVDoug\BoxPacker\Exception\TimeoutException;
use DVDoug\BoxPacker\Item;
use DVDoug\BoxPacker\LimitedSupplyBox;
use DVDoug\BoxPacker\Packer;
use DVDoug\BoxPacker\Rotation;

// BoxPacker's own WeightRedistributor uses the same pair-relative variance
// formula fitsolver's port does (see WeightRedistributor.cpp), which is not
// globally monotone and can cycle between box pairs without converging --
// confirmed empirically on one of this benchmark's own random scenarios.
// BoxPacker ships an opt-in TimeoutChecker for exactly this; without it a
// pathological scenario could hang this whole suite the same way it hung
// fitsolver_boxpacker_bench before WeightRedistributor.cpp grew its 250ms
// deadline. A scenario that times out still counts fully against
// dvdoug/boxpacker's average/worst time below -- this is a safety net for
// the suite, not a thumb on the scale.
const TIMEOUT_SECONDS = 5.0;

final class ScenarioItem implements Item
{
    public function __construct(
        private string $code,
        private int $width,
        private int $length,
        private int $depth,
        private int $weightGrams,
        private Rotation $rotation,
    ) {
    }

    public function getDescription(): string { return $this->code; }
    public function getWidth(): int { return $this->width; }
    public function getLength(): int { return $this->length; }
    public function getDepth(): int { return $this->depth; }
    public function getWeight(): int { return $this->weightGrams; }
    public function getAllowedRotation(): Rotation { return $this->rotation; }
}

// No wall thickness in FitSolver's model, so Inner == Outer here -- keeps
// the two engines packing into the exact same usable volume.
final class ScenarioBox implements Box, LimitedSupplyBox
{
    public function __construct(
        private string $reference,
        private int $width,
        private int $length,
        private int $depth,
        private int $emptyWeightGrams,
        private int $maxWeightGrams,
        private int $quantityAvailable,
    ) {
    }

    public function getReference(): string { return $this->reference; }
    public function getOuterWidth(): int { return $this->width; }
    public function getOuterLength(): int { return $this->length; }
    public function getOuterDepth(): int { return $this->depth; }
    public function getInnerWidth(): int { return $this->width; }
    public function getInnerLength(): int { return $this->length; }
    public function getInnerDepth(): int { return $this->depth; }
    public function getEmptyWeight(): int { return $this->emptyWeightGrams; }
    public function getMaxWeight(): int { return $this->maxWeightGrams; }
    public function getQuantityAvailable(): int { return $this->quantityAvailable; }
}

function rotationFor(string $name): Rotation
{
    return match ($name) {
        'Never' => Rotation::Never,
        'KeepFlat' => Rotation::KeepFlat,
        default => Rotation::BestFit,
    };
}

/** kg (float) -> grams (int), same rounding on both sides of the comparison. */
function grams(float $kg): int
{
    return (int) round($kg * 1000.0);
}

if ($argc < 3) {
    fwrite(STDERR, "usage: php {$argv[0]} <scenarios.json> <results.csv>\n");
    exit(2);
}
[, $scenariosPath, $resultsPath] = $argv;

$scenarios = json_decode(file_get_contents($scenariosPath), true, flags: JSON_THROW_ON_ERROR);

$out = fopen($resultsPath, 'w');
fwrite($out, "id,total_units,placed,unplaced,boxes_used,solve_ms\n");

$solveTimesMs = [];
$overBudget = 0;
$errors = 0;

foreach ($scenarios as $scenario) {
    $packer = new Packer();
    $packer->throwOnUnpackableItem(false); // match FitSolver: report unplaced, don't abort
    $packer->setTimeoutChecker(new DefaultTimeoutChecker(TIMEOUT_SECONDS));

    foreach ($scenario['items'] as $itemType) {
        $item = new ScenarioItem(
            $itemType['ItemCode'],
            (int) $itemType['Width'],
            (int) $itemType['Length'],
            (int) $itemType['Depth'],
            grams((float) $itemType['Weight']),
            rotationFor($itemType['AllowedRotation']),
        );
        $packer->addItem($item, (int) $itemType['Quantity']);
    }

    foreach ($scenario['boxes'] as $boxType) {
        if (!($boxType['Active'] ?? true)) {
            continue; // FitSolver's Packer::Pack also skips inactive boxes entirely
        }
        $quantity = array_key_exists('MaximumBoxes', $boxType) ? (int) $boxType['MaximumBoxes'] : PHP_INT_MAX;
        $box = new ScenarioBox(
            $boxType['Reference'],
            (int) $boxType['Width'],
            (int) $boxType['Length'],
            (int) $boxType['Depth'],
            grams((float) ($boxType['BoxWeight'] ?? 0.0)),
            grams((float) $boxType['MaxWeight']),
            $quantity,
        );
        $packer->addBox($box);
        if ($quantity !== PHP_INT_MAX) {
            $packer->setBoxQuantity($box, $quantity);
        }
    }

    $start = hrtime(true);
    try {
        $packedBoxes = $packer->pack();
        $boxesUsed = $packedBoxes->count();
        $unplaced = $packer->getUnpackedItems()->count();
    } catch (TimeoutException $e) {
        // The pairwise weight redistribution can cycle without converging
        // (see the TIMEOUT_SECONDS comment above) -- record the timeout
        // itself as this scenario's time rather than aborting the run.
        fwrite(STDERR, "scenario {$scenario['id']} timed out after {$e->getSpentTime()}s\n");
        $boxesUsed = -1;
        $unplaced = -1;
    } catch (\Throwable $e) {
        // Defensive only -- throwOnUnpackableItem(false) should prevent this.
        // Record as a failed scenario rather than aborting the whole run.
        fwrite(STDERR, "scenario {$scenario['id']} failed: {$e->getMessage()}\n");
        $boxesUsed = -1;
        $unplaced = -1;
        ++$errors;
    }
    $ms = (hrtime(true) - $start) / 1_000_000.0;

    $solveTimesMs[] = $ms;
    if ($ms > 1000.0) {
        ++$overBudget;
    }

    $placed = $scenario['total_units'] - ($unplaced < 0 ? 0 : $unplaced);
    fwrite($out, sprintf(
        "%d,%d,%d,%d,%d,%.4f\n",
        $scenario['id'],
        $scenario['total_units'],
        $placed,
        $unplaced,
        $boxesUsed,
        $ms,
    ));
}
fclose($out);

sort($solveTimesMs);
$count = count($solveTimesMs);
$average = $count ? array_sum($solveTimesMs) / $count : 0.0;
$median = $count ? $solveTimesMs[intdiv($count, 2)] : 0.0;
$worst = $count ? $solveTimesMs[$count - 1] : 0.0;

printf("boxpacker: %d scenarios\n", $count);
printf("  average solve time: %.3f ms\n", $average);
printf("  median solve time:  %.3f ms\n", $median);
printf("  worst solve time:   %.3f ms\n", $worst);
printf("  scenarios over 1000 ms: %d\n", $overBudget);
printf("  errored scenarios: %d\n", $errors);
printf("results written to %s\n", $resultsPath);
