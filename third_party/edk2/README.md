# EDK2 UEFI ABI Headers

Warren uses a curated header-only snapshot from TianoCore EDK2 to obtain
authoritative UEFI ABI declarations for its bootloader.

## Provenance

- Upstream: <https://github.com/tianocore/edk2>
- Release: `edk2-stable202605`
- Commit: `b03a21a63e3bd001f52c527e5a57feddb53a690b`
- Archive SHA-256: `a3160f2a4f6c574cf7ed929d863461530bc5843c2d5c892db2dc8c1ed02ff5f1`
- License: `BSD-2-Clause-Patent`; see `LICENSE`

`headers.sha256` is both the allowlist and integrity manifest. Only those files
are extracted into `.warren/dependencies/edk2-<commit>/` by
`tools/dependencies/fetch_edk2_headers.sh`.

The snapshot contains 28 headers. The upstream archive is cached locally after
verification, so subsequent bootstrap checks and installs can operate offline.

## Allowed Use

- The Warren UEFI bootloader may include the selected ABI declarations through
  `boot/include/warren/boot/Uefi.h`.
- The snapshot may be expanded only when a bootloader requirement demonstrates
  another declaration is necessary.

## Explicitly Not Used

- EDK2 build system or BaseTools
- EDK2 libraries or startup code
- EDK2 drivers or services
- EDK2 firmware source build
- EDK2 headers in Burrow or Warren userspace

## Local Patches

None. Retained headers must match `headers.sha256` byte-for-byte.
