<?php
/**
 * Packs one tests/ThommeaxMinimumTests/<scenario> fixture with
 * dvdoug/boxpacker, BoxGroup included (as LinkedItem), and prints the box
 * count and box types used -- ground truth for what the reference engine
 * itself produces on the same input fitsolver's thommeax_benchmark_test.cpp
 * packs.
 *
 * Usage: php run_thommeax.php <scenario dir>
 */

declare(strict_types=1);

require __DIR__ . '/vendor/autoload.php';

use DVDoug\BoxPacker\Box;
use DVDoug\BoxPacker\Item;
use DVDoug\BoxPacker\LimitedSupplyBox;
use DVDoug\BoxPacker\LinkedItem;
use DVDoug\BoxPacker\Packer;
use DVDoug\BoxPacker\Rotation;

class FixtureItem implements Item
{
    public function __construct(
        protected string $code,
        protected int $width,
        protected int $length,
        protected int $depth,
        protected int $weightGrams,
        protected Rotation $rotation,
    ) {
    }

    public function getDescription(): string { return $this->code; }
    public function getWidth(): int { return $this->width; }
    public function getLength(): int { return $this->length; }
    public function getDepth(): int { return $this->depth; }
    public function getWeight(): int { return $this->weightGrams; }
    public function getAllowedRotation(): Rotation { return $this->rotation; }
}

final class FixtureLinkedItem extends FixtureItem implements LinkedItem
{
    public function __construct(string $code, int $w, int $l, int $d, int $g, Rotation $r, private string $group)
    {
        parent::__construct($code, $w, $l, $d, $g, $r);
    }

    public function getLinkedItemGroup(): string { return $this->group; }
}

final class FixtureBox implements LimitedSupplyBox
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

function rotationFor(?string $name): Rotation
{
    return match ($name) {
        'Never' => Rotation::Never,
        'KeepFlat' => Rotation::KeepFlat,
        default => Rotation::BestFit,
    };
}

$dir = rtrim($argv[1] ?? '', '/');
// "linked": BoxGroup as BoxPacker 4.3's LinkedItem (whole group in one box).
// "passes": the reference RESULTS.md's own method -- ungrouped items pack
// together with the largest group, then each other group packs alone, with
// box quantities carried over between passes.
$mode = $argv[2] ?? 'linked';
$items = json_decode(file_get_contents("$dir/items.json"), true, flags: JSON_THROW_ON_ERROR);
$boxes = json_decode(file_get_contents("$dir/boxes.json"), true, flags: JSON_THROW_ON_ERROR);

$boxObjects = [];
foreach ($boxes as $b) {
    if (!($b['Active'] ?? true) || !isset($b['MaxWeight'])) {
        continue;
    }
    $boxObjects[] = new FixtureBox(
        $b['Reference'], (int) $b['Width'], (int) $b['Length'], (int) $b['Depth'],
        (int) round(($b['BoxWeight'] ?? 0) * 1000), (int) round($b['MaxWeight'] * 1000),
        isset($b['MaximumBoxes']) ? (int) $b['MaximumBoxes'] : PHP_INT_MAX,
    );
}

function makeItem(array $it, bool $linked): Item
{
    $args = [
        $it['ItemCode'], (int) $it['Width'], (int) $it['Length'], (int) $it['Depth'],
        (int) round($it['Weight'] * 1000), rotationFor($it['AllowedRotation'] ?? null),
    ];
    return ($linked && !empty($it['BoxGroup'])) ? new FixtureLinkedItem(...$args, group: $it['BoxGroup']) : new FixtureItem(...$args);
}

if ($mode === 'linked' || $mode === 'plain') {
    $passes = [$items];
} else {
    $unitsPerGroup = [];
    foreach ($items as $it) {
        if (!empty($it['BoxGroup'])) {
            $unitsPerGroup[$it['BoxGroup']] = ($unitsPerGroup[$it['BoxGroup']] ?? 0) + (int) ($it['Quantity'] ?? 1);
        }
    }
    arsort($unitsPerGroup);
    $largest = $argv[3] ?? array_key_first($unitsPerGroup);
    $passes = [array_values(array_filter($items, fn ($it) => empty($it['BoxGroup']) || $it['BoxGroup'] === $largest))];
    foreach (array_keys($unitsPerGroup) as $group) {
        if ($group !== $largest) {
            $passes[] = array_values(array_filter($items, fn ($it) => ($it['BoxGroup'] ?? '') === $group));
        }
    }
}

$used = [];
$byType = [];
$totalBoxes = 0;
$totalUnplaced = 0;
$start = hrtime(true);
foreach ($passes as $passItems) {
    $packer = new Packer();
    $packer->throwOnUnpackableItem(false);
    foreach ($passItems as $it) {
        $packer->addItem(makeItem($it, $mode === 'linked'), (int) ($it['Quantity'] ?? 1));
    }
    foreach ($boxObjects as $box) {
        $packer->addBox($box);
        $packer->setBoxQuantity($box, max(0, $box->getQuantityAvailable() - ($used[$box->getReference()] ?? 0)));
    }
    $packed = $packer->pack();
    foreach ($packed as $pb) {
        $ref = $pb->box->getReference();
        $byType[$ref] = ($byType[$ref] ?? 0) + 1;
        $used[$ref] = ($used[$ref] ?? 0) + 1;
    }
    $totalBoxes += $packed->count();
    $totalUnplaced += $packer->getUnpackedItems()->count();
}
$ms = (hrtime(true) - $start) / 1e6;

ksort($byType);
printf("mode=%s passes=%d boxes=%d unplaced=%d ms=%.1f\n", $mode, count($passes), $totalBoxes, $totalUnplaced, $ms);
foreach ($byType as $ref => $n) {
    printf("  %3d x %s\n", $n, $ref);
}
