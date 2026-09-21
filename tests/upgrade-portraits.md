# Upgrade portrait regression checks

The construction window fits the complete framed portrait row inside the
`TXT_INFO` parchment bounds, with a four-pixel inset. All portraits use one
scale factor and are never enlarged. Two native 164 x 99 portraits fit a
282-pixel row as two 137 x 83 images separated by eight pixels.

The resized image registers the native magenta color key on its output
surface. Other `CImage2Memory` callers remain opaque by default.

## Automated checks

Run on Windows with the Visual Studio C++ x86 tools installed. The second
argument must be an existing output directory outside the source tree:

```bat
tests\run-upgradeportraitlayout-test.cmd "<VS>\VC\Auxiliary\Build\vcvarsall.bat" "<output>"
tests\run-image2memory-transparency-test.cmd "<VS>\VC\Auxiliary\Build\vcvarsall.bat" "<output>"
```

- Layout: small/large combinations, equal scaling, parchment containment,
  translated coordinates, constrained height, invalid sizes and popup rows.
- Transparency: production `image2memory.cpp` with mocked game APIs, RGB565
  and RGB555, native/cnc-ddraw/legacy C4 backing layouts, keyed and opaque
  images, row padding, buffer guards and destruction.

## In-game reproduction

Use an isolated copy of the game and its data. To create a two-large-unit
case in the Undead construction tree, a test fixture can swap these
`Gunits.dbf` `UPGRADE_B` values:

| Unit | Original | Test fixture |
| --- | --- | --- |
| `g000uu0095` | `g000bb0093` | `g000bb0092` |
| `g003uu5013` | `g000bb0092` | `g000bb0093` |

This puts `g000uu0094` and `g000uu0095`, both `SIZE_SMALL=F`, in the same
`g000bb0092` building. These data changes are only a reproduction fixture;
they are not part of the patch and must not be shipped to players.

1. Open the Undead capital's construction window and select that building.
2. Check that both framed dragons have the same dimensions, stay inside the
   parchment and have no magenta outline.
3. Right-click each portrait and check the corresponding unit card.
4. Switch to a one-large/one-small branch, then an ordinary branch and back.
   Check that the original controls and clickable areas are restored.
5. Use the game's screenshot function to capture visual evidence.

Automated checks do not replace this visual and interaction check. The
final color-key correction has passed the automated tests; a fresh in-game
capture of that correction has not yet been recorded.
