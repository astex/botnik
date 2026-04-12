# CLAUDE.md

See [README.md](README.md).

## Testing after changes

Run the headless test script to verify your changes don't break the build or basic headless functionality:

```sh
bash scripts/test-headless.sh
```

Exit code 0 = pass. The script builds botnik, tests headless startup/shutdown, and optionally tests a chat round-trip if Ollama is running. See README.md for details.
