import os
import subprocess

Import("env")


def git(*args):
    return subprocess.check_output(["git", *args], stderr=subprocess.DEVNULL).decode().strip()


def identify_version():
    try:
        try:
            return git("describe", "--tags", "--exact-match")
        except subprocess.CalledProcessError:
            pass  # Not on an exact tag, continue to dev version

        tag = git("describe", "--tags", "--abbrev=0")
        sha = git("rev-parse", "--short=7", "HEAD")

        # GITHUB_REF is set automatically by GitHub Actions runners
        # "refs/pull/42/merge" → PR build
        # anything else        → branch/local build
        github_ref = os.environ.get("GITHUB_REF", "")

        if github_ref.startswith("refs/pull/"):
            pr_num = github_ref.split("/")[2]
            return f"{tag}dev-{sha} (#{pr_num})"

        branch = git("rev-parse", "--abbrev-ref", "HEAD")
        return f"{tag}dev-{sha} ({branch})"

    except Exception as e:
        print(f"{os.path.basename(__file__)}: failed to identify version: {e}")
        return "unknown"


env.Append(CPPDEFINES=[("BUILD_VERSION", f'\\"{identify_version()}\\"')])
