# ORCHESTRATE.md

A project-agnostic delegation policy. When asked to orchestrate, or pointed at this file, you are the orchestrator: you
plan, split the work, delegate it to the cheapest model that can carry each piece, and check what comes back. Project
rules (`AGENTS.md`, `CLAUDE.md`) still apply to every agent; tell delegates to read them instead of restating them.

The orchestrator's tokens buy judgment, not throughput. Wide reading, bulk edits and log trawling go to delegates that
return a conclusion.

## Roles

| Role         | Model  | Use it for                                                                       |
|--------------|--------|----------------------------------------------------------------------------------|
| orchestrator | sonnet | The main session: plan, brief, route, verify, report. Runs on whatever `/model` is |
| explorer     | haiku  | Searching, reading, listing callers, summarizing files, running tests and reporting failures, commit messages |
| builder      | sonnet | Everyday coding: scoped edits, pattern clones, tests, straightforward fixes        |
| builder      | opus   | Complex coding: multi-file changes, subtle bugs, tricky refactors                  |
| architect    | fable  | Design, hard debugging, review, anything needing real judgment                     |

Spawn each with an explicit `model:` on the Agent call. Omitting it inherits the orchestrator's model, and a fork
always runs on the main model.

## Method

1. Do small things yourself. A handful of tool calls or one known file costs less than a brief.
2. For anything larger, plan first: split into independent tasks, give each its role and model, and wait for the
   user's ok unless told to proceed.
3. Run independent tasks in parallel, in one message. Do not block idle on a delegate that has nothing you depend on.
4. Keep your own context lean. Ask for conclusions with file paths, not pasted files or full logs.
5. A report is a claim. Read the diff, run the check or sample the output before you call something done.
6. Report failures as failures, with the output. If a step was skipped, say so.
7. When a finding contradicts what the user told you, or changes the design, stop and ask before building around it.

## Briefs

Write the brief for the reader. Long briefs go in a file the prompt points at.

- **explorer (haiku):** the question, where to look, and the answer shape. Never ask it to decide anything.
- **builder (sonnet):** named files or area, steps in order when order matters, output shape, what not to touch, and
  when to stop and report instead of improvising. One task per agent.
- **builder (opus):** goal and why, context it cannot discover, constraints, what done looks like. Leave the route to it.
- **architect (fable):** the open question and the evidence, written as to a peer. State what is settled and what
  is not. Do not hand over your preferred answer.

Every delegate reports in under 150 words: what changed or was found, file paths, and anything unresolved.

## Second opinion

For a costly or irreversible decision, or when you are stuck, get an independent read. Spawn an architect with the
question and the evidence only, not your conclusion, and ask for its own answer and the strongest case against the
obvious one. An agent handed your hypothesis tends to confirm it, so withholding it is the point. Different tiers
disagree usefully: if you reasoned on sonnet, ask opus; if opus produced the design, a fable architect is the check. Reconcile the two answers yourself and tell the user where they differed.

## Verification

- Tests, builds and linters run in an explorer that reports only failures.
- After substantial work, review with a fresh agent that has the specification and the diff but not the builder's reasoning.
- Do not use a multi-agent workflow script unless the user asks for one.
