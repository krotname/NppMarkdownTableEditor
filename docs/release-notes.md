# Release notes

## 11.2.1

- Prevent freezes when opening or restoring large files. Automatic whole-document formatting skips documents larger than 1 MiB or 10,000 lines before reading text.
- Bound automatic current-table scans to 500 lines and 128 KiB, including typing, resize and zoom callbacks. Small tables in large documents still autoformat; explicit table commands remain available without these automatic-work limits.
- Skip formatting read-only buffers and avoid copying oversized lines on Enter.

## Validation

- Editor-message regression tests cover large documents, many short lines, oversized lines and tables, read-only buffers, and small-table automatic/manual formatting inside a large document.
