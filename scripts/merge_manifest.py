import glob
import os, sys

def resolve_main_image_name():
    if len(sys.argv) >= 3:
        return sys.argv[2]

    image_name = os.environ.get("IMAGE_NAME")
    if image_name:
        return image_name

    repository = os.environ.get("GITHUB_REPOSITORY", "")
    if "/" in repository:
        owner, _ = repository.split("/", 1)
        return f"ghcr.io/{owner.lower()}/subconverter"

    raise SystemExit("IMAGE_NAME env or argv[2] image name is required")


MAIN_IMAGE_NAME = resolve_main_image_name()
TARGET_TAG = "latest" if len(sys.argv) < 2 else sys.argv[1]

args=["docker manifest create {}:{}".format(MAIN_IMAGE_NAME, TARGET_TAG)]
for i in glob.glob("/tmp/images/*/*.txt"):
    with open(i, "r") as file:
        args += " --amend {}@{}".format(MAIN_IMAGE_NAME, file.readline().strip())
cmd_create="".join(args)
cmd_push="docker manifest push {}:{}".format(MAIN_IMAGE_NAME, TARGET_TAG)
os.system(cmd_create)
os.system(cmd_push)
