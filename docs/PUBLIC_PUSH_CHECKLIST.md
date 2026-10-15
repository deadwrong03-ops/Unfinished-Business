# Public Push Checklist

- [ ] Build `Release | x64` successfully from the working project.
- [ ] Confirm the addon loads in GW2.
- [ ] Confirm API validation and account progress sync work.
- [ ] Confirm the password field clears after validation.
- [ ] Confirm restart auto-validates the DPAPI-saved key.
- [ ] Confirm `Forget Key` prevents auto-validation on the next restart.
- [ ] Run `scripts\\check-secrets.ps1`.
- [ ] Inspect `git status --ignored`.
- [ ] Inspect `git diff --cached` before pushing.
- [ ] Never stage `.vs`, `x64`, `*.user`, logs, PDBs, dumps, or `api_key.dat`.
- [ ] Use a GitHub-provided `noreply` commit email if you do not want your personal email exposed in commit metadata.
- [ ] Verify the remote points to your new Unfinished Business repository before the first push.
