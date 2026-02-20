If you are reading this, you want to contribute to this project. Good. I want to get Ballistic to a working state as soon as possible so future developers will use Ballistic in their Switch 2 emulators.

But lets get one thing straight right now: **The Git history is sacred.** If you don't follow these rules, your Pull Request will be closed, and you will be told to go learn how to use Git properly.

# Forking and Branching (Do NOT use `main`)

Your `main` branch should be a pristine, 1:1 mirror of the upstream `main` branch. Never commit directly to your `main` branch.

The correct workflow:

1. Fork the repository.
2. Clone your fork locally.
3. Add the upstream remote: `git remote add origin https://github.com/pound-emu/ballistic.git`
4. Sync your local main: `git checkout main && git pull origin main`
5. Create a feature branch: `git checkout -b feature-name`

You do your work on `feature-name`. You push `feature-name` to your fork. You open a Pull Request from `feature-name`. Please leave `main` alone.

# Bisectability

Every single commit in your PR must **individually** compile and pass tests.

If I am tracking down a bug two years from now using `git bisect`, and I land on your commit where Ballistic doesn't even compile because you "added the header but forgot the source file", I have to manually skip that commit. If you do this multiple times, you've ruined the bisection and wasted my time.

Before you push, run an interactive rebase with the --exec flag to test every commit in your PR automatically:

```console
git rebase -i main --exec "make -j$(nproc) && ctest"
```

I will also run this command on your commits and if it fails I will reject your PR.

# Atomic Commits

Do not combine unrelated changes into a single commit. A commit must be a single, logical unit of work.

- If your commit says "fix X **and** Y", split it.
- If you are adding a new feature that requires a core engine change, do it in two commits:
    1. `engine: add new capability`
    2. `decoder: use new capability for X`
- If i need to `git revert` your commit, it should cleanly remove the feature without breaking three other unrelated things.

# Commit Message

A commit mesaage needs to explain the **why**, not just the **what**.

```text
prefix: short, imperative summary (under 72 chars)

Blank line.

Detailed explanation of the problem being solved, why this approach 
was chosen, and any edge cases considered. Wrap this text at 72 
characters.

Signed-off-by: Your Name <your.email@example.com>
```

## Prefix Rules

Map your prefix to the subsystem or directory you are touching:

* `assembler:` (`src/bal_assembler.c`)
* `decoder:` (`src/bal_decoder*`)
* `engine:` (everything else in `src/`)
* `tests:` (`tests/*`)
* `tests/translate:` (`tests/translate/*`)
* `tests/decoder:` (`tests/test_decoder.c`)
* `docs:` (for documentation)

## The Sign-off

Every commit must include a `Signed-off-by` tag. This means you are legally liable for the code you submit to Ballistic. Use `git commit -s` to add it automatically.

# The Pull Request Process

If I review your PR and ask for changes, do not add a new commit that says "fix review comments."

1. Make the changes locally.
2. Use `git commit --fixup` or `git rebase -i` to fold these changes back into the original commits where they belong.
3. `git push --force-with-lease` to update the PR.
4. The final history should look like you got it right the first time.

# Code Style and Standard

I chose BARR-C 2018 as the code standard to reduce as much bugs as possible. This will be enforce by our CI. If you push code that is not formatted correctly, the CI will fail to build your commit. Use `clang-format` to format your files.
