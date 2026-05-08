# Create the GitHub Repository

## Option A — GitHub CLI

From inside the repo folder:

```bash
chmod +x scripts/init-local-repo.sh
./scripts/init-local-repo.sh

gh repo create ddr5-spd-diagnostic-notes --private --source=. --remote=origin --push
```

Switch `--private` to `--public` only after checking `sources/`, `logs/`, and `assets/` for anything you do not want online.

## Option B — Manual GitHub web flow

```bash
git init
git add .
git commit -m "docs: seed DDR5 SPD diagnostic notes"
git branch -M main
git remote add origin git@github.com:YOUR_USER/ddr5-spd-diagnostic-notes.git
git push -u origin main
```

## Before making it public

- Confirm no PDFs are committed unless redistribution is allowed.
- Confirm no raw logs contain private system info.
- Choose a real license.
- Add harness photos only if they do not expose anything you care about.
