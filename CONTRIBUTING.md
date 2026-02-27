# Contributing to Deepalienist

Thanks for contributing to Deepalienist.

## Build prerequisites

Install a modern C++ compiler and GNU Make. Then build from source:

```bash
cd src
make -j profile-build
```

## Reporting issues

Please open an issue in this repository and include:

- OS and compiler/toolchain
- exact build command
- steps to reproduce
- UCI logs (if relevant)

## Submitting pull requests

- Keep pull requests focused and clearly scoped.
- Include a concise summary of what changed and why.
- If behavior changes, include validation steps or test logs.
- For first contributions, add your name to `AUTHORS` when appropriate.

## Code style

C++ changes should follow `.clang-format`.

```bash
cd src
make format
```

## Navigate

If you use `git blame` frequently, you may want:

```bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

## Community

- Use GitHub Issues for bug reports.
- Use GitHub Discussions for questions and design conversations.

## License

By contributing, you agree that your contributions are licensed under GPL v3.0. See `Copying.txt`.
