# AGENTS.md — D3D11 3dPROJECT

## Project Overview

This repository is a Visual Studio C++ / Direct3D 11 3D action prototype.

The project is **not** a Unity or Unreal project.
Do not introduce engine-specific patterns from Unity/Unreal unless explicitly requested.

The current core focus is a playable 3D action prototype with:

* Player movement, attack, roll, parry, block, and hit reactions
* Boss/enemy behavior
* Animation playback and root motion
* HitBox / HurtBox based combat
* Camera free mode and lock-on mode
* HUD for player HP and boss HP
* Frame-event driven combat timing

When modifying the code, prioritize maintaining a stable, buildable, playable prototype.

---

## General Rules for Codex

1. Prefer small, targeted changes.
2. Do not perform large architecture rewrites unless explicitly asked.
3. Do not rename existing files, classes, functions, or variables unless necessary.
4. Do not change gameplay behavior silently.
5. Preserve existing coding style as much as possible.
6. After editing code, check for compile errors and obvious runtime regressions.
7. Explain what was changed and why.
8. If a requested feature touches multiple systems, describe the affected systems before editing.

---

## Development Environment

Target environment:

* Windows 11
* Visual Studio
* C++
* Direct3D 11
* Git / GitHub Desktop

Expected build target:

* x64
* Debug first
* Release only when needed

If a `.sln` file exists, prefer building through MSBuild or Visual Studio project files.

Typical build command pattern:

```bat
msbuild <SolutionName>.sln /p:Configuration=Debug /p:Platform=x64
```

If the exact solution name is unknown, inspect the repository first and use the existing `.sln` file.

Do not add CMake, external package managers, or new build systems unless explicitly requested.

---

## Coding Style

Use the existing project style.

General C++ preferences:

* Prefer clear, straightforward C++ over clever abstractions.
* Keep gameplay code readable for learning and debugging.
* Avoid unnecessary templates or overly generic systems.
* Use explicit names for gameplay concepts.
* Keep math and collision code easy to inspect.
* Prefer simple helper functions over large monolithic blocks.

When adding code:

* Keep related logic near the existing system that owns it.
* Do not scatter one feature across many unrelated files unless necessary.
* Add comments only when they explain gameplay intent or non-obvious math.
* Do not over-comment obvious code.

---

## Architecture Boundaries

Respect the current project architecture.

### Player FSM

The player has a finite state machine with states such as:

* Idle
* Move
* Attack
* Roll
* Parry
* Block
* HitLight
* HitMid
* HitHeavy

Rules:

* Passive reactions such as hit reactions should be controlled by the state machine.
* Roll should keep invincibility behavior intact.
* Parry and block timing should stay frame-event driven where applicable.
* Do not bypass the FSM by forcing animation or movement directly from unrelated systems.

### Root Motion

The project uses Mixamo animations processed through Blender so that animations have a `root` bone.

Known conventions:

* `motionRootNameUTF8 = "root"`
* Walk usually does not consume animation root motion.
* Attack may use animation delta.
* Roll may use Z delta while zeroing XZ depending on current implementation.
* Root motion yaw may be accumulated into player yaw.

Rules:

* Be careful when editing animation or movement code.
* Do not remove root motion handling.
* Do not assume all animations behave the same.
* If changing root motion behavior, explain which state or animation is affected.

### Camera

The project supports:

* Free camera
* Lock-on camera toggle, usually with middle mouse button
* In lock-on mode, player orientation may face the boss/target

Rules:

* Do not break existing lock-on behavior when editing player rotation or camera code.
* If changing camera logic, test both free mode and lock-on mode.

### Collision and Combat

Current concepts include:

* Player Body AABB
* HurtBox
* HitBox
* Roll invincibility through disabling hurt detection
* Frame events for attack active windows, parry windows, and invincibility windows

Known approximate conventions:

* Player Body AABB: `{ 0.4, 0.9, 0.4 }`
* Player HurtBox: `{ 0.5, 1.2, 0.5 }`
* Roll may set hurt enabled to false during invincible frames

Rules:

* Do not merge Body collision and HurtBox logic unless explicitly asked.
* Do not make attacks always active.
* Preserve frame-event driven hit timing.
* When adding a new attack, make its active frames clear.
* Avoid duplicate hit registration unless the feature explicitly requires multi-hit behavior.

### HUD

Current HUD conventions:

* Player HP is shown near the upper-left.
* Boss HP is shown near the lower center.
* Boss HP uses a red bar with chip/background behavior.

Rules:

* Do not replace the HUD system unless requested.
* Keep debug displays easy to toggle or remove.

---

## Input Conventions

Current input conventions include:

* WASD: movement
* Mouse left button: attack
* Shift: roll
* Mouse middle button: lock-on toggle

Rules:

* Do not change existing input bindings unless explicitly requested.
* When adding new input, avoid conflicts with current bindings.
* If input behavior changes, document the new control clearly.

---

## Gameplay Design Intent

This project is a 3D action prototype with a focus on readable combat behavior.

Important design intentions:

* Player actions should feel responsive.
* Enemy attacks should have readable timing.
* Hit reactions should be understandable.
* Guard, parry, roll, and counterattack should have clear roles.
* Combat debugging visibility is important.

Known gameplay hint:

* Guard or parry can lead to counterattack opportunities.
* Yellow flash or similar feedback may indicate a counter timing.

When implementing features, prefer clarity and debugability over complex production-style systems.

---

## Animation and Frame Events

Frame events are important for combat timing.

Examples of frame-event style logic:

* ParryWindowBegin
* ParryWindowEnd
* Roll invincibility begin/end
* Attack HitBox spawn/enable/disable

Rules:

* Do not replace frame-event timing with rough timer guesses unless requested.
* If adding an event, keep the event name explicit.
* If a bug involves hit timing, inspect frame events before rewriting combat logic.
* Animation changes should not silently change gameplay windows.

---

## Enemy and Boss Behavior

When modifying enemy or boss behavior:

* Keep attack startup, active, recovery, and cooldown understandable.
* Do not make attacks unavoidable unless explicitly intended.
* If adding tracking, acceleration, or projectile logic, expose values clearly.
* Prefer debuggable values over hardcoded magic numbers.

For humanoid bosses:

* Successful player guard/parry should usually create understandable recovery or stiffness.
* If recovery speed is changed, make the value easy to tune.

---

## Debugging Guidelines

When investigating bugs:

1. Reproduce or identify the relevant state.
2. Check whether the issue is input, FSM, animation, root motion, collision, or rendering.
3. Add temporary debug display only when helpful.
4. Remove or isolate temporary debug code before finalizing unless requested.

Useful debug outputs may include:

* Current player state
* Current animation name/time
* Active HitBox/HurtBox status
* HP values
* Lock-on target status
* Root motion delta
* Parry/block window status

---

## Common Tasks

### Adding or modifying player actions

Check:

* Input binding
* Player FSM transition
* Animation selection
* Root motion mode
* Frame events
* Collision / invincibility / hitbox behavior
* Recovery back to Idle or Move

Do not implement an action only in input code.

### Adding or modifying attacks

Check:

* Attack animation
* Startup / active / recovery timing
* HitBox creation timing
* Hit target filtering
* Damage application
* Hit reaction
* Duplicate hit prevention
* Debug visibility

### Fixing collision issues

Check separately:

* Body collision
* HurtBox
* HitBox
* Ground / wall collision
* Roll invincibility
* Lock-on rotation side effects

Do not solve all collision bugs by enlarging boxes without explaining why.

### Fixing animation movement issues

Check:

* Whether root motion is enabled for that state
* Which bone is used as motion root
* Whether XZ/Y/Z deltas are intentionally filtered
* Whether yaw is applied
* Whether the animation itself contains unwanted translation

---

## Testing / Verification

After code changes, make a best effort to verify:

1. The project builds in Debug x64.
2. The game starts without crashing.
3. Player can move, attack, roll, parry/block if relevant.
4. Lock-on mode still works if movement/rotation/camera code changed.
5. Hit detection still works if combat code changed.
6. HUD still displays if HP/combat code changed.

Preferred build command:

```bat
msbuild <SolutionName>.sln /p:Configuration=Debug /p:Platform=x64
```

If MSBuild is unavailable, explain that the build could not be run and describe what was checked manually.

---

## Git Rules

Before editing:

* Inspect existing files.
* Search for references before changing shared functions.
* Understand current naming and ownership.

When done:

* Summarize modified files.
* Summarize behavior changes.
* Mention build/test results.
* Mention any remaining risks or unverified areas.

Do not create unrelated formatting-only changes.

---

## What Not To Do

Do not:

* Convert the project to another engine.
* Introduce Unity/Unreal concepts into the codebase.
* Replace the FSM with a new framework.
* Remove root motion support.
* Replace frame-event timing with approximate timers without approval.
* Add large third-party libraries.
* Rewrite rendering architecture unless explicitly asked.
* Hide gameplay constants deep inside unrelated code.
* Make broad refactors while fixing a small bug.

---

## Communication Style

When responding to the user:

* Use Chinese unless the user asks otherwise.
* Explain technical changes clearly.
* For complex gameplay or engine behavior, explain the design reason, not only the code diff.
* When proposing multiple options, recommend one practical first version.
* Prefer implementation plans that are suitable for a student / prototype project.
* Keep code examples complete enough to copy into the project.

The user values step-by-step technical explanation and clear boundaries between design intent, system responsibility, and actual code changes.

---

## Project-Specific Priority

For this project, correctness and clarity are more important than abstraction.

When choosing between:

* A small understandable fix
* A large generic framework

Prefer the small understandable fix unless the user explicitly asks for a framework-level redesign.
