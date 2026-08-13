# Warren Branching And Integration

Warren is developed through focused Git branches while `main` remains the last
integrated, verified project state. The branching model exists to protect the
bootable spine, make incomplete work easy to isolate, and keep each change
explainable. It is intentionally lightweight enough for a one-maintainer
lifetime project.

## Permanent Branch

`main` is the sole permanent integration branch.

Every commit on `main` must represent a coherent repository state that:

- configures from documented presets;
- builds every currently required debug and release target;
- passes the required host and QEMU tests;
- preserves the last completed vertical capability; and
- contains the documentation needed to understand its contracts.

New feature, foundation, fix, documentation, tooling, and experimental work is
developed on a named branch. There is no separate long-lived `develop` branch.

## Branch Scope

A branch owns one coherent outcome. Before substantial implementation begins,
its scope states:

- the observable result;
- included work and subsystem ownership;
- explicit non-goals;
- architectural decisions it must introduce or refine;
- verification and failure cases; and
- conditions required to merge.

A useful branch can change several layers when one vertical result requires
them. Conversely, several unrelated improvements do not belong together merely
because they are individually small. The test is whether one outcome explains
why every material change is present.

Plans for substantial branches live under `docs/plans/`. Plans describe intent
and merge gates; accepted ADRs remain the authority for architectural choices.
A branch plan may evolve as implementation produces evidence, but scope growth
must be recorded rather than silently absorbed.

An implementation branch does not rewrite an accepted ADR to match its code.
Any correction, clarification, supersession, or even editorial change to an
accepted ADR requires explicit discussion under the process in `DECISIONS.md`.

## Naming

Use a short lowercase name with a category prefix:

```text
foundation/phase-0-contracts
feature/burrow-image
feature/burrow-first-entry
fix/uefi-memory-map-retry
docs/virtual-memory-layout
tooling/boot-log-parser
experiment/aarch64-el2-transition
```

Categories describe the branch's dominant outcome, not every file it may touch.
An experiment does not define a production interface and is not merged until it
is either reshaped into a supported implementation or removed.

## Development Rules

- Begin from a verified `main` unless the branch explicitly depends on another
  unmerged branch.
- Keep commits small enough to explain and test, but do not split one invariant
  across commits merely to manufacture activity.
- Do not combine unrelated cleanup with foundational implementation.
- Keep machine-local state, downloaded dependencies, and generated build output
  out of commits.
- Preserve the existing working path alongside a replacement until automated
  evidence shows the replacement provides the required behavior.
- Reassess later runway items after every merge; only the next branch receives a
  fully committed scope.

Dependent branches are allowed when they materially improve development flow.
Their dependency and intended merge order must be explicit, and they must not
be mistaken for independently mergeable work.

## Integration Gate

Before merging into `main`:

1. Review the complete diff from the branch base.
2. Resolve or document every scope change and new dependency.
3. Update specifications and operator instructions affected by the work; add or
   supersede an ADR only after its architectural change is explicitly discussed.
4. Run the branch plan's verification matrix from clean build trees where the
   risk warrants it.
5. Confirm existing required tests still pass in debug and release profiles.
6. Confirm the resulting `main` preserves or advances the last observable
   end-to-end capability.

Merge style may vary with the work, but history must retain enough structure to
explain the completed capability and any architectural decision. After
integration, abandoned alternatives and obsolete branches may be deleted; their
important reasoning belongs in the repository rather than in branch names.

## Urgent And Tiny Changes

Small does not mean exempt from branching. A typo correction, build repair, or
urgent fix may use a correspondingly small branch and abbreviated plan, but it
still passes the checks proportional to its risk. The cost of a branch is low;
the value is that `main` never doubles as an uncertain workspace.
