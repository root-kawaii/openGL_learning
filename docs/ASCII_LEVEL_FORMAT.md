# ASCII Level Format

`AsciiLevelUtils` turns a 2D text layout into a normal scene JSON file.

Current symbols:

- `#`: wall cell
- `.`: empty walkable space
- `P`: player spawn marker placeholder
- `C`: chest placeholder
- `E`: enemy placeholder
- `T`: torch prop with warm light
- `L`: light only
- `B`: boulder / blocking prop

Rules:

- Empty lines are ignored.
- Lines starting with `;` or `//` are treated as comments.
- Common indentation is trimmed automatically.
- The generated level gets one large floor cube and one large ceiling cube.
- The generated JSON also writes `meta.player_spawn`, which current runtime code ignores safely but future tasks can use.

Example:

```text
###########
#..T...C..#
#..##.....#
#.P..E..L.#
#....B....#
###########
```

Suggested workflow:

1. Author a layout text file.
2. Run a small conversion helper in code or from a future tool task using `AsciiLevelUtils::saveLevelFromTextFile(...)`.
3. Load the generated JSON level normally through the existing scene pipeline.
