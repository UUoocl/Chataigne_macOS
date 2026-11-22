# Keynote Custom Module (OS + JXA + WebSocket Return)

A Chataigne custom module based on the `OS` module that controls Apple Keynote via JXA (`osascript -l JavaScript`) non-blocking and returns a JSON payload of slide details over WebSocket.

## Installation

Copy or symlink this folder into Chataigne's custom modules directory:

```bash
ln -s "$(pwd)/custom modules/Keynote" ~/Documents/Chataigne/modules/Keynote
```

(Or place it in a `modules/Keynote` folder next to your `.noisette` project file.)

## Setup in Chataigne

1. Add **Protocol → WebSocket Server**:
   - Set **Local Port** (e.g. `8080`).
   - Set **Protocol** to `JSON`.
2. Add **Software → Keynote**:
   - Set **WebSocket URL** to `ws://127.0.0.1:8080` (matching the WebSocket Server port).
   - Optionally enable **Auto Poll** and set **Poll Interval** (seconds).

## Commands

- **Next Slide**
- **Previous Slide**
- **Go to Slide** (`Slide Number`)
- **Start Presentation** (`From Current Slide`)
- **Stop Presentation**
- **Get Slide Info**

## Returned JSON Payload (via WebSocket)

Every command sends a JSON object to the configured WebSocket Server:

```json
{
  "ok": true,
  "action": "status",
  "running": true,
  "playing": false,
  "documentName": "Presentation.key",
  "totalSlides": 12,
  "slideNumber": 3,
  "slideTitle": "Agenda",
  "slideNotes": "Speaker notes for slide 3",
  "skipped": false,
  "error": "",
  "Scene": "Over-The-Shoulder"
}
```

Any shape on the current slide whose text contains `|||||` is split at the first `|||||` delimiter—text before it becomes a JSON key and text after it becomes its string value in the payload.
