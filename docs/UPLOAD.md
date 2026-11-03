# Publishing on GitHub

The project repository is https://github.com/Chrizzly92/vibeDSO.

To publish to an empty GitHub repository, add this local repository in GitHub
Desktop and push `main`. With Git on the command line:

```sh
git remote -v
# Add origin only if it is not configured:
git remote add origin https://github.com/Chrizzly92/vibeDSO.git
git push -u origin main
```

If the destination already contains commits, review its state before choosing
how to publish. Replacing a remote branch with a different history is destructive
and is not part of the packaging process.

Build release assets with `python tools/package_release.py`; add
`--desktop PATH_TO_EXE` for a Windows package. Attach the resulting source,
firmware and Windows ZIPs to a GitHub Release. Keep generated binaries out of the
source tree, and include the supplied licenses and NOTICE with distributions.

## Release description

The English description for v0.37 is in [releases/v0.37.md](releases/v0.37.md).
Copy its Markdown into the description when editing the
[v0.37 release](https://github.com/Chrizzly92/vibeDSO/releases/tag/v0.37), then save
the release. Pushing commits through GitHub Desktop updates repository files;
it does not automatically update the GitHub Release description.
