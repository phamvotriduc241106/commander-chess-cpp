# Commander Chess – Firebase setup note

**Step 1 — Frontend & hosting structure**

- **Main entry file is:** `frontend/public/index.html`
- **Main JS is:** `frontend/public/app.js` (game state: `state`, `gameId`, `moveHistory`, `stateHistory`; result in `state.game_over` / `state.result`)
- **Hosting public folder is:** `frontend/public` (from `firebase.json` → `hosting.public`)

Deploy rules: `firebase deploy --only firestore:rules`

Online mode requirements:
- Enable Firestore in project `commander-chess-cpp-1771534802`
- Enable Authentication `Anonymous` provider (used for guest matchmaking identity)
- Firestore rules file: `firestore.rules`
