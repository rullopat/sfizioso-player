# Evening Signals

Open `instrument.sfz` in a Player build containing manifest support. The OUTPUT
page shows the instrument/preset names, Tone and Expression sections, Brightness
and Vibrato knobs, a Volume slider, a Sustain toggle, and a packaged background.
Play the docked keyboard or a MIDI keyboard. The SFZ uses a built-in oscillator,
so no sample download is needed. All audio behaviour and defaults are in SFZ.

To try the identical presentation from a portable bundle:

```sh
python examples/manifest-instrument/build_bundle.py /tmp/evening-signals.sfzbundle
```

Open that file in the Player. To try fallback, temporarily rename
`instrument.json` in a copy of this directory and reopen the SFZ: the generic
SFZ-labelled controls remain playable. Editing/removing the sibling manifest
also triggers the existing reload poll. Reopen the SFZ after changing artwork.

The sample JSON, SFZ, example writer, and generated gradient background are
provided under the repository's BSD-2-Clause licence.

See the [README option reference](../../README.md#all-manifest-options) for every
supported field, or [Simple Sine](../manifest-minimal/) for a minimal manifest
that retains the Player's default controls.
