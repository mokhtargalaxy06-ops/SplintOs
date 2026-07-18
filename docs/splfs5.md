# SPLFS5 timestamp format

`SPLFS5` is the next explicit version of the SplintOS teaching filesystem. It
keeps the flat eight-entry directory, allocation bitmap, contiguous extents,
checksums, and clean/dirty commit protocol from `SPLFS4`. It adds fixed-width
file timestamps without changing sector geometry.

## On-disk entry

Every little-endian directory entry is exactly 56 bytes:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 32 | NUL-terminated UTF-8 name |
| 32 | 4 | file size in bytes |
| 36 | 4 | first data sector |
| 40 | 4 | contiguous sector count |
| 44 | 4 | creation time (`btime`) |
| 48 | 4 | content modification time (`mtime`) |
| 52 | 4 | metadata change time (`ctime`) |

Eight entries occupy 448 bytes of the directory sector; the remaining 64 bytes
must be zero. Timestamps are unsigned seconds since 1970-01-01 00:00:00 UTC.
Zero means that no trustworthy wall-clock value was available. The 32-bit
representation is intentionally bounded and valid through 2106; a future wider
format requires another version.

## Update semantics

- Create sets all three values to the same checked clock snapshot.
- Content write and truncate update `mtime` and `ctime`.
- Rename updates `ctime` while preserving `btime` and `mtime`.
- Unlink removes the entry, including all timestamps.
- Allocation-only recovery does not invent a new timestamp.
- A failed metadata commit exposes neither an unchecked timestamp nor a falsely
  clean superblock.

Clock conversion validates Gregorian fields and leap years. If RTC acquisition
or conversion fails, the operation continues with zero for a new entry or
preserves existing timestamps for an update.
The shared device clock helper converts only years 1970 through 2106, rejects
invalid month-specific days, and uses widened arithmetic before narrowing to
the on-disk field. Boot fixtures cover the Unix epoch, 2000-01-01, the 2000 leap
day, and rejection of 2100-02-29.

Ring 3 reads these fields with append-only syscall 38, `stat_timestamps`. Its
fixed 60-byte record extends the original metadata fields with the three
32-bit clocks; syscall 24 retains its original 48-byte record unchanged.

## Version and migration policy

The superblock magic is `SPLFS5\0\0`, version is `5`, and all existing geometry
and feature fields remain checked. A `SPLFS4` directory must never be decoded as
56-byte entries.

Normal boot inspects a valid `SPLFS4` volume read-only using a separate 44-byte
entry decoder and never migrates or formats it. The storage matrix verifies a
whole-image hash before and after boot. Migration remains an explicit recovery
operation:

1. Validate the complete `SPLFS4` superblock, directory, bitmap, and extents.
2. Build a separate `SPLFS5` directory with identical names and extents and zero
   timestamps.
3. Commit through dirty superblock, bitmap, new directory, flush, and clean
   checksummed superblock boundaries.
4. Remount and validate as `SPLFS5` before reporting success.

Unknown, malformed, dirty-with-directory-drift, or write-failing media is never
migrated. Tests must retain a byte-for-byte hash when migration is not explicitly
requested.

The trusted kernel console exposes this operation as `migrate-disk`, and that
console exists only in the explicit GRUB recovery boot. Migration rewrites the
already validated in-memory legacy entries with zero timestamps through the
same dirty-to-clean commit protocol. A boot conformance fixture remounts the
result as SPLFS5 before reporting the migration path online.
