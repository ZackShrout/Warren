# ADR-0013: Project License

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phase 0 onward
- **Supersedes:** The all-rights-reserved placeholder in README.md
- **Superseded by:** None

## Context

Warren needs a license before meaningful source distribution or outside
contribution. The license should allow study, modification, ports, SDK and
application use, and commercial experimentation while providing an explicit
patent grant. Warren also expects third-party components whose upstream terms
must remain separate.

## Decision

Warren-owned source code, build descriptions, and documentation are licensed
under the **Apache License, Version 2.0** unless a file or directory states
otherwise.

The repository root contains the complete Apache-2.0 text in `LICENSE`.
Third-party material retains its original license and provenance; its presence
does not relicense it as Apache-2.0. Generated binary distributions include all
notices required by their contained third-party material.

Contributions intentionally submitted for inclusion follow Apache-2.0's default
contribution terms unless a separate written contribution agreement applies.
The Warren, Burrow, Hare, Meadow, Forage, and BunnySoft names are not granted as
trademarks by this software license.

## Alternatives Considered

### MIT

MIT is shorter and highly permissive. Apache-2.0 is selected for its explicit
patent grant and clearer contribution/notice framework.

### MPL-2.0

MPL provides file-level reciprocity while allowing separate larger works under
other terms. The project currently prioritizes low-friction use and porting over
requiring published modifications to Warren files.

### GPL

GPL provides strong reciprocity and would help keep distributed derivative
kernels open. Its combined-work and distribution obligations are not the chosen
policy for Warren's SDK, application, and experimentation goals.

### No license yet

This preserves all rights by default but prevents clear reuse, contribution, and
distribution. Warren now has executable build infrastructure and is ready to
state its terms.

## Consequences

### Benefits

- explicit permission to use, modify, and distribute Warren;
- an express contributor patent grant;
- compatible permissive posture for SDKs and applications; and
- standard contribution licensing without inventing custom terms.

### Costs And Risks

- downstream distributed modifications need not be published;
- attribution and notice obligations must be preserved;
- third-party license compliance remains a separate ongoing responsibility; and
- project names/branding may eventually need an explicit trademark policy.

### Follow-Up Work

- add `LICENSE` and update README.md;
- maintain third-party provenance and notices from the first vendored dependency;
- decide whether a root `NOTICE` file becomes useful; and
- seek legal review before material public/commercial distribution if needed.

## Revisit When

- BunnySoft's policy changes toward reciprocity;
- outside contributions create a need for a formal contribution agreement;
- project branding needs a separate trademark policy; or
- a planned third-party integration presents a license incompatibility.
