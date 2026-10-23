# Release notes

## 11.2.2

- Prevent freezes when opening or restoring large files. Automatic whole-document formatting skips documents larger than 1 MiB or 10,000 lines before reading text.
- Limit automatic formatting to tables of 500 lines and 128 KiB with bounded lookaround, including typing, resize and zoom callbacks. Small tables in large documents still autoformat; explicit table commands remain available without these automatic-work limits.
- Enumerate document tables in one pass and bound estimated columns, cells and formatted output before automatic alignment. Whole-document replacement output is also capped at 1 MiB before any edit is applied.
- Skip automatic edits when an oversized adjacent row may continue the table, while allowing small leading-pipe tables beside clearly separate prose without copying the oversized line.
- Skip formatting read-only buffers and avoid copying oversized lines on Enter.

## Validation

- Editor-message regression tests cover large documents, separator-free pipe blocks, ragged and wide tables, oversized adjacent lines, read-only cell navigation, and small-table automatic/manual formatting inside a large document.
