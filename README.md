# FitSolver — CI Starter Kit

A minimal, verified-working CMake + Google Test project wired up with
GitHub Actions CI. Drop this into your repo as a starting point and
replace the placeholder code with the real packing engine.

## What's included

```
.github/workflows/ci.yml   # CI: build + test on every push/PR
.github/workflows/cd.yml   # CD: build + push Docker image to GHCR on merge to main
Dockerfile                 # Production image (slim, deployment)
Dockerfile.dev              # Dev environment image (full build/debug tools)
docker-compose.yml          # One-command way to spin up the dev environment
.devcontainer/              # VS Code "Reopen in Container" config
.dockerignore
CMakeLists.txt             # Root build config; pulls in GoogleTest via FetchContent
src/                       # Example library + app target
tests/                     # Example Google Test file
```

## How it works

1. On every push and every PR into `main`, GitHub Actions spins up an
   Ubuntu runner.
2. It configures the project with CMake, builds it, then runs the test
   suite with `ctest`.
3. If the build fails or any test fails, the workflow run is marked ❌
   and (depending on your branch protection settings) the PR can't be
   merged until it's fixed.

This has been built and tested locally end-to-end (`cmake -B build`,
`cmake --build build`, `ctest --output-on-failure`) to confirm the
exact sequence the CI workflow runs actually works.

## Setting this up in your repo

1. Copy `.github/workflows/ci.yml`, `CMakeLists.txt`, `src/`, and
   `tests/` into your repository (merge with your existing structure
   if you already have source files).
2. Replace `src/packer.cpp` / `src/packer.h` / `src/main.cpp` with your
   real FitSolver source files, and update the `add_library` /
   `add_executable` calls in `CMakeLists.txt` to point at them.
3. Add real tests in `tests/`, following the same `gtest_discover_tests`
   pattern — any `.cpp` file added as a source to the `fitsolver_tests`
   executable in `CMakeLists.txt` will be picked up automatically.
4. Commit and push. Go to the **Actions** tab on GitHub to watch the
   workflow run.
5. Once it's green, turn on branch protection (Settings → Branches →
   add rule for `main` → require status checks to pass → select the
   `build-and-test` check).

## Adding the CI badge to your README

Once the workflow has run at least once, add this to the top of your
main README (replace `ORG` and `REPO`):

```markdown
![CI](https://github.com/ORG/REPO/actions/workflows/ci.yml/badge.svg)
```

This renders as a live green/red badge showing the current build
status — useful evidence to show in sprint reviews.

## Notes

- `FetchContent` downloads GoogleTest fresh on first configure, so the
  very first CI run will be a little slower (subsequent runs can be
  sped up later with `actions/cache` on the `build/_deps` directory if
  build times become a problem).
- If you move to Drogon for the API layer, add `find_package(Drogon
  REQUIRED)` (after installing/fetching it) and link it into
  `fitsolver_app` the same way `fitsolver_core` is linked now. The
  Dockerfile already installs Drogon's common system dependencies
  (openssl, zlib, uuid, jsoncpp) so it won't need changes for that.

## CD — Docker image to GHCR

`.github/workflows/cd.yml` runs on every push to `main` (i.e. after a
PR has merged and CI has already passed on it). It builds the
`Dockerfile` and pushes the image to **GitHub Container Registry**
(`ghcr.io`) using the built-in `GITHUB_TOKEN` — no extra secret needed.

Each build is tagged two ways:
- the short commit SHA (e.g. `ghcr.io/your-org/fitsolver:a1b2c3d`) — for traceability back to an exact commit
- `latest` — always the newest successful build on `main`

**First-time setup:**
1. Push once — the workflow runs automatically on the next merge to `main`.
2. Go to your repo's **Packages** tab on GitHub to see the published image.
3. By default a new GHCR package is **private**. If your team wants to
   pull it without authenticating, go to the package settings and make
   it public (or use a token to pull if it stays private).

To pull and run it locally:
```bash
docker pull ghcr.io/YOUR-ORG/YOUR-REPO:latest
docker run -p 8080:8080 ghcr.io/YOUR-ORG/YOUR-REPO:latest
```

## Dev environment — no one has to install C++ tooling locally

Rather than everyone installing g++, CMake, and Drogon's system
libraries by hand (and hitting different bugs on Mac vs Windows vs
Linux), the repo includes a **dev container** with everything
preinstalled. Two ways to use it, same result:

### Option A — VS Code (easiest for most people)

1. Install the **Dev Containers** extension in VS Code.
2. Open the repo folder in VS Code.
3. Click **"Reopen in Container"** when prompted (or Cmd/Ctrl+Shift+P →
   "Dev Containers: Reopen in Container").
4. VS Code builds the image once, then drops you into a terminal
   inside the container with everything already installed. Edit files
   normally — your changes are on your actual disk (bind-mounted), the
   container just provides the toolchain.

### Option B — plain Docker / docker-compose (any editor, any OS)

```bash
# Build the dev image once
docker compose build

# Start it and get a shell inside
docker compose run --rm dev

# Inside the container, build and test exactly like CI does:
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && ctest --output-on-failure
```

Your local files are mounted into `/workspace` inside the container,
so anything you edit on your host is what gets built — nothing is
copied into the image. Exit the shell any time; your `build/` folder
persists on disk for the next run.

**Why this matters for a 4-person team:** whoever wrote the Dockerfile
originally already had to solve "what libraries does this need" —
everyone else just runs one command and gets the identical
environment, instead of each person separately debugging missing
`libssl-dev` or a wrong CMake version on their own machine.
