+++
title = "Tamper protection"
summary = "Why a connected session can never change or stop the program it is connected through, and what happens when it tries."
section = "security"
order = 2
keywords = "tamper protection integrity guard ban binary config data watched hashed terminal command kill taskkill systemctl hardened ptrace dumpable unban"
+++

A connected session must never change or stop the program it is connected through. On by default (`banOnTamper`).

## The idea {#idea}

The person at the PC trusts xdesk to show them everything a session does. A session that edits xdesk, its settings or its registry, or kills it, could hide itself or lock them out: so it is banned at once.

## What is watched {#watched}

| Watched | How | Reaction |
|---|---|---|
| the binary, the config folder, the data folder | hashed and watched | a change while a session is acting (a command running in one of its terminals, input in the last 5 s) bans those sessions; a change while no session acts is logged as a local change (a rebuild, an update) |
| lines typed in a remote terminal | checked **before** their Enter reaches the shell: paths of xdesk, its name, `kill` / `taskkill` / `systemctl` of its pid... | the line is not run, the session is banned |
| processes started from the remote terminals | scanned (Linux: command line and open files) | ban |
| the host process | hardened (Linux: not dumpable, not traceable; Windows: terminate, memory and thread rights removed) | a host killed during a remote session bans those devices at the next start |

## Local actions only {#local}

- Remote input is paused while the local user uses the panel (Windows: the panel is excluded from the capture).
- Unban, trust, forget and the settings are refused while a remote session has control or a terminal.
- Lifting a ban asks the word `unban` typed; `xdesk unban` refuses pipes and shells started by xdesk.

> [!WARNING]
> These checks stop the usual ways of touching xdesk from a remote terminal, not a determined attacker with a shell running as your user. The real protection is not giving the terminal right to people you do not trust.
