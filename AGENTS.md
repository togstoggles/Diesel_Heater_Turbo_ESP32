# Agent Development Guide

This repository uses a **verification-first** development approach.

## Core rules
- Understand the existing behaviour and architecture before editing.
- Prefer the smallest change that solves the requested problem.
- Preserve existing working behaviour unless a change is explicitly requested.
- Do not perform unrelated refactors, dependency upgrades, file deletions, or broad rewrites.
- Important requirements should become code-level invariants, validators, tests, types, or CI checks where practical rather than relying only on prompt instructions.
- When a bug is fixed, add a regression check where practical so the same failure is harder to reintroduce.
- For hardware/network/external-service code, keep real I/O behind clear adapters and prefer simulators, mocks, recorded fixtures, or replay data for automated verification where practical.
- Never claim a change is complete merely because the code was written. Run the relevant build/tests/checks and state what was actually verified.
- If something cannot be verified in the available environment, say exactly what remains unverified and give a short physical/manual test procedure.
- Keep commits/changes reviewable and reversible.

## Next substantive edit: deeper repository pass
On the **next substantive code edit**, perform a more thorough repository-specific pass before making broad changes:
1. Identify the main entry points, major modules, state/data flow, and external or hardware boundaries.
2. Identify existing tests, validation, build commands, and common failure points.
3. Record useful project-specific findings in lightweight documentation such as `ARCHITECTURE.md` and/or `VERIFY.md` if they do not already exist.
4. Add or improve a small number of high-value automated checks around the area being changed.
5. Do **not** refactor the whole repository just to satisfy this pass. Improve incrementally while preserving current behaviour.

## Definition of done
A change is done when its intended behaviour is implemented, relevant checks pass, regressions have been considered, and any remaining unverified hardware/manual behaviour is clearly called out.
