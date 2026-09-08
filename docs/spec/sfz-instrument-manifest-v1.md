# SFZ Instrument Manifest v1 — initial Player implementation

Status: draft, implemented for review in Sfizioso Player. Related:
[rullopat/sfizioso-player#9](https://github.com/rullopat/sfizioso-player/issues/9).
This is an optional Player convention, not an SFZ standard change.

An instrument author can group Brightness and Volume under “Tone”, choose a
slider for Volume and a toggle for Sustain, and include artwork. The same SFZ
remains playable in players without manifest support.

## Ownership

| Layer | Owns |
| --- | --- |
| SFZ | Sound, modulation, MIDI CC defaults/current values, fallback labels |
| `instrument.json` | Identity, preset catalogue, artwork, section/control order, widget hints |
| Shared presentation libraries and UI kit | Snapshot construction, artwork validation, control layout and widgets |
| Host product | Preset selection, asset source, CC dispatch, theme, shell and resource serving |

The shared BSD-2-Clause `sfizioso_player::instrument_manifest` target uses JUCE
Core and the existing bundle UTF-8 validator. The engine has no JSON or GUI
code. The BSD-2-Clause `sfizioso_player::instrument_presentation` target adds artwork
decoding through JUCE Graphics and bridge DTO construction, without linking the
Player application or engine. The shared React renderer and configurable bridge
hook live in `src/ui-shared/`. The host builds and publishes a presentation
snapshot on its instrument-load path while processing is suspended. `processBlock` never reads metadata/assets.
The bridge sends presentation separately from live `ccValues` events.

## Authoring contract

See the [JSON Schema](sfz-instrument-manifest-v1.schema.json) and
[complete example](../../examples/manifest-instrument/instrument.json).

- `format` is `sfizioso.instrument-manifest`; integer `formatVersion` is `1`.
- `instrument.name` is required. `id`, `author`, `version` are optional strings.
- `presets` has 1–256 entries, each with nonblank `id`, `name`, and `sfz`.
  IDs and SFZ paths must be unique. Optional fields: `category`, `author`.
- `artwork.controlsBackground` references a PNG or JPEG.
- `presentation.accent` is a `#RRGGBB` colour hint, scoped to the controls area.
- `presentation.sections` has at most 32 sections. Each has a unique `id`,
  optional `label` (defaults to its ID), and up to 128 `controls`.
- Each control targets `{ "type": "cc", "number": 0..127 }`. A CC appears at
  most once per preset. Array order is display order. An optional `label`
  overrides SFZ `label_ccN`, then falls back to `CC N`.
- `widget` is `knob` (default), `slider`, or `toggle`. Knobs/sliders use 0–127;
  toggles send 0/127 and display on at values >=64.
- SFZ `set_ccN` and live engine state supply values. No manifest defaults, DSP,
  arbitrary parameter targets, or dynamic DAW/APVTS parameters are created.
- Strings are bounded to 4096 characters. Unknown fields are ignored.

The schema describes valid authored documents. The runtime is deliberately
more forgiving about presentation: bad controls/sections are skipped, unknown
widgets become knobs, and invalid accents/artwork are discarded. Empty usable
presentation falls back to the SFZ-labelled grid. Valid authored sections show
only the controls they list. Runtime uniqueness, nonblank identities, encoding,
aggregate byte/depth, and file-resolution rules supplement the schema.

## Discovery and selection

For a loose SFZ, look only for sibling `instrument.json`. The manifest must
contain a preset whose `sfz` exactly equals the loaded file's name; otherwise
ignore it. Paths are case-sensitive manifest identifiers, UTF-8 with `/`
separators, and relative to this directory. No parent-directory search occurs.

For a plaintext `.sfzbundle`, look for the unique `MetadataJson` entry named
`instrument.json`. Every catalogue SFZ must resolve to exactly one `SfzText`
entry. Load the first catalogue preset, regardless of table order. There is no
preset browser in this version. Without valid canonical metadata, load the
first SFZ table entry and retain legacy `presets.json` instrumentName/patchName
display support. The catalogue is not MIDI Program Change: SFZ program routing
continues working within the loaded SFZ.

Missing, malformed, oversized, unsupported-major, or unresolved manifests never
block valid SFZ audio. Invalid container structure and invalid SFZ data still
fail normally. Canonical manifest diagnostics appear in a collapsible details
area. Loading another instrument clears the previous presentation and artwork.
Sibling manifest changes trigger the existing reload poll; artwork-only edits
require reopening the SFZ. Saved sessions retain the SFZ/bundle path and load
metadata again from the package.

## Assets and limits

All manifest paths reject absolute/drive/UNC paths, backslashes, empty or dot
components, traversal, control characters, colon/URL schemes, percent encodings,
queries, and fragments. Loose files are canonicalised and checked beneath the
instrument root, including symlinks. Bundle assets require exact entry names;
there is no basename fallback or temporary extraction for assets.

JSON is limited to 1 MiB and 32 nesting levels before parsing. Each encoded
image is limited to 8 MiB, 4096 pixels per dimension, and 8,388,608 total pixels.
Only PNG or baseline/progressive JPEG signatures are accepted. Dimensions are
checked before decoding; the application decodes and re-encodes a static PNG
for the WebView resource provider. Resource URLs include a load revision to
avoid stale cached artwork. Missing/invalid artwork falls back to the theme.
The manifest cannot supply WebView HTML, CSS, JS, fonts, SVG, remote resources,
coordinates, scripting, or executable content.

## Bundle addition

The shared SMPB header remains version 2: 48-byte header, 256-byte entries,
existing entry values 0–7 unchanged. `Asset = 8` adds package-relative opaque
asset bytes. A Player resolves referenced PNG/JPEG assets through that entry
type. Other readers can ignore it. See the existing
[byte layout and bounded parser](../../src/sfzbundle/README.md).

## Follow-up work

This PR implements the Player consumer and a reproducible example writer.
Sample Machine exporter integration, a full legacy catalogue adapter/shared
serializer, a public container inspector, optional `sm_pkg_manifest` discovery,
SFZ `image_controls` fallback, a generic preset browser, and branded-rompler
integration are separate follow-ups. No new engine opcode is needed to try
this version. No release or consumer submodule update is part of this PR.

## Validation

`cmake --build build` and `ctest --test-dir build --output-on-failure` include
parser/path/asset cases, bundle selection/fallback, rendered-audio equivalence,
CC dispatch, and a real PlayerProcessor session-restore test using the example.

For browser-side widget/layout/refresh regression checks, build the UI, serve
the repository root with `python -m http.server 18743 --bind 127.0.0.1`, then run
`node tests/ui/manifest.mjs` with Playwright available. `PLAYWRIGHT_MODULE` can
point to an installed Playwright `index.mjs`. This test mocks the native bridge;
it does not substitute for native plugin validation. Screenshots go to the
system temporary directory.

For reproducible README screenshots, with the same local server and Playwright
setup, run `node tools/capture-manifest-examples.mjs`. It reads the committed
example files, supplies deterministic bridge data to the built React UI, and
writes `docs/assets/manifest-instrument.png` and `manifest-minimal.png`. It does
not capture the user's desktop or run the audio engine.
