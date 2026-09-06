# Release notes

The release workflow appends this file to the generated `Builds / Downloads` section of the GitHub
release for the current tag. Rewrite it in the same commit that bumps `Version.props`, so a release
can never ship the previous version's notes.

## What's Changed

- No change in behavior. The table engine gains the two public entry points its JetBrains and Visual
  Studio Code counterparts already had - `findTableRanges` for every table in a document and
  `isPotentialSeparatorLine` for a single line - so all three Markdown Table Editor cores now expose
  the same surface.
- `findTableRange` was refactored onto the same header/separator helper as the new functions, so one
  scan rule serves every caller and discovered ranges still cannot overlap.
- The shared golden fixture gained a `separatorLines` and a `ranges` section (`schemaVersion` 2) and
  the fixture runner executes both, so the three implementations are checked against one file.

## Validation

- Cross-core parity harness: 51629 generated scenarios - every action at many caret positions,
  `applyWrappedToWidth` at 12 widths, CSV/TSV conversion, table creation, range lookup, range
  enumeration, separator detection and cursor-to-column mapping - produced byte-identical reports
  from the Java, C++ and TypeScript cores (234504 lines, same SHA-256). Two mutation controls
  confirm the harness detects divergence.
- Core smoke tests: 85 scenario checks and 156 golden fixture checks. Plugin shortcut smoke tests
  passed. C++ core line coverage 93.76%. Core performance benchmarks within thresholds.
- GitHub Actions release build completed for Win32, x64, and ARM64.
