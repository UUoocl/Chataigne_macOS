# Apple Shortcuts Custom Module (OS + WebSocket Return)

A Chataigne custom module based on the `OS` module that triggers macOS Apple Shortcuts (`/usr/bin/shortcuts`) non-blocking and returns results as a JSON payload over WebSocket.

## Installation

Symlink or copy this folder into Chataigne's custom modules directory:

```bash
ln -sfn "$(pwd)/custom modules/Apple Shortcuts" ~/Documents/Chataigne/modules/"Apple Shortcuts"
```

(Or place it in a `modules/Apple Shortcuts` folder next to your `.noisette` project file.)

## Setup in Chataigne

1. Add **Protocol → WebSocket Server**:
   - Set **Local Port** (e.g. `8080`).
   - Set **Protocol** to `JSON`.
2. Add **System → Apple Shortcuts**:
   - Set **WebSocket URL** to `ws://127.0.0.1:8080` (matching the WebSocket Server port).

## Commands

- **Run Shortcut** (`Shortcut Name`, optional `Input Text`)
- **List Shortcuts**

## Returned JSON Payloads (via WebSocket)

### Run Shortcut
```json
{
  "ok": true,
  "action": "run",
  "shortcut": "My Shortcut",
  "output": "{\"status\":\"done\"}",
  "outputJson": { "status": "done" },
  "error": ""
}
```

### List Shortcuts
```json
{
  "ok": true,
  "action": "list",
  "shortcut": "",
  "shortcuts": ["Shortcut A", "Shortcut B"],
  "count": 2,
  "error": ""
}
```
