# Warren Third-Party Policy

Third-party material is permitted only when its leverage exceeds its ownership,
bootstrap, licensing, and maintenance cost.

Each dependency directory must contain:

- purpose and owning Warren subsystem;
- upstream project and immutable source revision;
- integrity information for every downloaded archive and retained file;
- the applicable upstream license;
- an explicit statement of what is and is not consumed;
- a checked-in acquisition/update mechanism when the source itself is not
  vendored; and
- a record of every local patch.

Third-party source never silently becomes Warren-owned source. It remains in a
distinct include/build boundary and retains upstream style and licensing.

Heavy dependency archives and generated local trees belong under ignored
`.warren/`. Their checked-in manifests and licenses belong here.
