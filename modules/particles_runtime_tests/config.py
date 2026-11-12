def can_build(env, platform):
    return env["tests"] and not env["disable_3d"]


def configure(env):
    pass


def is_enabled():
    return False
