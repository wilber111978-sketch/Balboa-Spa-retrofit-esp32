# Frozen V0.6.2 checkpoints

The reviewed firmware checkpoint is immutable commit **`3cc63df3fa445b5e01342122deafaa788749abcc`**, also referenced by branch `baseline/v0.6.2-reviewed`.

The complete pre-integration demo-app checkpoint is **`bb305ccd27b6ad94ca5db20c2dd0dcadaa89ee62`**.

The live integration is a later commit on `dev/spa-v062-demo-app`; neither checkpoint is rewritten. `firmware/FROZEN_V062.json` records SHA-256 hashes for all 44 baseline firmware/source/test/evidence files, verified against the Git blob IDs of the reviewed firmware commit. Integration builds and tests write to separate live directories.

```sh
python3 firmware/live-tests/verify_frozen.py
```

For an isolated rollback checkout without resetting or discarding the current branch:

```sh
git worktree add ../spa-v062-baseline 3cc63df3fa445b5e01342122deafaa788749abcc
```

For the original working demo UI plus firmware:

```sh
git worktree add ../spa-demo-baseline bb305ccd27b6ad94ca5db20c2dd0dcadaa89ee62
```

Use the baseline README/build script inside that worktree. Running the historical test scripts can refresh their log fixtures; keep such generated evidence changes out of the frozen checkpoint. The new integration regression runner uses the historical scenario sources unchanged but places executables and generated results in `live-tests/` and `live-logs/`.

The baseline includes Main and Demo builds, 141 passing host scenarios and the original compiler evidence. Both physical heater outputs are inhibited. Neither the baseline nor live integration constitutes physical commissioning or electrical certification.
