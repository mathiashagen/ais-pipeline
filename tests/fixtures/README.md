# AIS test fixtures

## kystverket_live_sample.nmea

765 lines of real, live AIS traffic captured directly from Kystverket's open
AIS feed at `153.44.253.27:5631` on 2026-08-19. See
https://www.kystverket.no/sjotransport-og-havn/ais/tilgang-pa-ais-data/ for
access details (free, no registration required, NLOD-licensed).

**Not plain AIVDM.** Each line is prefixed with an NMEA tag block —
`\s:<station>,c:<unix-timestamp>*<checksum>\` — before the `!...VDM` sentence
itself. `Sentence` does not strip this itself; a raw line handed to it as-is
will be parsed incorrectly (wrong checksum range, shifted comma count).
`DecoderStage` separates the tag block (see `tag_block.hpp`) before parsing,
and uses its station as the source when reassembling fragments.

Also includes three different talker IDs (`B1VDM`, `B2VDM`, `BSVDM` — not the
`AIVDM` used in every synthetic test fixture so far) and ~266 lines that are
part of a multi-fragment (`fragment_count == 2`) message, useful for testing
`SentenceAssembler` against real reassembly cases.

Per Kystverket's terms, external use of this data must be credited to
Kystverket.
