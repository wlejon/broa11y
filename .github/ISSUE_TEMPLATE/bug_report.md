---
name: Bug report
about: a screen reader gets the wrong role, name, text or offset, misses an event, cannot drive a control, or broa11y hangs or crashes
labels: bug
---

**The tree** (the smallest one that shows it: roles, names, text, states, and
the action handler if a request is involved):

```cpp
```

**What the assistive technology reports or does instead** (Orca / Accerciser /
`accerciser`'s console, Narrator / NVDA / Inspect.exe / Accessibility Insights,
VoiceOver / Accessibility Inspector; paste what it shows or says):

```
```

**Does it reproduce in the tests?** Which `ctest` test fails, if any:

**Environment:**
- OS and version, and the screen reader and its version:
- Linux: X11 or Wayland? at-spi2-core version? Does `bridge.last_error()` say anything?
- broa11y commit:
