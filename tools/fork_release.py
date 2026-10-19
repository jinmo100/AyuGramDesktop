#!/usr/bin/env python3
"""Publish a locally validated Windows Debug test build through an owned draft."""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile

REPO = "jinmo100/AyuGramDesktop"
RELEASES_URL = f"https://github.com/{REPO}/releases"
TAG_RE = re.compile(r"^v(\d+\.\d+\.\d+)-jinmo\.([1-9]\d*)$")
ORIGINS = {f"{prefix}{REPO}{suffix}" for prefix in
           ("https://github.com/", "git@github.com:", "ssh://git@github.com/")
           for suffix in ("", ".git")}
CACHE_KEYS = ("DESKTOP_APP_DISABLE_AUTOUPDATE", "TDESKTOP_FORK_RELEASES_URL",
              "CMAKE_HOME_DIRECTORY", "CMAKE_GENERATOR", "CMAKE_GENERATOR_PLATFORM",
              "CMAKE_GENERATOR_TOOLSET", "CMAKE_CONFIGURATION_TYPES",
              "CMAKE_MSVC_RUNTIME_LIBRARY")
GATES = ["clean source and recursive submodules", "selected HEAD contained in pushed origin/dev",
         "successful Debug build invocation", "layout regression executable passed",
         "checkout executable not running", "cache and artifact hashes verified"]
LIMITATIONS = ["Windows application launch and clean-machine runtime dependencies untested",
              "Configured Debug CRT is static (/MTd); PE imports and third-party DLL dependencies untested",
              "No end-to-end updater or account tests"]


class ReleaseError(RuntimeError):
    pass


def run(argv, *, cwd=None, input_text=None, check=True):
    p = subprocess.run(argv, cwd=cwd, input=input_text, text=True, capture_output=True)
    if check and p.returncode:
        raise ReleaseError(f"command failed ({p.returncode}): {argv[0]}")
    return p


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def git(repo, *args):
    return run(["git", *args], cwd=repo).stdout.strip()


def git_path(repo, name):
    path = Path(git(repo, "rev-parse", "--git-path", name))
    return path if path.is_absolute() else Path(repo) / path


@contextmanager
def release_lock(repo):
    lock = git_path(repo, "fork-release-publish.lock")
    try:
        fd = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    except FileExistsError as e:
        raise ReleaseError("another publisher is in progress") from e
    try:
        with os.fdopen(fd, "w") as f:
            f.write(f"{os.getpid()}\n")
        yield
    finally:
        lock.unlink(missing_ok=True)


def preflight(repo, requested_sha):
    repo = Path(repo).resolve()
    git(repo, "rev-parse", "--show-toplevel")
    status = run(["git", "status", "--porcelain", "--untracked-files=all"], cwd=repo).stdout.rstrip()
    if status:
        raise ReleaseError("source or submodules are dirty:\n" + status)
    sub = run(["git", "submodule", "status", "--recursive"], cwd=repo).stdout
    if any(line.startswith(("-", "+", "U")) for line in sub.splitlines()):
        raise ReleaseError("submodules are not at recorded commits")
    head = git(repo, "rev-parse", "HEAD")
    target = git(repo, "rev-parse", f"{requested_sha or head}^{{commit}}")
    if target != head:
        raise ReleaseError("HEAD differs from requested source SHA")
    # Read the configured URL before insteadOf expansion, including in offline tests.
    if git(repo, "config", "--get", "remote.origin.url") not in ORIGINS:
        raise ReleaseError("origin is not an exact supported jinmo100/AyuGramDesktop URL")
    run(["git", "fetch", "--quiet", "origin", "+refs/heads/dev:refs/remotes/origin/dev"], cwd=repo)
    pushed = run(["git", "merge-base", "--is-ancestor", target, "refs/remotes/origin/dev"],
                 cwd=repo, check=False)
    if pushed.returncode:
        raise ReleaseError("source SHA is not contained in pushed origin/dev")
    return head


def base_version(repo):
    text = (Path(repo) / "Telegram/build/version").read_text(encoding="utf-8")
    m = re.search(r"^AppVersionStrSmall\s+(\d+\.\d+\.\d+)\s*$", text, re.M)
    if not m:
        raise ReleaseError("cannot determine AppVersionStrSmall")
    return m.group(1)


def require_e_output(path):
    if os.name == "nt" and Path(path).resolve().drive.upper() != "E:":
        raise ReleaseError("all release output must be on Windows E:")


def read_cache(path):
    values = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        m = re.match(r"([^:#=]+):[^=]+=(.*)$", line)
        if m and m[1] in CACHE_KEYS:
            values[m[1]] = m[2]
    return values


def load_manifest(path, source_sha, repo):
    repo = Path(repo).resolve()
    path = Path(path).resolve()
    require_e_output(path)
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        if data["source_sha"] != source_sha or any(data[k] is not True for k in
                ("build_succeeded", "layout_succeeded", "process_checked")):
            raise ReleaseError("build proof does not prove this source and all gates")
        start, finish = data["started_at"], data["finished_at"]
        build_start, build_finish = data["build_started_at"], data["build_finished_at"]
        now = time.time()
        if not (0 < start <= build_start <= build_finish <= finish <= now + 5
                and 0 <= now - finish < 86400 and finish - start < 86400):
            raise ReleaseError("build proof timestamps are stale or inconsistent")
        artifact = Path(data["artifact"]).resolve()
        if artifact != (repo / "out/Debug/AyuGram.exe").resolve():
            raise ReleaseError("artifact must be this checkout's out/Debug/AyuGram.exe")
        require_e_output(artifact)
        validation = data["validation_config"]
        compiler_files = list((repo / "out/CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
        if len(compiler_files) != 1:
            raise ReleaseError("configured compiler evidence is missing or ambiguous")
        compiler = re.search(r'set\(CMAKE_CXX_COMPILER "([^"\n]+)"\)',
                             compiler_files[0].read_text(encoding="utf-8"))
        if (not compiler or not re.fullmatch(r"14\.\d+\.\d+", validation["msvc_version"])
                or f'/MSVC/{validation["msvc_version"]}/bin/Hostx64/x64/cl.exe'.lower()
                not in compiler[1].replace("\\", "/").lower()
                or not re.fullmatch(r"\d+\.\d+\.\d+", validation["qt_version"])
                or validation["tbuild_root"].replace("\\", "/").rstrip("/").lower() != "e:/tbuild"):
            raise ReleaseError("proof does not match configured MSVC/layout validation")
        cache = read_cache(repo / "out/CMakeCache.txt")
        if (cache != data["cache"]
                or cache.get("DESKTOP_APP_DISABLE_AUTOUPDATE") != "ON"
                or cache.get("TDESKTOP_FORK_RELEASES_URL") != RELEASES_URL
                or Path(cache.get("CMAKE_HOME_DIRECTORY", "")).resolve() != repo
                or cache.get("CMAKE_GENERATOR_PLATFORM") != "x64"
                or not cache.get("CMAKE_GENERATOR", "").startswith("Visual Studio ")
                or "Debug" not in cache.get("CMAKE_CONFIGURATION_TYPES", "").split(";")
                or not cache.get("CMAKE_GENERATOR_TOOLSET")
                or cache.get("CMAKE_MSVC_RUNTIME_LIBRARY") != "MultiThreaded$<$<CONFIG:Debug>:Debug>"):
            raise ReleaseError("actual CMake cache differs from required build configuration")
        if sha256(artifact) != data["artifact_sha256"]:
            raise ReleaseError("artifact hash differs from build proof")
    except (OSError, ValueError, KeyError, TypeError) as e:
        raise ReleaseError("invalid or incomplete build proof") from e
    return data, artifact


def gh(args, *, input_data=None, check=True):
    return run(["gh", "api", *args], input_text=input_data, check=check)


def api_json(args, *, payload=None, missing=False):
    if payload is not None:
        args = [*args, "--input", "-"]
    p = gh(args, input_data=json.dumps(payload) if payload is not None else None, check=False)
    if p.returncode:
        if missing and re.search(r"\bHTTP 404\b", p.stderr):
            return None
        raise ReleaseError("GitHub API request failed; refusing publication")
    try:
        return json.loads(p.stdout)
    except ValueError as e:
        raise ReleaseError("GitHub returned invalid JSON") from e


def remote_release(tag):
    pages = api_json([f"repos/{REPO}/releases?per_page=100", "--paginate", "--slurp"])
    matches = [item for page in pages for item in page if item.get("tag_name") == tag]
    if len(matches) > 1:
        raise ReleaseError("multiple releases share this tag")
    return matches[0] if matches else None


def tag_target(tag):
    ref = api_json([f"repos/{REPO}/git/ref/tags/{tag}"], missing=True)
    if ref is None:
        return None
    obj = ref["object"]
    for _ in range(8):
        if obj["type"] == "commit":
            return obj["sha"]
        if obj["type"] != "tag":
            break
        obj = api_json([f"repos/{REPO}/git/tags/{obj['sha']}"])["object"]
    raise ReleaseError("tag does not peel to a commit")


def release_tag(repo, explicit):
    version = base_version(repo)
    if explicit:
        m = TAG_RE.fullmatch(explicit)
        if not m or m[1] != version:
            raise ReleaseError("tag must match the base version and have a positive serial")
        return explicit
    names = []
    for endpoint, key in (("releases", "tag_name"), ("tags", "name")):
        pages = api_json([f"repos/{REPO}/{endpoint}?per_page=100", "--paginate", "--slurp"])
        names.extend(item[key] for page in pages for item in page)
    nums = [int(m[2]) for name in names
            if (m := TAG_RE.fullmatch(name)) and m[1] == version]
    return f"v{version}-jinmo.{max(nums, default=0) + 1}"


def make_assets(repo, tag, manifest, artifact, staging):
    require_e_output(staging)
    license_path = Path(repo) / "LICENSE"
    if not license_path.is_file():
        raise ReleaseError("LICENSE is mandatory")
    root = Path(tempfile.mkdtemp(prefix="ayu-release-", dir=staging))
    try:
        prefix = f"AyuGram-Jinmo-{tag}/"
        readme = (f"AyuGram-Jinmo {tag}: Windows x64 Debug test build\n"
                  f"Source: https://github.com/{REPO}/tree/{manifest['source_sha']}\n"
                  f"Manual releases: {RELEASES_URL}\nAuto updates disabled.\n"
                  "Keep the TelegramForcePortable directory next to AyuGram.exe.\n"
                  + "\n".join("Validated: " + x for x in GATES) + "\n"
                  + "\n".join("Limitation: " + x for x in LIMITATIONS) + "\n")
        contents = {"AyuGram.exe": artifact.read_bytes(), "LICENSE": license_path.read_bytes(),
                    "README.txt": readme.encode(), "TelegramForcePortable/.keep": b""}
        zip_path = root / f"AyuGram-Jinmo-{tag}.zip"
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
            for name, content in sorted(contents.items()):
                entry = zipfile.ZipInfo(prefix + name, (1980, 1, 1, 0, 0, 0))
                entry.create_system = 3
                entry.external_attr = (0o40755 if name.endswith("/") else 0o100644) << 16
                entry.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(entry, content)
        digest = sha256(zip_path)
        sums = root / "SHA256SUMS"
        sums.write_bytes(f"{digest}  {zip_path.name}\n".encode())
        info = root / "build-info.json"
        info.write_bytes((json.dumps({"tag": tag, "source_sha": manifest["source_sha"],
            "source_url": f"https://github.com/{REPO}/tree/{manifest['source_sha']}",
            "artifact_sha256": manifest["artifact_sha256"], "zip_sha256": digest,
            "configuration": "Debug", "auto_updates": False, "cache": manifest["cache"],
            "validation_config": manifest["validation_config"],
            "validated": GATES, "limitations": LIMITATIONS,
            "releases_url": RELEASES_URL}, indent=2, sort_keys=True) + "\n").encode())
        return root, [zip_path, sums, info]
    except Exception:
        shutil.rmtree(root)
        raise


def owner_body(tag, source, assets):
    marker = json.dumps({"source": source, "tag": tag,
                         "assets": {a.name: sha256(a) for a in assets}}, sort_keys=True)
    return (f"Debug portable test build for {tag}.\nSource: `{source}`\n"
            f"Manual download; auto updates disabled.\n\n"
            + "\n".join(LIMITATIONS) + f"\n\n<!-- fork-release-v1 {marker} -->")


def verify_draft(release, source, body):
    if (not release or release.get("draft") is not True
            or release.get("target_commitish") != source or release.get("body") != body):
        raise ReleaseError("draft is not owned by this exact source/tag/checksum set")


def verify_assets(release, assets, *, complete):
    remote = release.get("assets", [])
    expected = {a.name: f"sha256:{sha256(a)}" for a in assets}
    names = [a.get("name") for a in remote]
    if len(set(names)) != len(names) or set(names) - expected.keys():
        raise ReleaseError("draft contains extra or duplicate assets")
    if complete and set(names) != expected.keys():
        raise ReleaseError("draft is missing assets")
    if any(a.get("digest") != expected[a["name"]] for a in remote):
        raise ReleaseError("draft asset digest mismatch")
    return set(names)


def publish(args, repo, source):
    manifest, artifact = load_manifest(args.build_manifest, source, repo)
    tag = release_tag(repo, args.tag)
    existing = remote_release(tag)
    target = tag_target(tag)
    if existing and existing.get("draft") is not True:
        raise ReleaseError("published release already exists")
    if target is not None and (not existing or not args.resume or target != source):
        raise ReleaseError("existing tag cannot be used for a new release")
    if args.resume and not existing:
        raise ReleaseError("resume requires an existing owned draft")
    if existing and not args.resume:
        raise ReleaseError("draft exists; retry with --resume and --tag")
    root, assets = make_assets(repo, tag, manifest, artifact, Path(args.build_manifest).resolve().parent)
    try:
        body = owner_body(tag, source, assets)
        if existing:
            verify_draft(existing, source, body)
        else:
            existing = api_json([f"repos/{REPO}/releases", "--method", "POST"], payload={
                "tag_name": tag, "name": f"AyuGram-Jinmo {tag}", "body": body,
                "draft": True, "prerelease": True, "target_commitish": source})
            verify_draft(existing, source, body)
        present = verify_assets(existing, assets, complete=False)
        for asset in assets:
            if asset.name not in present:
                run(["gh", "release", "upload", tag, str(asset), "--repo", REPO])
        check = remote_release(tag)
        verify_draft(check, source, body)
        verify_assets(check, assets, complete=True)
        if tag_target(tag) is None:
            api_json([f"repos/{REPO}/git/refs", "--method", "POST"],
                     payload={"ref": f"refs/tags/{tag}", "sha": source})
        if tag_target(tag) != source:
            raise ReleaseError("tag does not peel to the exact source commit")
        preflight(repo, source)
        load_manifest(args.build_manifest, source, repo)
        api_json([f"repos/{REPO}/releases/{check['id']}", "--method", "PATCH"],
                 payload={"draft": False, "prerelease": True})
        print(tag)
    finally:
        shutil.rmtree(root, ignore_errors=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo", default=".")
    ap.add_argument("--source-sha")
    ap.add_argument("--build-manifest")
    ap.add_argument("--tag")
    ap.add_argument("--resume", action="store_true")
    ap.add_argument("--preflight-only", action="store_true")
    ap.add_argument("--verify-proof-only", action="store_true")
    args = ap.parse_args(argv)
    if args.resume and not args.tag:
        ap.error("--resume requires --tag")
    if not args.preflight_only and not args.build_manifest:
        ap.error("--build-manifest is required")
    repo = Path(args.repo).resolve()
    try:
        with release_lock(repo):
            source = preflight(repo, args.source_sha)
            if args.preflight_only:
                print(source)
            elif args.verify_proof_only:
                load_manifest(args.build_manifest, source, repo)
            else:
                publish(args, repo, source)
        return 0
    except (ReleaseError, OSError, KeyError, TypeError, ValueError) as e:
        print(f"release refused: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
