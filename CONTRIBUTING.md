# AI Generated Code

I don't care if you use AI to write your code. But I do not tolerate my time being wasted with AI slop in PRs. So
here are the rules, read them, or I will close your pull request without even asking.

## YOU HAVE TO UNDERSTAND IT

You will not get way with pasting generated LLM code blindly. I **will** find a flaw in your code. If you copy paste
huge blocks of code from an LLM, you better know exactly what every single line does. If I do a code review and ask why
you use a certain data structure and you cannot give a proper reason, your PR is getting rejected. Write the PR
description in your own words. Don't let the robot do your thinking for you.

## TEST BEFORE PUSHING

Run the unit tests. Like actually run them on your local machine before you commit. If the tests fail during CI/CD I
will blast you for it.

## CLEAN UP FORMATTING AND COMMENTS

AI usually leave weird comments in PRs I've seen like `// loop through the array`. It's useless, delete it. Any
competent programmer would recognize this immediately, but I'm spelling it out for you anyway.

That is it for the AI rules.

# Forking and Branching (Do NOT use `main`)

Your `main` branch should be a pristine, 1:1 mirror of the upstream `main` branch. Never commit directly to your `main`
branch.

The correct workflow:

1. Fork the repository.
2. Clone your fork locally.
3. Add the upstream remote: `git remote add origin https://github.com/pound-emu/ballistic.git`
4. Sync your local main: `git checkout main && git pull origin main`
5. Create a feature branch: `git checkout -b feature-name`

You do your work on `feature-name`. You push `feature-name` to your fork. You open a Pull Request from `feuature-name`.
Please leave `main` alone.

# Bisectability

Every single commit in your PR must **individually** compile and pass tests.

If I am tracking down a bug two years from now using `git bisect`, and I land on your commit where Ballistic doesn't
even compile because you "added the header but forgot the source file", I have to manually skip that commit. If you do
this multiple times, you've ruined the bisection and wasted my time.

Before you push, run an interactive rebase with the --exec flag to test every commit in your PR automatically:

```console
git rebase -i main --exec "cd build && make -j$(nproc) && ctest"
```

I will also run this command on your commits and if it fails I will reject your PR.

# Atomic Commits

Do not combine unrelated changes into a single commit. A commit must be a single, logical unit of work.

- If your commit says "fix X **and** Y", split it.
- If you are adding a new feature that requires a core engine change, do it in two commits:
    1. `engine: add new capability`
    2. `decoder: use new capability for X`
- If i need to `git revert` your commit, it should cleanly remove the feature without breaking three other unrelated
  things.

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

Every commit must include a `Signed-off-by` tag. This means you are legally liable for the code you submit to Ballistic.
Use `git commit -s` to add it automatically.

# The Pull Request Process

If I review your PR and ask for changes, do not add a new commit that says "fix review comments."

1. Make the changes locally.
2. Use `git commit --fixup` or `git rebase -i` to fold these changes back into the original commits where they belong.
3. `git push --force-with-lease` to update the PR.
4. The final history should look like you got it right the first time.

# Code Style and Standard

I chose BARR-C 2018 as the code standard to reduce as much bugs as possible. This will be enforce by our CI. If you push
code that is not formatted correctly, the CI will fail to build your commit. Use `clang-format` to format your files.
